#include "LocalAI/StateTree/AIRECompanionStateTree.h"

#include "Core/AIRECompanionAIController.h"
#include "Core/AIRECompanionCharacter.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Policy/AIRECompanionLocalBehaviorPolicyComponent.h"
#include "Support/AIRECompanionSupportComponent.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Threat/AIRECompanionThreatComponent.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "Work/AIRECompanionCraftingWorkRequest.h"
#include "Work/AIRECompanionHarvestWorkRequest.h"
#include "AIREGameplayInventorySubsystem.h"
#include "AIRESharedStorageActor.h"
#include "AI_RECraftingTypes.h"
#include "AI_REHarvestableResourceActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "AI_REItemActor.h"
#include "AI_REItemDataAsset.h"
#include "AI_REItemSubsystem.h"
#include "AI_REWorkBenchBase.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "Engine/DataTable.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionStateTree, Log, All);

namespace
{
	constexpr float CombatApproachMargin = 50.0f;
	constexpr float CombatApproachAcceptanceRadius = 25.0f;
	constexpr float CombatRangeExitSlack = 25.0f;
	constexpr float CombatMovementRetryInterval = 0.5f;
	constexpr float CombatActivationRetryInterval = 0.1f;
	constexpr float SupportApproachMargin = 50.0f;
	constexpr float SupportApproachAcceptanceRadius = 25.0f;
	constexpr float SupportMovementRetryInterval = 0.5f;
	constexpr float SupportActivationRetryInterval = 0.1f;
	constexpr float WorkApproachAcceptanceRadius = 25.0f;
	constexpr float WorkMovementRetryInterval = 0.5f;
	constexpr float WorkActivationRetryInterval = 0.1f;

	FVector CalculateCombatApproachLocation(
		const APawn& CompanionPawn,
		const AActor& TargetActor,
		const float AttackRange)
	{
		FVector TargetToCompanion =
			CompanionPawn.GetActorLocation() - TargetActor.GetActorLocation();
		TargetToCompanion.Z = 0.0f;
		if (!TargetToCompanion.Normalize())
		{
			TargetToCompanion = -TargetActor.GetActorForwardVector().GetSafeNormal2D();
		}

		const float DesiredSurfaceDistance = FMath::Max(
			0.0f,
			AttackRange - CombatApproachMargin);
		const float DesiredCenterDistance =
			CompanionPawn.GetSimpleCollisionRadius()
			+ TargetActor.GetSimpleCollisionRadius()
			+ DesiredSurfaceDistance;

		return TargetActor.GetActorLocation()
			+ TargetToCompanion * DesiredCenterDistance;
	}

	FVector CalculateSupportApproachLocation(
		const APawn& CompanionPawn,
		const AActor& TargetActor,
		const float SupportRange)
	{
		FVector TargetToCompanion =
			CompanionPawn.GetActorLocation()
			- TargetActor.GetActorLocation();
		TargetToCompanion.Z = 0.0f;
		if (!TargetToCompanion.Normalize())
		{
			TargetToCompanion =
				-TargetActor.GetActorForwardVector().GetSafeNormal2D();
		}

		const float DesiredSurfaceDistance = FMath::Max(
			0.0f,
			SupportRange - SupportApproachMargin);
		const float DesiredCenterDistance =
			CompanionPawn.GetSimpleCollisionRadius()
			+ TargetActor.GetSimpleCollisionRadius()
			+ DesiredSurfaceDistance;
		return TargetActor.GetActorLocation()
			+ TargetToCompanion * DesiredCenterDistance;
	}

	FVector CalculateWorkbenchApproachLocation(
		const APawn& CompanionPawn,
		const AActor& Workbench,
		const float InteractionGap)
	{
		FVector FrontDirection = Workbench.GetActorForwardVector();
		FrontDirection.Z = 0.0f;
		if (!FrontDirection.Normalize())
		{
			FrontDirection = FVector::ForwardVector;
		}
		FVector BoundsOrigin;
		FVector BoundsExtent;
		Workbench.GetActorBounds(false, BoundsOrigin, BoundsExtent);
		const float BoundsDistanceAlongForward =
			FMath::Abs(FrontDirection.X) * BoundsExtent.X
			+ FMath::Abs(FrontDirection.Y) * BoundsExtent.Y;

		return BoundsOrigin
			+ FrontDirection
				* (CompanionPawn.GetSimpleCollisionRadius()
					+ BoundsDistanceAlongForward
					+ FMath::Max(0.0f, InteractionGap));
	}

	bool IsWithinSurfaceRange(
		const APawn& CompanionPawn,
		const AActor& TargetActor,
		const float SurfaceRange)
	{
		const float HorizontalDistance = FVector::Dist2D(
			CompanionPawn.GetActorLocation(),
			TargetActor.GetActorLocation());
		const float EffectiveDistance = FMath::Max(
			0.0f,
			HorizontalDistance
				- CompanionPawn.GetSimpleCollisionRadius()
				- TargetActor.GetSimpleCollisionRadius());
		return EffectiveDistance <= SurfaceRange;
	}

	void FaceWorkTarget(APawn& CompanionPawn, const AActor& TargetActor)
	{
		FVector TargetDirection =
			TargetActor.GetActorLocation() - CompanionPawn.GetActorLocation();
		TargetDirection.Z = 0.0f;
		if (!TargetDirection.IsNearlyZero())
		{
			CompanionPawn.SetActorRotation(
				FRotator(0.0f, TargetDirection.Rotation().Yaw, 0.0f));
		}
	}

	const FAI_RECraftingRecipe* ResolveCraftingRecipe(
		const AActor* TargetActor,
		const UDataTable* RecipeTable,
		const FName RecipeRowId)
	{
		const AAI_REWorkBenchBase* Workbench =
			Cast<AAI_REWorkBenchBase>(TargetActor);
		if (!FAIRECompanionCraftingWorkRequest::IsValidRequestInputs(
				Workbench,
				RecipeTable,
				RecipeRowId))
		{
			return nullptr;
		}

		return RecipeTable->FindRow<FAI_RECraftingRecipe>(
			RecipeRowId,
			TEXT("AIRECompanionStateTreeWork"),
			false);
	}

	bool BuildCraftWorkRequest(
		AAIRECompanionCharacter& CompanionCharacter,
		UAIRECompanionInventoryComponent& InventoryComponent,
		const FGuid& WorkOrderId,
		const FAI_RECraftingRecipe& Recipe,
		const bool bCanWorldDrop,
		FAIREMakoCraftWorkRequest& OutRequest)
	{
		FAIREInventoryContainerSnapshot MakoSnapshot;
		if (!InventoryComponent.GetInventorySnapshot(MakoSnapshot))
		{
			return false;
		}

		const UWorld* World = CompanionCharacter.GetWorld();
		const UGameInstance* GameInstance =
			IsValid(World) ? World->GetGameInstance() : nullptr;
		UAIREGameplayInventorySubsystem* GameplayInventory =
			IsValid(GameInstance)
				? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
				: nullptr;
		FAIREInventoryContainerSnapshot StorageSnapshot;
		if (!IsValid(GameplayInventory)
			|| !GameplayInventory->GetContainerSnapshot(
				UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
				StorageSnapshot)
			|| MakoSnapshot.SessionId != StorageSnapshot.SessionId)
		{
			return false;
		}

		OutRequest = FAIREMakoCraftWorkRequest();
		OutRequest.SessionId = MakoSnapshot.SessionId;
		OutRequest.WorkOrderId = WorkOrderId;
		OutRequest.ExpectedMakoRevision = MakoSnapshot.Revision;
		OutRequest.ExpectedStorageRevision = StorageSnapshot.Revision;
		OutRequest.Result.ItemId = Recipe.ResultItemId;
		OutRequest.Result.Count = Recipe.ResultAmount;
		OutRequest.bCanWorldDrop = bCanWorldDrop;
		OutRequest.Ingredients.Reserve(Recipe.Ingredients.Num());
		for (const FAI_RECraftingIngredient& Ingredient : Recipe.Ingredients)
		{
			FAIREInventoryItemQuantity& Quantity =
				OutRequest.Ingredients.AddDefaulted_GetRef();
			Quantity.ItemId = Ingredient.ItemId;
			Quantity.Count = Ingredient.Amount;
		}
		return true;
	}

	UAI_REItemDataAsset* ResolveItemAsset(
		const UObject& WorldContext,
		const FName ItemId)
	{
		const UWorld* World = WorldContext.GetWorld();
		const UGameInstance* GameInstance =
			IsValid(World) ? World->GetGameInstance() : nullptr;
		UAI_REItemSubsystem* ItemSubsystem =
			IsValid(GameInstance)
				? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
				: nullptr;
		return IsValid(ItemSubsystem)
			? ItemSubsystem->GetItemDataAsset(ItemId)
			: nullptr;
	}

	const FAIRECompanionStorageRule* FindStorageRule(
		const UAIRECompanionConfigDataAsset& CompanionConfig,
		const FName ItemId)
	{
		return CompanionConfig.StorageRules.FindByPredicate(
			[ItemId](const FAIRECompanionStorageRule& Rule)
			{
				return IsValid(Rule.ItemDefinition)
					&& Rule.ItemDefinition->ItemId == ItemId;
			});
	}

	int64 GetMakoCarriedItemCount(
		const FAIREInventoryContainerSnapshot& MakoSnapshot,
		const FName ItemId)
	{
		int64 TotalCount = 0;
		for (const FAIREInventoryItemStackSnapshot& Stack
			: MakoSnapshot.ItemStacks)
		{
			if (Stack.ItemId == ItemId)
			{
				TotalCount += Stack.Count;
			}
		}
		if (MakoSnapshot.Equipment.EquippedItemId == ItemId)
		{
			++TotalCount;
		}
		return TotalCount;
	}

	const FAIREInventoryItemStackSnapshot* FindStorageTransferStack(
		const FAIREInventoryContainerSnapshot& SourceSnapshot,
		const FName ItemId)
	{
		return SourceSnapshot.ItemStacks.FindByPredicate(
			[ItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == ItemId && Stack.Count > 0;
			});
	}

	bool BuildStorageTransferRequest(
		UAIRECompanionInventoryComponent& InventoryComponent,
		AAIRESharedStorageActor& StorageActor,
		const UAIRECompanionConfigDataAsset& CompanionConfig,
		const FAIRECompanionWorkOrderSnapshot& WorkOrderSnapshot,
		FAIREInventoryTransferRequest& OutRequest)
	{
		OutRequest = FAIREInventoryTransferRequest();
		const FAIRECompanionStorageTransferPayload& Payload =
			WorkOrderSnapshot.StorageTransfer;
		if (!Payload.RequestSessionId.IsValid()
			|| Payload.ItemId.IsNone()
			|| Payload.Count <= 0)
		{
			return false;
		}

		const FAIRECompanionStorageRule* Rule =
			FindStorageRule(CompanionConfig, Payload.ItemId);
		if (Rule == nullptr
			|| Rule->MinimumCarryCount < 0
			|| Rule->MaximumCarryCount < Rule->MinimumCarryCount)
		{
			return false;
		}

		FAIREInventoryContainerSnapshot MakoSnapshot;
		FAIREInventoryContainerSnapshot StorageSnapshot;
		if (!InventoryComponent.GetInventorySnapshot(MakoSnapshot)
			|| !StorageActor.GetStorageSnapshot(StorageSnapshot)
			|| MakoSnapshot.SessionId != Payload.RequestSessionId
			|| StorageSnapshot.SessionId != Payload.RequestSessionId)
		{
			return false;
		}

		const int64 CarriedCount = GetMakoCarriedItemCount(
			MakoSnapshot,
			Payload.ItemId);
		const FAIREInventoryContainerSnapshot* SourceSnapshot = nullptr;
		int64 RequiredCount = 0;
		switch (Payload.Direction)
		{
		case EAIRECompanionStorageTransferDirection::
			DepositMakoToStorage:
			if (CarriedCount <= Rule->MaximumCarryCount
				|| MakoSnapshot.Equipment.PendingItemId == Payload.ItemId)
			{
				return false;
			}
			SourceSnapshot = &MakoSnapshot;
			RequiredCount =
				CarriedCount - Rule->MaximumCarryCount;
			OutRequest.SourceContainerId =
				UAIREGameplayInventorySubsystem::GetMakoContainerId();
			OutRequest.DestinationContainerId =
				UAIREGameplayInventorySubsystem::
					GetSharedStorageContainerId();
			OutRequest.ExpectedSourceRevision = MakoSnapshot.Revision;
			OutRequest.ExpectedDestinationRevision =
				StorageSnapshot.Revision;
			break;

		case EAIRECompanionStorageTransferDirection::
			WithdrawStorageToMako:
			if (CarriedCount >= Rule->MinimumCarryCount
				|| MakoSnapshot.Equipment.PendingItemId == Payload.ItemId)
			{
				return false;
			}
			SourceSnapshot = &StorageSnapshot;
			RequiredCount =
				Rule->MinimumCarryCount - CarriedCount;
			OutRequest.SourceContainerId =
				UAIREGameplayInventorySubsystem::
					GetSharedStorageContainerId();
			OutRequest.DestinationContainerId =
				UAIREGameplayInventorySubsystem::GetMakoContainerId();
			OutRequest.ExpectedSourceRevision =
				StorageSnapshot.Revision;
			OutRequest.ExpectedDestinationRevision = MakoSnapshot.Revision;
			break;

		default:
			return false;
		}

		const FAIREInventoryItemStackSnapshot* SourceStack =
			SourceSnapshot != nullptr
				? FindStorageTransferStack(
					*SourceSnapshot,
					Payload.ItemId)
				: nullptr;
		if (SourceStack == nullptr)
		{
			return false;
		}

		const int64 TransferCount = FMath::Min<int64>(
			Payload.Count,
			FMath::Min<int64>(RequiredCount, SourceStack->Count));
		if (TransferCount <= 0 || TransferCount > MAX_int32)
		{
			return false;
		}

		OutRequest.SessionId = Payload.RequestSessionId;
		OutRequest.MutationId = WorkOrderSnapshot.WorkOrderId;
		OutRequest.SourceSlotIndex = SourceStack->SlotIndex;
		OutRequest.Count = static_cast<int32>(TransferCount);
		return true;
	}

	AAI_REItemActor* SpawnDeferredWorkResult(
		AAIRECompanionCharacter& CompanionCharacter,
		AActor& WorkTarget,
		const TSubclassOf<AAI_REItemActor> ItemActorClass,
		UAI_REItemDataAsset& ItemAsset,
		const int32 Count,
		FTransform& OutSpawnTransform)
	{
		UWorld* World = CompanionCharacter.GetWorld();
		if (!IsValid(World) || !ItemActorClass || Count <= 0)
		{
			return nullptr;
		}

		const FVector SpawnLocation =
			WorkTarget.GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
		OutSpawnTransform = FTransform(FRotator::ZeroRotator, SpawnLocation);
		AAI_REItemActor* DeferredItem =
			World->SpawnActorDeferred<AAI_REItemActor>(
				ItemActorClass,
				OutSpawnTransform,
				&CompanionCharacter,
				&CompanionCharacter,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (IsValid(DeferredItem))
		{
			DeferredItem->ItemAsset = &ItemAsset;
			DeferredItem->ItemCount = Count;
		}
		return DeferredItem;
	}

	enum class EAIRECompanionBehaviorInput : uint16
	{
		HasPlayer = 1 << 0,
		ShouldFollow = 1 << 1,
		ShouldReturn = 1 << 2,
		Disabled = 1 << 3,
		Survival = 1 << 4,
		Combat = 1 << 5,
		DirectCommand = 1 << 6,
		Work = 1 << 7,
		Support = 1 << 8
	};
	ENUM_CLASS_FLAGS(EAIRECompanionBehaviorInput);

	void AddBehaviorInput(
		EAIRECompanionBehaviorInput& InputMask,
		const EAIRECompanionBehaviorInput Input,
		const bool bIsActive)
	{
		if (bIsActive)
		{
			EnumAddFlags(InputMask, Input);
		}
	}

	FString GetBehaviorStateName(const EAIRECompanionBehaviorState BehaviorState)
	{
		const UEnum* BehaviorStateEnum = StaticEnum<EAIRECompanionBehaviorState>();
		return IsValid(BehaviorStateEnum)
			? BehaviorStateEnum->GetNameStringByValue(static_cast<int64>(BehaviorState))
			: TEXT("Unknown");
	}
}

void FAIRECompanionContextEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	UpdateContext(Context);
}

void FAIRECompanionContextEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.PlayerPawn = nullptr;
	InstanceData.ThreatTarget = nullptr;
	InstanceData.SupportTarget = nullptr;
	InstanceData.EquipmentComponent = nullptr;
	InstanceData.InventoryComponent = nullptr;
	InstanceData.SupportComponent = nullptr;
	InstanceData.WorkOrderComponent = nullptr;
	InstanceData.WorkTarget = nullptr;
	InstanceData.WorkRecipeTable = nullptr;
	InstanceData.WorkOrderId.Invalidate();
	InstanceData.WorkRecipeRowId = NAME_None;
	InstanceData.WorkType = EAIRECompanionWorkOrderType::None;
	InstanceData.WorkOrderState = EAIRECompanionWorkOrderState::None;
	InstanceData.AbilitySystemComponent = nullptr;
	InstanceData.DistanceToPlayer = 0.0f;
	InstanceData.MovementSpeed = 0.0f;
	InstanceData.bIsRunning = false;
	InstanceData.FollowStopDistance = 0.0f;
	InstanceData.ReturnStartDistance = 0.0f;
	InstanceData.CombatDistance = 0.0f;
	InstanceData.CombatCooldown = 0.0f;
	InstanceData.bHasPlayer = false;
	InstanceData.bShouldFollow = false;
	InstanceData.bShouldReturn = false;
	InstanceData.bIsDisabledRequested = false;
	InstanceData.bIsSurvivalRequested = false;
	InstanceData.bIsCombatRequested = false;
	InstanceData.bIsSupportRequested = false;
	InstanceData.bIsDirectCommandRequested = false;
	InstanceData.bIsWorkRequested = false;
	InstanceData.bBehaviorSelectionChanged = false;
	InstanceData.PreviousBehaviorInputMask = 0;
	InstanceData.bHasPreviousBehaviorInput = false;
	InstanceData.PreviousPlayerPawn.Reset();
	InstanceData.PreviousThreatTarget.Reset();
	InstanceData.PreviousSupportTarget.Reset();
	InstanceData.PreviousWorkOrderId.Invalidate();
}

void FAIRECompanionContextEvaluator::Tick(FStateTreeExecutionContext& Context, const float) const
{
	UpdateContext(Context);
}

void FAIRECompanionContextEvaluator::UpdateContext(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.PlayerPawn = nullptr;
	InstanceData.ThreatTarget = nullptr;
	InstanceData.SupportTarget = nullptr;
	InstanceData.EquipmentComponent = nullptr;
	InstanceData.InventoryComponent = nullptr;
	InstanceData.SupportComponent = nullptr;
	InstanceData.WorkOrderComponent = nullptr;
	InstanceData.WorkTarget = nullptr;
	InstanceData.WorkRecipeTable = nullptr;
	InstanceData.WorkOrderId.Invalidate();
	InstanceData.WorkRecipeRowId = NAME_None;
	InstanceData.WorkType = EAIRECompanionWorkOrderType::None;
	InstanceData.WorkOrderState = EAIRECompanionWorkOrderState::None;
	InstanceData.AbilitySystemComponent = nullptr;
	InstanceData.DistanceToPlayer = 0.0f;
	InstanceData.MovementSpeed = 0.0f;
	InstanceData.FollowStopDistance = 0.0f;
	InstanceData.ReturnStartDistance = 0.0f;
	InstanceData.CombatDistance = 0.0f;
	InstanceData.CombatCooldown = 0.0f;
	InstanceData.bHasPlayer = false;
	InstanceData.bShouldFollow = false;
	InstanceData.bShouldReturn = false;
	InstanceData.bIsDisabledRequested = false;
	InstanceData.bIsSurvivalRequested = false;
	InstanceData.bIsCombatRequested = false;
	InstanceData.bIsSupportRequested = false;
	InstanceData.bIsDirectCommandRequested = false;
	InstanceData.bIsWorkRequested = false;

	if (IsValid(InstanceData.CompanionController))
	{
		InstanceData.bIsDisabledRequested = InstanceData.CompanionController->IsTestBehaviorRequestActive(
			EAIRECompanionTestBehaviorRequest::Disabled);
		InstanceData.bIsSurvivalRequested = InstanceData.CompanionController->IsTestBehaviorRequestActive(
			EAIRECompanionTestBehaviorRequest::Survival);
		InstanceData.bIsDirectCommandRequested = InstanceData.CompanionController->IsTestBehaviorRequestActive(
			EAIRECompanionTestBehaviorRequest::DirectCommand);
		const UAIRECompanionThreatComponent* ThreatComponent = InstanceData.CompanionController->GetThreatComponent();
		if (IsValid(ThreatComponent))
		{
			InstanceData.ThreatTarget = ThreatComponent->GetSelectedThreatTarget();
			InstanceData.bIsCombatRequested = ThreatComponent->IsCombatRequested();
		}
	}

	if (IsValid(InstanceData.CompanionCharacter))
	{
		InstanceData.EquipmentComponent =
			InstanceData.CompanionCharacter->GetEquipmentComponent();
		InstanceData.InventoryComponent =
			InstanceData.CompanionCharacter->GetInventoryComponent();
		InstanceData.SupportComponent =
			InstanceData.CompanionCharacter->GetSupportComponent();
		InstanceData.WorkOrderComponent =
			InstanceData.CompanionCharacter->GetWorkOrderComponent();
		InstanceData.AbilitySystemComponent =
			InstanceData.CompanionCharacter->GetAbilitySystemComponent();
		if (IsValid(InstanceData.WorkOrderComponent))
		{
			const FAIRECompanionWorkOrderSnapshot WorkSnapshot =
				InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
			InstanceData.WorkTarget = WorkSnapshot.TargetActor.Get();
			InstanceData.WorkRecipeTable = WorkSnapshot.RecipeTable.Get();
			InstanceData.WorkOrderId = WorkSnapshot.WorkOrderId;
			InstanceData.WorkRecipeRowId = WorkSnapshot.RecipeRowId;
			InstanceData.WorkType = WorkSnapshot.WorkType;
			InstanceData.WorkOrderState = WorkSnapshot.State;
			InstanceData.bIsWorkRequested =
				InstanceData.WorkOrderComponent->HasActiveWorkOrder();
		}
		if (IsValid(InstanceData.SupportComponent))
		{
			InstanceData.SupportTarget =
				InstanceData.SupportComponent->GetSupportTarget();
			InstanceData.bIsSupportRequested =
				InstanceData.SupportComponent->IsSupportRequested();
		}
		InstanceData.bIsDisabledRequested = InstanceData.bIsDisabledRequested
			|| InstanceData.CompanionCharacter->IsAbilitySystemDisabled();

		if (InstanceData.bIsCombatRequested
			&& InstanceData.bIsSupportRequested)
		{
			const UAIRECompanionLocalBehaviorPolicyComponent* PolicyComponent =
				InstanceData.CompanionCharacter
					->GetLocalBehaviorPolicyComponent();
			if (IsValid(PolicyComponent)
				&& PolicyComponent->GetLocalBehaviorPolicy().RolePreference
					== EAIRECompanionRolePreference::SupportPriority)
			{
				InstanceData.bIsCombatRequested = false;
			}
		}

		const UAIRECompanionConfigDataAsset* CompanionConfig = InstanceData.CompanionCharacter->GetCompanionConfig();
		if (IsValid(CompanionConfig))
		{
			InstanceData.FollowStopDistance = CompanionConfig->FollowStopDistance;
			InstanceData.ReturnStartDistance = CompanionConfig->ReturnStartDistance;

			const UWorld* World = InstanceData.CompanionCharacter->GetWorld();
			APawn* CurrentPlayerPawn = IsValid(World) ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			if (IsValid(CurrentPlayerPawn))
			{
				InstanceData.PlayerPawn = CurrentPlayerPawn;
				InstanceData.DistanceToPlayer = FVector::Distance(
					InstanceData.CompanionCharacter->GetActorLocation(),
					CurrentPlayerPawn->GetActorLocation());
				InstanceData.bHasPlayer = true;
				InstanceData.bShouldFollow = InstanceData.DistanceToPlayer > InstanceData.FollowStopDistance;
				InstanceData.bShouldReturn = InstanceData.DistanceToPlayer > InstanceData.ReturnStartDistance;
			}

			if (InstanceData.bIsCombatRequested || InstanceData.bShouldReturn)
			{
				InstanceData.bIsRunning = true;
			}
			else if (InstanceData.bIsRunning)
			{
				InstanceData.bIsRunning =
					InstanceData.DistanceToPlayer > CompanionConfig->WalkResumeDistance;
			}
			else
			{
				InstanceData.bIsRunning =
					InstanceData.DistanceToPlayer >= CompanionConfig->RunStartDistance;
			}

			InstanceData.MovementSpeed = InstanceData.bIsRunning
				? CompanionConfig->MovementSpeed
				: CompanionConfig->WalkSpeed;
			if (UCharacterMovementComponent* MovementComponent =
					InstanceData.CompanionCharacter->GetCharacterMovement();
				IsValid(MovementComponent)
				&& !FMath::IsNearlyEqual(
					MovementComponent->MaxWalkSpeed,
					InstanceData.MovementSpeed))
			{
				MovementComponent->MaxWalkSpeed = InstanceData.MovementSpeed;
			}
		}
	}

	EAIRECompanionBehaviorInput BehaviorInputMask = static_cast<EAIRECompanionBehaviorInput>(0);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::HasPlayer, InstanceData.bHasPlayer);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::ShouldFollow, InstanceData.bShouldFollow);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::ShouldReturn, InstanceData.bShouldReturn);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Disabled, InstanceData.bIsDisabledRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Survival, InstanceData.bIsSurvivalRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Combat, InstanceData.bIsCombatRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Support, InstanceData.bIsSupportRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::DirectCommand, InstanceData.bIsDirectCommandRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Work, InstanceData.bIsWorkRequested);

	const uint16 CurrentBehaviorInputMask = static_cast<uint16>(BehaviorInputMask);
	const bool bPlayerPawnChanged = InstanceData.PreviousPlayerPawn.Get() != InstanceData.PlayerPawn.Get();
	const bool bThreatTargetChanged = InstanceData.PreviousThreatTarget.Get() != InstanceData.ThreatTarget.Get();
	const bool bSupportTargetChanged =
		InstanceData.PreviousSupportTarget.Get()
		!= InstanceData.SupportTarget.Get();
	const bool bWorkOrderChanged =
		InstanceData.PreviousWorkOrderId != InstanceData.WorkOrderId;
	InstanceData.bBehaviorSelectionChanged = !InstanceData.bHasPreviousBehaviorInput
		|| InstanceData.PreviousBehaviorInputMask != CurrentBehaviorInputMask
		|| bPlayerPawnChanged
		|| bThreatTargetChanged
		|| bSupportTargetChanged
		|| bWorkOrderChanged;
	InstanceData.PreviousBehaviorInputMask = CurrentBehaviorInputMask;
	InstanceData.bHasPreviousBehaviorInput = true;
	InstanceData.PreviousPlayerPawn = InstanceData.PlayerPawn;
	InstanceData.PreviousThreatTarget = InstanceData.ThreatTarget;
	InstanceData.PreviousSupportTarget = InstanceData.SupportTarget;
	InstanceData.PreviousWorkOrderId = InstanceData.WorkOrderId;
}

FAIREApplyCompanionMovementSettingsTask::FAIREApplyCompanionMovementSettingsTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIREApplyCompanionMovementSettingsTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionCharacter))
	{
		UE_LOG(LogAIRECompanionStateTree, Warning, TEXT("Cannot apply movement settings without a valid Companion Character."));
		return EStateTreeRunStatus::Failed;
	}

	UCharacterMovementComponent* MovementComponent = InstanceData.CompanionCharacter->GetCharacterMovement();
	if (!IsValid(MovementComponent))
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Warning,
			TEXT("Companion %s has no valid Character Movement Component."),
			*GetNameSafe(InstanceData.CompanionCharacter));
		return EStateTreeRunStatus::Failed;
	}

	if (!FMath::IsFinite(InstanceData.MovementSpeed) || InstanceData.MovementSpeed <= 0.0f)
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Warning,
			TEXT("Companion %s received invalid movement speed %.2f from its StateTree binding."),
			*GetNameSafe(InstanceData.CompanionCharacter),
			InstanceData.MovementSpeed);
		return EStateTreeRunStatus::Failed;
	}

	MovementComponent->MaxWalkSpeed = InstanceData.MovementSpeed;
	return EStateTreeRunStatus::Succeeded;
}

FAIRECompanionIdleTask::FAIRECompanionIdleTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionIdleTask::EnterState(
	FStateTreeExecutionContext&,
	const FStateTreeTransitionResult&) const
{
	return EStateTreeRunStatus::Running;
}

FAIRECompanionBehaviorDebugTask::FAIRECompanionBehaviorDebugTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionBehaviorDebugTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UE_LOG(
		LogAIRECompanionStateTree,
		Log,
		TEXT("Companion behavior entered. State=%s Target=%s Priority=%s ChangeType=%s"),
		*GetBehaviorStateName(InstanceData.BehaviorState),
		*GetNameSafe(InstanceData.TargetActor),
		*StaticEnum<EStateTreeTransitionPriority>()->GetNameStringByValue(static_cast<int64>(Transition.Priority)),
		*StaticEnum<EStateTreeStateChangeType>()->GetNameStringByValue(static_cast<int64>(Transition.ChangeType)));
	return EStateTreeRunStatus::Succeeded;
}

void FAIRECompanionBehaviorDebugTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UE_LOG(
		LogAIRECompanionStateTree,
		Log,
		TEXT("Companion behavior exited. State=%s Target=%s RunStatus=%s MoveCancellationBoundary=%s"),
		*GetBehaviorStateName(InstanceData.BehaviorState),
		*GetNameSafe(InstanceData.TargetActor),
		*StaticEnum<EStateTreeRunStatus>()->GetNameStringByValue(static_cast<int64>(Transition.CurrentRunStatus)),
		InstanceData.bOwnsMovementRequest ? TEXT("StateExit") : TEXT("None"));
}

FAIRECompanionExecuteWorkOrderTask::FAIRECompanionExecuteWorkOrderTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionExecuteWorkOrderTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionCharacter)
		|| !IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.WorkOrderComponent)
		|| !IsValid(InstanceData.InventoryComponent)
		|| !IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent)
		|| !InstanceData.WorkOrderId.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	FAIRECompanionWorkOrderSnapshot Snapshot =
		InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
	if (Snapshot.WorkOrderId != InstanceData.WorkOrderId
		|| Snapshot.WorkType == EAIRECompanionWorkOrderType::None
		|| !IsValid(Snapshot.TargetActor.Get()))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.ActiveWorkOrderId != Snapshot.WorkOrderId)
	{
		InstanceData.ActiveWorkOrderId = Snapshot.WorkOrderId;
		InstanceData.ElapsedCraftingTime = 0.0f;
		InstanceData.ElapsedStorageMovementTime = 0.0f;
		InstanceData.ElapsedStorageWorkTime = 0.0f;
		InstanceData.RetryTimeRemaining = 0.0f;
		InstanceData.bMoveRequested = false;
		InstanceData.ActiveStorageMontage = nullptr;
	}
	InstanceData.ActiveTarget = Snapshot.TargetActor;

	if (Snapshot.State == EAIRECompanionWorkOrderState::PausedByCombat
		&& !InstanceData.bIsCombatRequested)
	{
		if (!InstanceData.WorkOrderComponent->TryResumeAfterCombat(
				Snapshot.WorkOrderId))
		{
			return EStateTreeRunStatus::Failed;
		}
		Snapshot = InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
	}
	if (Snapshot.State == EAIRECompanionWorkOrderState::Requested
		&& !InstanceData.WorkOrderComponent->TryStartMoving(
			Snapshot.WorkOrderId))
	{
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIRECompanionExecuteWorkOrderTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionCharacter)
		|| !IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.WorkOrderComponent)
		|| !IsValid(InstanceData.InventoryComponent)
		|| !IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	FAIRECompanionWorkOrderSnapshot Snapshot =
		InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
	if (Snapshot.WorkOrderId != InstanceData.ActiveWorkOrderId)
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}
	if (Snapshot.State == EAIRECompanionWorkOrderState::Completed)
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Succeeded;
	}
	if (Snapshot.State == EAIRECompanionWorkOrderState::Cancelled
		|| Snapshot.State == EAIRECompanionWorkOrderState::Failed
		|| Snapshot.State == EAIRECompanionWorkOrderState::None)
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.bIsCombatRequested)
	{
		if (Snapshot.WorkType
			== EAIRECompanionWorkOrderType::StorageTransfer)
		{
			InstanceData.ElapsedStorageMovementTime = 0.0f;
			InstanceData.ElapsedStorageWorkTime = 0.0f;
		}
		if (Snapshot.State == EAIRECompanionWorkOrderState::Moving
			|| Snapshot.State == EAIRECompanionWorkOrderState::Working)
		{
			InstanceData.WorkOrderComponent->TryPauseForCombat(
				Snapshot.WorkOrderId);
		}
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Running;
	}
	if (Snapshot.State == EAIRECompanionWorkOrderState::PausedByCombat)
	{
		if (!InstanceData.WorkOrderComponent->TryResumeAfterCombat(
				Snapshot.WorkOrderId))
		{
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}
		Snapshot = InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
	}

	AActor* TargetActor = Snapshot.TargetActor.Get();
	APawn* CompanionPawn = InstanceData.CompanionController->GetPawn();
	if (!IsValid(TargetActor)
		|| TargetActor->IsActorBeingDestroyed()
		|| !IsValid(CompanionPawn))
	{
		InstanceData.WorkOrderComponent->TryFailWorkOrder(
			Snapshot.WorkOrderId);
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RetryTimeRemaining = FMath::Max(
		0.0f,
		InstanceData.RetryTimeRemaining - DeltaTime);
	if (InstanceData.bMoveRequested
		&& InstanceData.CompanionController->GetMoveStatus()
			!= EPathFollowingStatus::Moving)
	{
		InstanceData.bMoveRequested = false;
	}

	if (Snapshot.State == EAIRECompanionWorkOrderState::Requested)
	{
		if (!InstanceData.WorkOrderComponent->TryStartMoving(
				Snapshot.WorkOrderId))
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			return EStateTreeRunStatus::Failed;
		}
		Snapshot = InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
	}

	if (Snapshot.WorkType == EAIRECompanionWorkOrderType::Crafting)
	{
		const FAI_RECraftingRecipe* Recipe = ResolveCraftingRecipe(
			TargetActor,
			Snapshot.RecipeTable.Get(),
			Snapshot.RecipeRowId);
		if (Recipe == nullptr
			|| !FMath::IsFinite(InstanceData.WorkbenchInteractionGap)
			|| InstanceData.WorkbenchInteractionGap < 0.0f)
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		if (Snapshot.State == EAIRECompanionWorkOrderState::Moving)
		{
			const FVector ApproachLocation =
				CalculateWorkbenchApproachLocation(
					*CompanionPawn,
					*TargetActor,
					InstanceData.WorkbenchInteractionGap);
			if (FVector::Dist2D(
					CompanionPawn->GetActorLocation(),
					ApproachLocation)
				> WorkApproachAcceptanceRadius)
			{
				if (!InstanceData.bMoveRequested
					&& InstanceData.RetryTimeRemaining <= 0.0f)
				{
					const EPathFollowingRequestResult::Type MoveResult =
						InstanceData.CompanionController->MoveToLocation(
							ApproachLocation,
							WorkApproachAcceptanceRadius,
							false,
							true,
							true,
							true,
							nullptr,
							true);
					InstanceData.bMoveRequested =
						MoveResult
							== EPathFollowingRequestResult::RequestSuccessful;
					if (MoveResult == EPathFollowingRequestResult::Failed)
					{
						InstanceData.RetryTimeRemaining =
							WorkMovementRetryInterval;
					}
				}
				return EStateTreeRunStatus::Running;
			}

			InstanceData.CompanionController->StopMovement();
			InstanceData.bMoveRequested = false;
			InstanceData.CompanionController->SetFocus(
				TargetActor,
				EAIFocusPriority::Gameplay);
			FaceWorkTarget(*CompanionPawn, *TargetActor);

			UAI_REItemDataAsset* ResultItemAsset = ResolveItemAsset(
				*InstanceData.CompanionCharacter,
				Recipe->ResultItemId);
			FAIREMakoCraftWorkRequest CraftRequest;
			FAIREInventoryWorkResult PreflightResult;
			const bool bCanWorldDrop =
				InstanceData.DroppedItemActorClass != nullptr
				&& IsValid(ResultItemAsset);
			if (!BuildCraftWorkRequest(
					*InstanceData.CompanionCharacter,
					*InstanceData.InventoryComponent,
					Snapshot.WorkOrderId,
					*Recipe,
					bCanWorldDrop,
					CraftRequest)
				|| !InstanceData.InventoryComponent
					->CanCompleteMakoCraftWork(
						CraftRequest,
						PreflightResult)
				|| !InstanceData.WorkOrderComponent->TryStartWorking(
					Snapshot.WorkOrderId))
			{
				InstanceData.WorkOrderComponent->TryFailWorkOrder(
					Snapshot.WorkOrderId);
				CancelOwnedRequests(InstanceData);
				return EStateTreeRunStatus::Failed;
			}
			Snapshot = InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
		}

		const FVector WorkbenchApproachLocation =
			CalculateWorkbenchApproachLocation(
				*CompanionPawn,
				*TargetActor,
				InstanceData.WorkbenchInteractionGap);
		if (Snapshot.State != EAIRECompanionWorkOrderState::Working
			|| FVector::Dist2D(
				CompanionPawn->GetActorLocation(),
				WorkbenchApproachLocation)
				> WorkApproachAcceptanceRadius)
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.CompanionController->SetFocus(
			TargetActor,
			EAIFocusPriority::Gameplay);
		FaceWorkTarget(*CompanionPawn, *TargetActor);
		if (IsValid(InstanceData.WorkMontage))
		{
			UAnimInstance* AnimInstance =
				InstanceData.CompanionCharacter->GetMesh()
					? InstanceData.CompanionCharacter->GetMesh()
						->GetAnimInstance()
					: nullptr;
			if (IsValid(AnimInstance)
				&& !AnimInstance->Montage_IsPlaying(
					InstanceData.WorkMontage))
			{
				AnimInstance->Montage_Play(InstanceData.WorkMontage);
			}
		}

		InstanceData.ElapsedCraftingTime += FMath::Max(0.0f, DeltaTime);
		if (InstanceData.ElapsedCraftingTime < Recipe->CraftingTime)
		{
			return EStateTreeRunStatus::Running;
		}

		UAI_REItemDataAsset* ResultItemAsset = ResolveItemAsset(
			*InstanceData.CompanionCharacter,
			Recipe->ResultItemId);
		const bool bCanWorldDrop =
			InstanceData.DroppedItemActorClass != nullptr
			&& IsValid(ResultItemAsset);
		FAIREMakoCraftWorkRequest CraftRequest;
		FAIREInventoryWorkResult PreflightResult;
		if (!BuildCraftWorkRequest(
				*InstanceData.CompanionCharacter,
				*InstanceData.InventoryComponent,
				Snapshot.WorkOrderId,
				*Recipe,
				bCanWorldDrop,
				CraftRequest)
			|| !InstanceData.InventoryComponent->CanCompleteMakoCraftWork(
				CraftRequest,
				PreflightResult))
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		FTransform DeferredDropTransform;
		AAI_REItemActor* DeferredDrop = nullptr;
		if (!PreflightResult.bAlreadyApplied
			&& PreflightResult.Destination
				== EAIREInventoryWorkResultDestination::WorldDrop)
		{
			DeferredDrop = IsValid(ResultItemAsset)
				? SpawnDeferredWorkResult(
					*InstanceData.CompanionCharacter,
					*TargetActor,
					InstanceData.DroppedItemActorClass,
					*ResultItemAsset,
					Recipe->ResultAmount,
					DeferredDropTransform)
				: nullptr;
			if (!IsValid(DeferredDrop))
			{
				InstanceData.WorkOrderComponent->TryFailWorkOrder(
					Snapshot.WorkOrderId);
				CancelOwnedRequests(InstanceData);
				return EStateTreeRunStatus::Failed;
			}
		}

		const FAIREInventoryWorkResult CompletionResult =
			InstanceData.InventoryComponent->TryCompleteMakoCraftWork(
				CraftRequest);
		const bool bCompletionApplied =
			CompletionResult.Code == EAIREInventoryMutationCode::Succeeded
			|| CompletionResult.Code
				== EAIREInventoryMutationCode::AlreadyApplied;
		if (!bCompletionApplied)
		{
			if (IsValid(DeferredDrop))
			{
				DeferredDrop->Destroy();
			}
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}
		if (IsValid(DeferredDrop))
		{
			if (CompletionResult.Code == EAIREInventoryMutationCode::Succeeded
				&& CompletionResult.Destination
					== EAIREInventoryWorkResultDestination::WorldDrop)
			{
				DeferredDrop->FinishSpawning(DeferredDropTransform);
			}
			else
			{
				DeferredDrop->Destroy();
			}
		}

		if (!InstanceData.WorkOrderComponent->TryCompleteWorkOrder(
				Snapshot.WorkOrderId))
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Succeeded;
	}

	if (Snapshot.WorkType == EAIRECompanionWorkOrderType::Harvesting)
	{
		AAI_REHarvestableResourceActor* ResourceActor =
			Cast<AAI_REHarvestableResourceActor>(TargetActor);
		UAI_REHarvestableResourceComponent* ResourceComponent =
			IsValid(ResourceActor)
				? ResourceActor->GetHarvestableResourceComponent()
				: nullptr;
		if (IsValid(ResourceComponent) && ResourceComponent->IsDepleted())
		{
			if (Snapshot.State == EAIRECompanionWorkOrderState::Moving)
			{
				InstanceData.WorkOrderComponent->TryStartWorking(
					Snapshot.WorkOrderId);
			}
			if (InstanceData.WorkOrderComponent->TryCompleteWorkOrder(
					Snapshot.WorkOrderId))
			{
				CancelOwnedRequests(InstanceData);
				return EStateTreeRunStatus::Succeeded;
			}
		}
		if (!FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(
				ResourceActor))
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition =
			InstanceData.EquipmentComponent->GetCurrentWeaponDefinition();
		const float AttackRange = IsValid(WeaponDefinition)
			? WeaponDefinition->AttackRange
			: -1.0f;
		if (!IsValid(WeaponDefinition)
			|| !WeaponDefinition->IsMeleeWeapon()
			|| !FMath::IsFinite(AttackRange)
			|| AttackRange < 0.0f)
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		const bool bInAttackRange = IsWithinSurfaceRange(
			*CompanionPawn,
			*TargetActor,
			AttackRange);
		if (!bInAttackRange)
		{
			if (!InstanceData.bMoveRequested
				&& InstanceData.RetryTimeRemaining <= 0.0f)
			{
				const FVector ApproachLocation = CalculateCombatApproachLocation(
					*CompanionPawn,
					*TargetActor,
					AttackRange);
				const EPathFollowingRequestResult::Type MoveResult =
					InstanceData.CompanionController->MoveToLocation(
						ApproachLocation,
						WorkApproachAcceptanceRadius,
						false,
						true,
						true,
						true,
						nullptr,
						true);
				InstanceData.bMoveRequested =
					MoveResult
						== EPathFollowingRequestResult::RequestSuccessful;
				if (MoveResult == EPathFollowingRequestResult::Failed)
				{
					InstanceData.RetryTimeRemaining =
						WorkMovementRetryInterval;
				}
			}
			return EStateTreeRunStatus::Running;
		}

		InstanceData.CompanionController->StopMovement();
		InstanceData.bMoveRequested = false;
		InstanceData.CompanionController->SetFocus(
			TargetActor,
			EAIFocusPriority::Gameplay);
		FaceWorkTarget(*CompanionPawn, *TargetActor);
		if (Snapshot.State == EAIRECompanionWorkOrderState::Moving)
		{
			if (!InstanceData.WorkOrderComponent->TryStartWorking(
					Snapshot.WorkOrderId))
			{
				InstanceData.WorkOrderComponent->TryFailWorkOrder(
					Snapshot.WorkOrderId);
				return EStateTreeRunStatus::Failed;
			}
			Snapshot = InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
		}
		if (Snapshot.State != EAIRECompanionWorkOrderState::Working)
		{
			return EStateTreeRunStatus::Running;
		}

		if (InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
				AIRECompanionGameplayTags::StateActionAttacking)
			|| InstanceData.RetryTimeRemaining > 0.0f)
		{
			return EStateTreeRunStatus::Running;
		}

		FGameplayEventData AttackRequest;
		AttackRequest.EventTag =
			AIRECompanionGameplayTags::EventAttackRequest;
		AttackRequest.Instigator = CompanionPawn;
		AttackRequest.Target = TargetActor;
		InstanceData.AbilitySystemComponent->HandleGameplayEvent(
			AIRECompanionGameplayTags::EventAttackRequest,
			&AttackRequest);
		InstanceData.RetryTimeRemaining = WorkActivationRetryInterval;
		return EStateTreeRunStatus::Running;
	}

	if (Snapshot.WorkType
		== EAIRECompanionWorkOrderType::StorageTransfer)
	{
		AAIRESharedStorageActor* StorageActor =
			Cast<AAIRESharedStorageActor>(TargetActor);
		const UAIRECompanionConfigDataAsset* CompanionConfig =
			InstanceData.CompanionCharacter->GetCompanionConfig();
		if (!IsValid(StorageActor)
			|| !IsValid(CompanionConfig)
			|| !FMath::IsFinite(
				CompanionConfig->StorageAcceptanceRadius)
			|| CompanionConfig->StorageAcceptanceRadius < 0.0f
			|| !FMath::IsFinite(
				CompanionConfig->StorageMovementTimeout)
			|| CompanionConfig->StorageMovementTimeout <= 0.0f
			|| !FMath::IsFinite(
				CompanionConfig->StorageWorkDuration)
			|| CompanionConfig->StorageWorkDuration < 0.0f)
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		const FTransform InteractionTransform =
			StorageActor->GetCompanionInteractionTransform();
		const FVector InteractionLocation =
			InteractionTransform.GetLocation();
		if (Snapshot.State == EAIRECompanionWorkOrderState::Moving)
		{
			InstanceData.ElapsedStorageMovementTime +=
				FMath::Max(0.0f, DeltaTime);
			if (InstanceData.ElapsedStorageMovementTime
				> CompanionConfig->StorageMovementTimeout)
			{
				InstanceData.WorkOrderComponent->TryFailWorkOrder(
					Snapshot.WorkOrderId);
				CancelOwnedRequests(InstanceData);
				return EStateTreeRunStatus::Failed;
			}

			if (FVector::Dist2D(
					CompanionPawn->GetActorLocation(),
					InteractionLocation)
				> CompanionConfig->StorageAcceptanceRadius)
			{
				if (!InstanceData.bMoveRequested
					&& InstanceData.RetryTimeRemaining <= 0.0f)
				{
					const EPathFollowingRequestResult::Type MoveResult =
						InstanceData.CompanionController->MoveToLocation(
							InteractionLocation,
							CompanionConfig->StorageAcceptanceRadius,
							false,
							true,
							true,
							true,
							nullptr,
							true);
					InstanceData.bMoveRequested =
						MoveResult
							== EPathFollowingRequestResult::
								RequestSuccessful;
					if (MoveResult
						== EPathFollowingRequestResult::Failed)
					{
						InstanceData.RetryTimeRemaining =
							WorkMovementRetryInterval;
					}
				}
				return EStateTreeRunStatus::Running;
			}

			InstanceData.CompanionController->StopMovement();
			InstanceData.CompanionController->ClearFocus(
				EAIFocusPriority::Gameplay);
			InstanceData.bMoveRequested = false;
			CompanionPawn->SetActorRotation(
				FRotator(
					0.0f,
					InteractionTransform.Rotator().Yaw,
					0.0f));
			if (!InstanceData.WorkOrderComponent->TryStartWorking(
					Snapshot.WorkOrderId))
			{
				InstanceData.WorkOrderComponent->TryFailWorkOrder(
					Snapshot.WorkOrderId);
				CancelOwnedRequests(InstanceData);
				return EStateTreeRunStatus::Failed;
			}
			InstanceData.ElapsedStorageWorkTime = 0.0f;
			Snapshot =
				InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
		}

		if (Snapshot.State != EAIRECompanionWorkOrderState::Working
			|| FVector::Dist2D(
				CompanionPawn->GetActorLocation(),
				InteractionLocation)
				> CompanionConfig->StorageAcceptanceRadius)
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.CompanionController->StopMovement();
		InstanceData.CompanionController->ClearFocus(
			EAIFocusPriority::Gameplay);
		CompanionPawn->SetActorRotation(
			FRotator(
				0.0f,
				InteractionTransform.Rotator().Yaw,
				0.0f));
		if (IsValid(CompanionConfig->StorageWorkMontage))
		{
			UAnimInstance* AnimInstance =
				InstanceData.CompanionCharacter->GetMesh()
					? InstanceData.CompanionCharacter->GetMesh()
						->GetAnimInstance()
					: nullptr;
			if (IsValid(AnimInstance)
				&& !AnimInstance->Montage_IsPlaying(
					CompanionConfig->StorageWorkMontage))
			{
				AnimInstance->Montage_Play(
					CompanionConfig->StorageWorkMontage);
			}
			InstanceData.ActiveStorageMontage =
				CompanionConfig->StorageWorkMontage;
		}

		InstanceData.ElapsedStorageWorkTime +=
			FMath::Max(0.0f, DeltaTime);
		if (InstanceData.ElapsedStorageWorkTime
			< CompanionConfig->StorageWorkDuration)
		{
			return EStateTreeRunStatus::Running;
		}

		UGameInstance* GameInstance =
			InstanceData.CompanionCharacter->GetGameInstance();
		UAIREGameplayInventorySubsystem* GameplayInventory =
			IsValid(GameInstance)
				? GameInstance
					->GetSubsystem<UAIREGameplayInventorySubsystem>()
				: nullptr;
		FAIREInventoryTransferRequest TransferRequest;
		if (!IsValid(GameplayInventory)
			|| !BuildStorageTransferRequest(
				*InstanceData.InventoryComponent,
				*StorageActor,
				*CompanionConfig,
				Snapshot,
				TransferRequest))
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		const FAIREInventoryMutationResult TransferResult =
			GameplayInventory->TryTransferItem(TransferRequest);
		if (!TransferResult.WasApplied()
			|| !InstanceData.WorkOrderComponent->TryCompleteWorkOrder(
				Snapshot.WorkOrderId))
		{
			InstanceData.WorkOrderComponent->TryFailWorkOrder(
				Snapshot.WorkOrderId);
			CancelOwnedRequests(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.WorkOrderComponent->TryFailWorkOrder(Snapshot.WorkOrderId);
	CancelOwnedRequests(InstanceData);
	return EStateTreeRunStatus::Failed;
}

void FAIRECompanionExecuteWorkOrderTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.bIsCombatRequested
		&& IsValid(InstanceData.WorkOrderComponent))
	{
		const FAIRECompanionWorkOrderSnapshot Snapshot =
			InstanceData.WorkOrderComponent->GetWorkOrderSnapshot();
		if (Snapshot.WorkType
			== EAIRECompanionWorkOrderType::StorageTransfer)
		{
			InstanceData.ElapsedStorageMovementTime = 0.0f;
			InstanceData.ElapsedStorageWorkTime = 0.0f;
		}
		if (Snapshot.WorkOrderId == InstanceData.ActiveWorkOrderId
			&& (Snapshot.State == EAIRECompanionWorkOrderState::Moving
				|| Snapshot.State
					== EAIRECompanionWorkOrderState::Working))
		{
			InstanceData.WorkOrderComponent->TryPauseForCombat(
				Snapshot.WorkOrderId);
		}
	}
	CancelOwnedRequests(InstanceData);
}

void FAIRECompanionExecuteWorkOrderTask::CancelOwnedRequests(
	FInstanceDataType& InstanceData)
{
	if (IsValid(InstanceData.CompanionController))
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.CompanionController->ClearFocus(
			EAIFocusPriority::Gameplay);
	}
	InstanceData.bMoveRequested = false;

	if (IsValid(InstanceData.CompanionCharacter)
		&& IsValid(InstanceData.WorkMontage))
	{
		UAnimInstance* AnimInstance =
			InstanceData.CompanionCharacter->GetMesh()
				? InstanceData.CompanionCharacter->GetMesh()->GetAnimInstance()
				: nullptr;
		if (IsValid(AnimInstance))
		{
			AnimInstance->Montage_Stop(0.2f, InstanceData.WorkMontage);
		}
	}
	if (IsValid(InstanceData.CompanionCharacter)
		&& IsValid(InstanceData.ActiveStorageMontage))
	{
		UAnimInstance* AnimInstance =
			InstanceData.CompanionCharacter->GetMesh()
				? InstanceData.CompanionCharacter->GetMesh()
					->GetAnimInstance()
				: nullptr;
		if (IsValid(AnimInstance))
		{
			AnimInstance->Montage_Stop(
				0.2f,
				InstanceData.ActiveStorageMontage);
		}
	}
	InstanceData.ActiveStorageMontage = nullptr;

	if (!IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		return;
	}
	const FGameplayAbilitySpecHandle AttackAbilityHandle =
		InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatBasicAttack);
	if (AttackAbilityHandle.IsValid())
	{
		InstanceData.AbilitySystemComponent->CancelAbilityHandle(
			AttackAbilityHandle);
	}
}

FAIRECompanionEngageThreatTask::FAIRECompanionEngageThreatTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionEngageThreatTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Warning,
			TEXT("Engage Threat received invalid component bindings."));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ActiveTarget = InstanceData.ThreatTarget;
	InstanceData.RetryTimeRemaining = 0.0f;
	InstanceData.bMoveRequested = false;
	InstanceData.bSkillIntentBuffered = false;
	InstanceData.bSkillIntentEvaluatedForStep = false;
	InstanceData.bWasSkillCancelWindowOpen = false;
	InstanceData.bWasBasicAttackActive = false;
	UE_LOG(
		LogAIRECompanionStateTree,
		Log,
		TEXT("Companion threat engagement started. Companion=%s Target=%s"),
		*GetNameSafe(InstanceData.CompanionController->GetPawn()),
		*GetNameSafe(InstanceData.ThreatTarget));
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIRECompanionEngageThreatTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.ActiveTarget.Get() != InstanceData.ThreatTarget)
	{
		CancelOwnedRequests(InstanceData);
		InstanceData.ActiveTarget = InstanceData.ThreatTarget;
		InstanceData.RetryTimeRemaining = 0.0f;
		InstanceData.bSkillIntentBuffered = false;
		InstanceData.bSkillIntentEvaluatedForStep = false;
		InstanceData.bWasSkillCancelWindowOpen = false;
		InstanceData.bWasBasicAttackActive = false;
	}

	APawn* CompanionPawn = InstanceData.CompanionController->GetPawn();
	AActor* TargetActor = InstanceData.ActiveTarget.Get();
	if (!IsValid(CompanionPawn) || !IsTargetUsable(TargetActor))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Running;
	}

	const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition =
		InstanceData.EquipmentComponent->GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Running;
	}
	const float AttackRange = WeaponDefinition->AttackRange;

	InstanceData.CompanionController->SetFocus(
		TargetActor,
		EAIFocusPriority::Gameplay);

	bool bAttackActive = InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
		AIRECompanionGameplayTags::StateActionAttacking);
	const bool bCombatSkillWasActive =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttackingSkill);
	const float ActiveAttackRange = bCombatSkillWasActive
		? WeaponDefinition->CombatSkill.AttackRange
		: AttackRange;
	const float AttackExitDistance =
		ActiveAttackRange + CombatRangeExitSlack;
	if (bAttackActive
		&& !IsTargetInRange(*CompanionPawn, *TargetActor, AttackExitDistance))
	{
		const FGameplayTag CombatAbilityTags[] =
		{
			AIRECompanionGameplayTags::AbilityCombatBasicAttack,
			AIRECompanionGameplayTags::AbilityCombatSkill
		};
		for (const FGameplayTag CombatAbilityTag : CombatAbilityTags)
		{
			const FGameplayAbilitySpecHandle AbilityHandle =
				InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
					CombatAbilityTag);
			if (AbilityHandle.IsValid())
			{
				InstanceData.AbilitySystemComponent->CancelAbilityHandle(
					AbilityHandle);
			}
		}
		bAttackActive = InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttacking);
		InstanceData.RetryTimeRemaining = 0.0f;
		InstanceData.bSkillIntentBuffered = false;
		InstanceData.bSkillIntentEvaluatedForStep = false;
		InstanceData.bWasSkillCancelWindowOpen = false;
		InstanceData.bWasBasicAttackActive = false;
	}

	InstanceData.RetryTimeRemaining = FMath::Max(
		0.0f,
		InstanceData.RetryTimeRemaining - DeltaTime);
	if (InstanceData.bMoveRequested
		&& InstanceData.CompanionController->GetMoveStatus()
			!= EPathFollowingStatus::Moving)
	{
		InstanceData.bMoveRequested = false;
	}

	if (!IsTargetInRange(*CompanionPawn, *TargetActor, AttackRange))
	{
		if (!bAttackActive
			&& !InstanceData.bMoveRequested
			&& InstanceData.RetryTimeRemaining <= 0.0f)
		{
			const FVector ApproachLocation = CalculateCombatApproachLocation(
				*CompanionPawn,
				*TargetActor,
				AttackRange);
			const EPathFollowingRequestResult::Type MoveResult =
				InstanceData.CompanionController->MoveToLocation(
					ApproachLocation,
					CombatApproachAcceptanceRadius,
					false,
					true,
					true,
					true,
					nullptr,
					true);
			InstanceData.bMoveRequested =
				MoveResult == EPathFollowingRequestResult::RequestSuccessful;
			if (MoveResult == EPathFollowingRequestResult::Failed)
			{
				UE_LOG(
					LogAIRECompanionStateTree,
					Warning,
					TEXT("Companion combat move failed. Companion=%s Target=%s ApproachLocation=%s AcceptanceRadius=%.2f"),
					*GetNameSafe(CompanionPawn),
					*GetNameSafe(TargetActor),
					*ApproachLocation.ToCompactString(),
					CombatApproachAcceptanceRadius);
				InstanceData.RetryTimeRemaining = CombatMovementRetryInterval;
			}
		}
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bMoveRequested)
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.bMoveRequested = false;
	}

	const bool bBasicAttackActive =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttackingBasic);
	const bool bCombatSkillActive =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttackingSkill);
	const bool bSkillCancelWindowOpen =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::
				StateActionAttackingSkillCancelable);
	const FGameplayAbilitySpecHandle CombatSkillHandle =
		InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatSkill);
	const bool bCanSelectCombatSkill =
		WeaponDefinition->CombatSkill.bEnabled
		&& CombatSkillHandle.IsValid();
	const auto ShouldSelectCombatSkill =
		[WeaponDefinition, bCanSelectCombatSkill]()
		{
			return bCanSelectCombatSkill
				&& FMath::FRand()
					< WeaponDefinition->CombatSkill.SelectionChance;
		};
	const auto RequestCombatSkill =
		[&InstanceData, CompanionPawn, TargetActor]()
		{
			FGameplayEventData SkillRequest;
			SkillRequest.EventTag =
				AIRECompanionGameplayTags::EventCombatSkillRequest;
			SkillRequest.Instigator = CompanionPawn;
			SkillRequest.Target = TargetActor;
			return InstanceData.AbilitySystemComponent->HandleGameplayEvent(
				AIRECompanionGameplayTags::EventCombatSkillRequest,
				&SkillRequest);
		};

	if (bCombatSkillActive)
	{
		return EStateTreeRunStatus::Running;
	}

	if (bBasicAttackActive)
	{
		if (!bSkillCancelWindowOpen
			&& InstanceData.bWasSkillCancelWindowOpen)
		{
			InstanceData.bSkillIntentBuffered = false;
			InstanceData.bSkillIntentEvaluatedForStep = false;
		}

		if (!InstanceData.bSkillIntentEvaluatedForStep)
		{
			InstanceData.bSkillIntentEvaluatedForStep = true;
			InstanceData.bSkillIntentBuffered =
				ShouldSelectCombatSkill();
		}

		if (bSkillCancelWindowOpen
			&& !InstanceData.bWasSkillCancelWindowOpen
			&& InstanceData.bSkillIntentBuffered)
		{
			const int32 ActivatedAbilityCount = RequestCombatSkill();
			UE_LOG(
				LogAIRECompanionStateTree,
				Verbose,
				TEXT("Companion buffered combat skill requested during combo. Target=%s ActivatedAbilities=%d"),
				*GetNameSafe(TargetActor),
				ActivatedAbilityCount);
			InstanceData.bSkillIntentBuffered = false;
		}

		InstanceData.bWasSkillCancelWindowOpen =
			bSkillCancelWindowOpen;
		InstanceData.bWasBasicAttackActive = true;
		return EStateTreeRunStatus::Running;
	}

	if (bAttackActive)
	{
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bWasBasicAttackActive)
	{
		InstanceData.bWasBasicAttackActive = false;
		InstanceData.bSkillIntentEvaluatedForStep = false;
		InstanceData.bWasSkillCancelWindowOpen = false;
	}

	if (InstanceData.RetryTimeRemaining > 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	FVector TargetDirection =
		TargetActor->GetActorLocation() - CompanionPawn->GetActorLocation();
	TargetDirection.Z = 0.0f;
	if (!TargetDirection.IsNearlyZero())
	{
		CompanionPawn->SetActorRotation(
			FRotator(
				0.0f,
				TargetDirection.Rotation().Yaw,
				0.0f));
	}

	const bool bShouldTryCombatSkill =
		InstanceData.bSkillIntentBuffered
		|| ShouldSelectCombatSkill();
	InstanceData.bSkillIntentBuffered = false;
	if (bShouldTryCombatSkill)
	{
		const int32 ActivatedAbilityCount = RequestCombatSkill();
		const bool bCombatSkillActiveAfterRequest =
			InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
				AIRECompanionGameplayTags::
					StateActionAttackingSkill);
		const bool bCombatSkillCommitted =
			ActivatedAbilityCount > 0
			&& bCombatSkillActiveAfterRequest;
		if (bCombatSkillCommitted)
		{
			UE_LOG(
				LogAIRECompanionStateTree,
				Verbose,
				TEXT("Companion combat skill requested. Target=%s ActivatedAbilities=%d RetryDelay=%.2f"),
				*GetNameSafe(TargetActor),
				ActivatedAbilityCount,
				CombatActivationRetryInterval);
			InstanceData.RetryTimeRemaining =
				CombatActivationRetryInterval;
			return EStateTreeRunStatus::Running;
		}
	}

	FGameplayEventData AttackRequest;
	AttackRequest.EventTag = AIRECompanionGameplayTags::EventAttackRequest;
	AttackRequest.Instigator = CompanionPawn;
	AttackRequest.Target = TargetActor;
	const int32 ActivatedAbilityCount =
		InstanceData.AbilitySystemComponent->HandleGameplayEvent(
		AIRECompanionGameplayTags::EventAttackRequest,
		&AttackRequest);
	if (ActivatedAbilityCount > 0)
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Verbose,
			TEXT("Companion attack requested. Target=%s ActivatedAbilities=%d RetryDelay=%.2f"),
			*GetNameSafe(TargetActor),
			ActivatedAbilityCount,
			CombatActivationRetryInterval);
	}
	else
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Log,
			TEXT("Companion attack request activated no abilities. Companion=%s Target=%s RetryDelay=%.2f"),
			*GetNameSafe(CompanionPawn),
			*GetNameSafe(TargetActor),
			CombatActivationRetryInterval);
	}
	InstanceData.RetryTimeRemaining = CombatActivationRetryInterval;
	return EStateTreeRunStatus::Running;
}

void FAIRECompanionEngageThreatTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	CancelOwnedRequests(InstanceData);
	UE_LOG(
		LogAIRECompanionStateTree,
		Log,
		TEXT("Companion threat engagement ended. Target=%s RunStatus=%s"),
		*GetNameSafe(InstanceData.ActiveTarget.Get()),
		*StaticEnum<EStateTreeRunStatus>()->GetNameStringByValue(
			static_cast<int64>(Transition.CurrentRunStatus)));
	InstanceData.ActiveTarget.Reset();
	InstanceData.RetryTimeRemaining = 0.0f;
	InstanceData.bSkillIntentBuffered = false;
	InstanceData.bSkillIntentEvaluatedForStep = false;
	InstanceData.bWasSkillCancelWindowOpen = false;
	InstanceData.bWasBasicAttackActive = false;
}

bool FAIRECompanionEngageThreatTask::IsTargetUsable(const AActor* TargetActor)
{
	return IsValid(TargetActor)
		&& TargetActor->GetClass()->ImplementsInterface(
			UAIREThreatTargetInterface::StaticClass())
		&& IAIREThreatTargetInterface::Execute_IsAliveThreatTarget(
			const_cast<AActor*>(TargetActor));
}

bool FAIRECompanionEngageThreatTask::IsTargetInRange(
	const APawn& CompanionPawn,
	const AActor& TargetActor,
	const float AttackRange)
{
	const float HorizontalDistance = FVector::Dist2D(
		CompanionPawn.GetActorLocation(),
		TargetActor.GetActorLocation());
	const float EffectiveDistance = FMath::Max(
		0.0f,
		HorizontalDistance
			- CompanionPawn.GetSimpleCollisionRadius()
			- TargetActor.GetSimpleCollisionRadius());
	return EffectiveDistance <= AttackRange;
}

void FAIRECompanionEngageThreatTask::CancelOwnedRequests(
	FInstanceDataType& InstanceData)
{
	if (IsValid(InstanceData.CompanionController))
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.CompanionController->ClearFocus(
			EAIFocusPriority::Gameplay);
	}
	InstanceData.bMoveRequested = false;

	if (!IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		return;
	}

	const FGameplayAbilitySpecHandle AttackAbilityHandle =
		InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatBasicAttack);
	if (AttackAbilityHandle.IsValid())
	{
		InstanceData.AbilitySystemComponent->CancelAbilityHandle(
			AttackAbilityHandle);
	}

	const FGameplayAbilitySpecHandle CombatSkillAbilityHandle =
		InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatSkill);
	if (CombatSkillAbilityHandle.IsValid())
	{
		InstanceData.AbilitySystemComponent->CancelAbilityHandle(
			CombatSkillAbilityHandle);
	}

	InstanceData.bSkillIntentBuffered = false;
	InstanceData.bSkillIntentEvaluatedForStep = false;
	InstanceData.bWasSkillCancelWindowOpen = false;
	InstanceData.bWasBasicAttackActive = false;
}

FAIRECompanionEngageSupportTask::FAIRECompanionEngageSupportTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionEngageSupportTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.SupportComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent)
		|| !InstanceData.SupportComponent->IsSupportRequested()
		|| InstanceData.SupportComponent->GetSupportTarget()
			!= InstanceData.SupportTarget)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ActiveTarget = InstanceData.SupportTarget;
	InstanceData.RetryTimeRemaining = 0.0f;
	InstanceData.bMoveRequested = false;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIRECompanionEngageSupportTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.SupportComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	AActor* TargetActor = InstanceData.ActiveTarget.Get();
	APawn* CompanionPawn = InstanceData.CompanionController->GetPawn();
	if (!IsValid(TargetActor)
		|| !IsValid(CompanionPawn)
		|| InstanceData.SupportComponent->GetSupportTarget()
			!= TargetActor
		|| !InstanceData.SupportComponent->IsSupportRequested())
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Running;
	}

	const float SupportRange =
		InstanceData.SupportComponent->GetSupportRange();
	if (!FMath::IsFinite(SupportRange) || SupportRange < 0.0f)
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RetryTimeRemaining = FMath::Max(
		0.0f,
		InstanceData.RetryTimeRemaining - DeltaTime);
	if (InstanceData.bMoveRequested
		&& InstanceData.CompanionController->GetMoveStatus()
			!= EPathFollowingStatus::Moving)
	{
		InstanceData.bMoveRequested = false;
	}

	const bool bSupportAbilityActive =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionSupporting);
	if (!IsTargetInRange(
			*CompanionPawn,
			*TargetActor,
			SupportRange))
	{
		if (bSupportAbilityActive)
		{
			const FGameplayAbilitySpecHandle AbilityHandle =
				InstanceData.SupportComponent
					->FindSupportAbilityHandle();
			if (AbilityHandle.IsValid())
			{
				InstanceData.AbilitySystemComponent
					->CancelAbilityHandle(AbilityHandle);
			}
		}

		if (!InstanceData.bMoveRequested
			&& InstanceData.RetryTimeRemaining <= 0.0f)
		{
			const FVector ApproachLocation =
				CalculateSupportApproachLocation(
					*CompanionPawn,
					*TargetActor,
					SupportRange);
			const EPathFollowingRequestResult::Type MoveResult =
				InstanceData.CompanionController->MoveToLocation(
					ApproachLocation,
					SupportApproachAcceptanceRadius,
					false,
					true,
					true,
					true,
					nullptr,
					true);
			InstanceData.bMoveRequested =
				MoveResult
				== EPathFollowingRequestResult::RequestSuccessful;
			if (MoveResult == EPathFollowingRequestResult::Failed)
			{
				InstanceData.RetryTimeRemaining =
					SupportMovementRetryInterval;
			}
		}
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bMoveRequested)
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.bMoveRequested = false;
	}

	InstanceData.CompanionController->SetFocus(
		TargetActor,
		EAIFocusPriority::Gameplay);
	if (bSupportAbilityActive
		|| InstanceData.RetryTimeRemaining > 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	FGameplayEventData HealRequest;
	HealRequest.EventTag =
		AIRECompanionGameplayTags::EventSupportHealRequest;
	HealRequest.Instigator = CompanionPawn;
	HealRequest.Target = TargetActor;
	InstanceData.AbilitySystemComponent->HandleGameplayEvent(
		AIRECompanionGameplayTags::EventSupportHealRequest,
		&HealRequest);
	InstanceData.RetryTimeRemaining =
		SupportActivationRetryInterval;
	return EStateTreeRunStatus::Running;
}

void FAIRECompanionEngageSupportTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	CancelOwnedRequests(InstanceData);
	InstanceData.ActiveTarget.Reset();
	InstanceData.RetryTimeRemaining = 0.0f;
}

bool FAIRECompanionEngageSupportTask::IsTargetInRange(
	const APawn& CompanionPawn,
	const AActor& TargetActor,
	const float SupportRange)
{
	const float HorizontalDistance = FVector::Dist2D(
		CompanionPawn.GetActorLocation(),
		TargetActor.GetActorLocation());
	const float EffectiveDistance = FMath::Max(
		0.0f,
		HorizontalDistance
			- CompanionPawn.GetSimpleCollisionRadius()
			- TargetActor.GetSimpleCollisionRadius());
	return EffectiveDistance <= SupportRange;
}

void FAIRECompanionEngageSupportTask::CancelOwnedRequests(
	FInstanceDataType& InstanceData)
{
	if (IsValid(InstanceData.CompanionController))
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.CompanionController->ClearFocus(
			EAIFocusPriority::Gameplay);
	}
	InstanceData.bMoveRequested = false;

	if (IsValid(InstanceData.SupportComponent))
	{
		InstanceData.SupportComponent->CancelSupportRequest();
	}
}

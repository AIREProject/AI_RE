#include "Testing/AIRECompanionTestingBlueprintLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "Core/AIRECompanionCharacter.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionDamageGameplayEffect.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AI_RECraftingTypes.h"
#include "AI_REHarvestableResourceActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "AI_REWorkBenchBase.h"
#include "AIREGameplayInventorySubsystem.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Work/AIRECompanionCraftingWorkRequest.h"
#include "Work/AIRECompanionHarvestWorkRequest.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionTesting, Log, All);

namespace
{
	AAIRECompanionAIController* FindFirstCompanionController(const UObject* WorldContextObject)
	{
		if (!IsValid(GEngine) || !IsValid(WorldContextObject))
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (!IsValid(World))
		{
			return nullptr;
		}

		for (TActorIterator<AAIRECompanionAIController> Iterator(World); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator))
			{
				return *Iterator;
			}
		}

		return nullptr;
	}

	AAIRECompanionCharacter* FindFirstCompanionCharacter(const UObject* WorldContextObject)
	{
		if (!IsValid(GEngine) || !IsValid(WorldContextObject))
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (!IsValid(World))
		{
			return nullptr;
		}

		for (TActorIterator<AAIRECompanionCharacter> Iterator(World); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator))
			{
				return *Iterator;
			}
		}

		return nullptr;
	}

	bool IsCloserCandidate(
		const float CandidateDistanceSquared,
		const FString& CandidatePrimaryTieBreaker,
		const FString& CandidateSecondaryTieBreaker,
		const float BestDistanceSquared,
		const FString& BestPrimaryTieBreaker,
		const FString& BestSecondaryTieBreaker)
	{
		if (CandidateDistanceSquared != BestDistanceSquared)
		{
			return CandidateDistanceSquared < BestDistanceSquared;
		}

		if (CandidatePrimaryTieBreaker != BestPrimaryTieBreaker)
		{
			return CandidatePrimaryTieBreaker < BestPrimaryTieBreaker;
		}

		return CandidateSecondaryTieBreaker < BestSecondaryTieBreaker;
	}

	UAIREGameplayInventorySubsystem* FindGameplayInventorySubsystem(
		const UObject* WorldContextObject)
	{
		if (!IsValid(GEngine) || !IsValid(WorldContextObject))
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		return IsValid(GameInstance)
			? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
			: nullptr;
	}

	UAI_REPlayerInventoryComponent* FindFirstPlayerInventory(const UObject* WorldContextObject)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, 0);
		return IsValid(PlayerPawn)
			? PlayerPawn->FindComponentByClass<UAI_REPlayerInventoryComponent>()
			: nullptr;
	}

	bool FindFirstMatchingStack(
		const FAIREInventoryContainerSnapshot& Snapshot,
		const FName ItemId,
		FAIREInventoryItemStackSnapshot& OutStack)
	{
		for (const FAIREInventoryItemStackSnapshot& Stack : Snapshot.ItemStacks)
		{
			if (Stack.ItemId == ItemId && Stack.Count > 0)
			{
				OutStack = Stack;
				return true;
			}
		}

		return false;
	}

	void LogInventorySnapshot(const TCHAR* Label, const FAIREInventoryContainerSnapshot& Snapshot)
	{
		UE_LOG(
			LogAIRECompanionTesting,
			Log,
			TEXT("%s Inventory. Container=%s Session=%s Revision=%lld Capacity=%d Equipped=%s Pending=%s Transition=%d Stacks=%d"),
			Label,
			*Snapshot.ContainerId.ToString(),
			*Snapshot.SessionId.ToString(),
			Snapshot.Revision,
			Snapshot.Capacity,
			*Snapshot.Equipment.EquippedItemId.ToString(),
			*Snapshot.Equipment.PendingItemId.ToString(),
			static_cast<int32>(Snapshot.Equipment.TransitionState),
			Snapshot.ItemStacks.Num());

		for (const FAIREInventoryItemStackSnapshot& Stack : Snapshot.ItemStacks)
		{
			UE_LOG(
				LogAIRECompanionTesting,
				Log,
				TEXT("%s Inventory Stack. Slot=%d Item=%s Count=%d"),
				Label,
				Stack.SlotIndex,
				*Stack.ItemId.ToString(),
				Stack.Count);
		}
	}
}

bool UAIRECompanionTestingBlueprintLibrary::SetFirstCompanionTestBehaviorRequest(
	const UObject* WorldContextObject,
	const EAIRECompanionTestBehaviorRequest Request,
	const bool bIsRequested)
{
	AAIRECompanionAIController* CompanionController = FindFirstCompanionController(WorldContextObject);
	if (!IsValid(CompanionController))
	{
		return false;
	}

	CompanionController->SetTestBehaviorRequest(Request, bIsRequested);
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::ClearFirstCompanionTestBehaviorRequests(
	const UObject* WorldContextObject)
{
	AAIRECompanionAIController* CompanionController = FindFirstCompanionController(WorldContextObject);
	if (!IsValid(CompanionController))
	{
		return false;
	}

	CompanionController->ClearTestBehaviorRequests();
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAIRECompanionWorkOrderComponent* WorkOrderComponent = IsValid(CompanionCharacter)
		? CompanionCharacter->GetWorkOrderComponent()
		: nullptr;
	if (IsValid(WorkOrderComponent) && WorkOrderComponent->HasActiveWorkOrder())
	{
		const FAIRECompanionWorkOrderSnapshot WorkOrder = WorkOrderComponent->GetWorkOrderSnapshot();
		return WorkOrder.WorkOrderId.IsValid()
			&& WorkOrderComponent->TryCancelWorkOrder(WorkOrder.WorkOrderId);
	}

	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::RequestFirstCompanionNearestCraftingWork(
	const UObject* WorldContextObject,
	UDataTable* CraftingRecipeTable)
{
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAIRECompanionWorkOrderComponent* WorkOrderComponent = IsValid(CompanionCharacter)
		? CompanionCharacter->GetWorkOrderComponent()
		: nullptr;
	if (!IsValid(CompanionCharacter)
		|| !IsValid(WorkOrderComponent)
		|| WorkOrderComponent->HasActiveWorkOrder()
		|| !IsValid(CraftingRecipeTable))
	{
		return false;
	}

	FName BestRecipeRowId = NAME_None;
	AAI_REWorkBenchBase* BestWorkbench = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	FString BestWorkbenchPath;
	TArray<FName> RecipeRowNames = CraftingRecipeTable->GetRowNames();
	RecipeRowNames.Sort(FNameLexicalLess());
	for (const FName RecipeRowId : RecipeRowNames)
	{
		const FAI_RECraftingRecipe* Recipe = CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(
			RecipeRowId,
			TEXT("AIRE Companion Crafting Test Fixture"));
		if (Recipe == nullptr
			|| !FMath::IsFinite(Recipe->CraftingTime)
			|| Recipe->CraftingTime <= 0.0f
			|| Recipe->RequiredWorkbench == EWorkbenchType::None)
		{
			continue;
		}

		for (TActorIterator<AAI_REWorkBenchBase> Iterator(CompanionCharacter->GetWorld()); Iterator; ++Iterator)
		{
			AAI_REWorkBenchBase* Workbench = *Iterator;
			if (!IsValid(Workbench)
				|| Workbench->WorkbenchType != Recipe->RequiredWorkbench
				|| !FAIRECompanionCraftingWorkRequest::IsValidRequestInputs(
					Workbench,
					CraftingRecipeTable,
					RecipeRowId))
			{
				continue;
			}

			const float CandidateDistanceSquared = FVector::DistSquared(
				CompanionCharacter->GetActorLocation(),
				Workbench->GetActorLocation());
			const FString CandidateRowName = RecipeRowId.ToString();
			const FString CandidateWorkbenchPath = Workbench->GetPathName();
			if (BestWorkbench == nullptr
				|| IsCloserCandidate(
					CandidateDistanceSquared,
					CandidateRowName,
					CandidateWorkbenchPath,
					BestDistanceSquared,
					BestRecipeRowId.ToString(),
					BestWorkbenchPath))
			{
				BestRecipeRowId = RecipeRowId;
				BestWorkbench = Workbench;
				BestDistanceSquared = CandidateDistanceSquared;
				BestWorkbenchPath = CandidateWorkbenchPath;
			}
		}
	}

	FGuid WorkOrderId;
	return IsValid(BestWorkbench)
		&& FAIRECompanionCraftingWorkRequest::TryRequest(
			WorkOrderComponent,
			BestWorkbench,
			CraftingRecipeTable,
			BestRecipeRowId,
			WorkOrderId);
}

bool UAIRECompanionTestingBlueprintLibrary::RequestFirstCompanionNearestHarvestWork(
	const UObject* WorldContextObject)
{
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAIRECompanionWorkOrderComponent* WorkOrderComponent = IsValid(CompanionCharacter)
		? CompanionCharacter->GetWorkOrderComponent()
		: nullptr;
	if (!IsValid(CompanionCharacter)
		|| !IsValid(WorkOrderComponent)
		|| WorkOrderComponent->HasActiveWorkOrder())
	{
		return false;
	}

	AAI_REHarvestableResourceActor* BestResource = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	FString BestResourcePath;
	for (TActorIterator<AAI_REHarvestableResourceActor> Iterator(CompanionCharacter->GetWorld()); Iterator; ++Iterator)
	{
		AAI_REHarvestableResourceActor* ResourceActor = *Iterator;
		UAI_REHarvestableResourceComponent* ResourceComponent = IsValid(ResourceActor)
			? ResourceActor->GetHarvestableResourceComponent()
			: nullptr;
		if (!IsValid(ResourceComponent)
			|| ResourceComponent->IsDepleted()
			|| !FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor))
		{
			continue;
		}

		const float CandidateDistanceSquared = FVector::DistSquared(
			CompanionCharacter->GetActorLocation(),
			ResourceActor->GetActorLocation());
		const FString CandidateResourcePath = ResourceActor->GetPathName();
		if (BestResource == nullptr
			|| CandidateDistanceSquared < BestDistanceSquared
			|| (CandidateDistanceSquared == BestDistanceSquared
				&& CandidateResourcePath < BestResourcePath))
		{
			BestResource = ResourceActor;
			BestDistanceSquared = CandidateDistanceSquared;
			BestResourcePath = CandidateResourcePath;
		}
	}

	FGuid WorkOrderId;
	return IsValid(BestResource)
		&& FAIRECompanionHarvestWorkRequest::TryRequest(
			WorkOrderComponent,
			BestResource,
			WorkOrderId);
}

bool UAIRECompanionTestingBlueprintLibrary::CancelFirstCompanionWorkOrder(
	const UObject* WorldContextObject)
{
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAIRECompanionWorkOrderComponent* WorkOrderComponent = IsValid(CompanionCharacter)
		? CompanionCharacter->GetWorkOrderComponent()
		: nullptr;
	if (!IsValid(WorkOrderComponent) || !WorkOrderComponent->HasActiveWorkOrder())
	{
		return false;
	}

	const FAIRECompanionWorkOrderSnapshot WorkOrder = WorkOrderComponent->GetWorkOrderSnapshot();
	return WorkOrder.WorkOrderId.IsValid()
		&& WorkOrderComponent->TryCancelWorkOrder(WorkOrder.WorkOrderId);
}

bool UAIRECompanionTestingBlueprintLibrary::ApplyDamageToFirstCompanion(
	const UObject* WorldContextObject,
	const float DamageAmount)
{
	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return false;
	}

	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAbilitySystemComponent* AbilitySystem = IsValid(CompanionCharacter)
		? CompanionCharacter->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(AbilitySystem))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(CompanionCharacter);
	FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
		UAIRECompanionDamageGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!EffectSpec.IsValid())
	{
		return false;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(AIRECompanionGameplayTags::DataDamage, -DamageAmount);
	const FActiveGameplayEffectHandle AppliedEffect = AbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	return AppliedEffect.WasSuccessfullyApplied();
}

bool UAIRECompanionTestingBlueprintLibrary::ResetFirstCompanionAttributes(const UObject* WorldContextObject)
{
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	return IsValid(CompanionCharacter) && CompanionCharacter->ResetAttributesToConfiguredDefaults();
}

bool UAIRECompanionTestingBlueprintLibrary::LogFirstCompanionAbilityState(const UObject* WorldContextObject)
{
	const AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	if (!IsValid(CompanionCharacter))
	{
		return false;
	}

	const UAIRECompanionAttributeSet* Attributes = CompanionCharacter->GetCompanionAttributeSet();
	const UAbilitySystemComponent* AbilitySystem = CompanionCharacter->GetAbilitySystemComponent();
	if (!IsValid(Attributes) || !IsValid(AbilitySystem))
	{
		return false;
	}

	UE_LOG(
		LogAIRECompanionTesting,
		Log,
		TEXT("Companion GAS state. Companion=%s Health=%.2f/%.2f Stamina=%.2f/%.2f Dead=%s Disabled=%s"),
		*GetNameSafe(CompanionCharacter),
		Attributes->GetHealth(),
		Attributes->GetMaxHealth(),
		Attributes->GetStamina(),
		Attributes->GetMaxStamina(),
		AbilitySystem->HasMatchingGameplayTag(AIRECompanionGameplayTags::StateDisabledDead) ? TEXT("true") : TEXT("false"),
		AbilitySystem->HasMatchingGameplayTag(AIRECompanionGameplayTags::StateDisabled) ? TEXT("true") : TEXT("false"));
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::SeedFirstCompanionInventoryItem(
	const UObject* WorldContextObject,
	const FName ItemId,
	const int32 Count)
{
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAIRECompanionInventoryComponent* Inventory = IsValid(CompanionCharacter)
		? CompanionCharacter->GetInventoryComponent()
		: nullptr;
	return IsValid(Inventory) && Inventory->TryAddItem(ItemId, Count);
}

bool UAIRECompanionTestingBlueprintLibrary::SeedFirstPlayerInventoryItem(
	const UObject* WorldContextObject,
	const FName ItemId,
	const int32 Count)
{
	UAI_REPlayerInventoryComponent* Inventory = FindFirstPlayerInventory(WorldContextObject);
	if (!IsValid(Inventory))
	{
		return false;
	}

	TArray<FInventoryItemStack> NewItems;
	if (!Inventory->BuildExactAddState(ItemId, Count, NewItems))
	{
		return false;
	}
	Inventory->CommitExactInventoryState(MoveTemp(NewItems));
	Inventory->NotifyExactInventoryMutation();
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::TransferFirstInventoryItem(
	const UObject* WorldContextObject,
	const EAIRECompanionTestingInventoryTransferDirection Direction,
	const FName ItemId,
	const int32 Count)
{
	UAIREGameplayInventorySubsystem* InventorySubsystem = FindGameplayInventorySubsystem(WorldContextObject);
	if (!IsValid(InventorySubsystem) || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!InventorySubsystem->GetContainerSnapshot(UAIREGameplayInventorySubsystem::GetMakoContainerId(), MakoSnapshot)
		|| !InventorySubsystem->GetContainerSnapshot(UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(), StorageSnapshot))
	{
		return false;
	}

	if (Direction == EAIRECompanionTestingInventoryTransferDirection::MakoToStorage
		|| Direction == EAIRECompanionTestingInventoryTransferDirection::StorageToMako)
	{
		const FAIREInventoryContainerSnapshot& SourceSnapshot =
			Direction == EAIRECompanionTestingInventoryTransferDirection::MakoToStorage
			? MakoSnapshot
			: StorageSnapshot;
		FAIREInventoryItemStackSnapshot SourceStack;
		if (!FindFirstMatchingStack(SourceSnapshot, ItemId, SourceStack))
		{
			return false;
		}

		FAIREInventoryTransferRequest Request;
		Request.SessionId = InventorySubsystem->GetInventorySessionId();
		Request.MutationId = FGuid::NewGuid();
		Request.SourceContainerId = SourceSnapshot.ContainerId;
		Request.DestinationContainerId =
			Direction == EAIRECompanionTestingInventoryTransferDirection::MakoToStorage
			? StorageSnapshot.ContainerId
			: MakoSnapshot.ContainerId;
		Request.ExpectedSourceRevision = SourceSnapshot.Revision;
		Request.ExpectedDestinationRevision =
			Direction == EAIRECompanionTestingInventoryTransferDirection::MakoToStorage
			? StorageSnapshot.Revision
			: MakoSnapshot.Revision;
		Request.SourceSlotIndex = SourceStack.SlotIndex;
		Request.Count = Count;
		return InventorySubsystem->TryTransferItem(Request).WasApplied();
	}

	UAI_REPlayerInventoryComponent* PlayerInventory = FindFirstPlayerInventory(WorldContextObject);
	if (!IsValid(PlayerInventory))
	{
		return false;
	}

	FAIREPlayerStorageTransferRequest Request;
	Request.SessionId = InventorySubsystem->GetInventorySessionId();
	Request.MutationId = FGuid::NewGuid();
	Request.ExpectedStorageRevision = StorageSnapshot.Revision;
	Request.Count = Count;
	if (Direction == EAIRECompanionTestingInventoryTransferDirection::PlayerToStorage)
	{
		const FInventoryItemStack* SourceStack = PlayerInventory->Items.FindByPredicate(
			[ItemId, PlayerInventory](const FInventoryItemStack& Stack)
			{
				return Stack.SlotIndex >= 0
					&& Stack.SlotIndex < PlayerInventory->MaxSlots
					&& Stack.ItemId == ItemId
					&& Stack.Count > 0;
			});
		if (!SourceStack)
		{
			return false;
		}

		Request.Direction = EAIREPlayerStorageTransferDirection::DepositPlayerToStorage;
		Request.SourceSlotIndex = SourceStack->SlotIndex;
	}
	else
	{
		FAIREInventoryItemStackSnapshot SourceStack;
		if (!FindFirstMatchingStack(StorageSnapshot, ItemId, SourceStack))
		{
			return false;
		}

		Request.Direction = EAIREPlayerStorageTransferDirection::WithdrawStorageToPlayer;
		Request.SourceSlotIndex = SourceStack.SlotIndex;
	}

	return InventorySubsystem->TryTransferPlayerStorage(PlayerInventory, Request).WasApplied();
}

bool UAIRECompanionTestingBlueprintLibrary::LogFirstCompanionInventoryState(
	const UObject* WorldContextObject)
{
	UAIREGameplayInventorySubsystem* InventorySubsystem = FindGameplayInventorySubsystem(WorldContextObject);
	if (!IsValid(InventorySubsystem))
	{
		return false;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!InventorySubsystem->GetContainerSnapshot(UAIREGameplayInventorySubsystem::GetMakoContainerId(), MakoSnapshot)
		|| !InventorySubsystem->GetContainerSnapshot(UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(), StorageSnapshot))
	{
		return false;
	}

	LogInventorySnapshot(TEXT("MAKO"), MakoSnapshot);
	LogInventorySnapshot(TEXT("Shared Storage"), StorageSnapshot);
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::ResetGameplayInventorySession(
	const UObject* WorldContextObject)
{
	UAIREGameplayInventorySubsystem* InventorySubsystem = FindGameplayInventorySubsystem(WorldContextObject);
	return IsValid(InventorySubsystem) && InventorySubsystem->ResetInventorySession().IsValid();
}

#include "Command/AIRECompanionCommandGatewayComponent.h"

#include "AI_RECraftingTypes.h"
#include "AI_REHarvestGameplayTags.h"
#include "AI_REHarvestableResourceActor.h"
#include "AI_REItemDataAsset.h"
#include "AI_REItemSubsystem.h"
#include "AIREGameplayInventoryTypes.h"
#include "Chat/AIRECompanionChatComponent.h"
#include "Core/AIRECompanionAIController.h"
#include "Core/AIRECompanionCharacter.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "Support/AIRECompanionSupportComponent.h"
#include "Threat/AIRECompanionThreatComponent.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "Work/AIRECompanionCraftingWorkRequest.h"
#include "Work/AIRECompanionHarvestableResourceQuery.h"
#include "Work/AIRECompanionHarvestWorkRequest.h"
#include "Work/AIRECompanionWorkbenchQuery.h"
#include "AI_REWorkBenchBase.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionCommandGateway, Log, All);

namespace
{
	constexpr int32 MaxProcessedCommandIds = 256;
	constexpr int32 CommandMaxStableIdLength = 128;
	// Backend와 GameClient가 서로 다른 호스트에서 실행되므로 소규모 NTP 오차를 허용한다.
	// Candidate 자체의 수명과 로컬 만료 검사는 아래에서 별도로 엄격하게 유지한다.
	constexpr double FutureToleranceSeconds = 30.0;
	constexpr double MaxLeaseSeconds = 60.0;
	constexpr float EvaluationPeriodSeconds = 0.1f;
	const FName SupportedCraftRecipeRowId(TEXT("IronSword"));
	const FString SupportedCraftRecipeId(TEXT("recipe-11"));

	bool IsSupportedCraftRecipe(const FAI_RECraftingRecipe& Recipe)
	{
		if (Recipe.ResultItemId != FName(TEXT("Sword_Iron"))
			|| Recipe.ResultAmount != 1
			|| Recipe.RequiredWorkbench != EWorkbenchType::Blacksmith
			|| !FMath::IsNearlyEqual(Recipe.CraftingTime, 3.0f)
			|| Recipe.Ingredients.Num() != 2)
		{
			return false;
		}

		TMap<FName, int32> IngredientTotals;
		for (const FAI_RECraftingIngredient& Ingredient : Recipe.Ingredients)
		{
			if (Ingredient.ItemId.IsNone() || Ingredient.Amount <= 0)
			{
				return false;
			}
			IngredientTotals.FindOrAdd(Ingredient.ItemId) += Ingredient.Amount;
		}
		return IngredientTotals.Num() == 2
			&& IngredientTotals.FindRef(FName(TEXT("IronIngot"))) == 3
			&& IngredientTotals.FindRef(FName(TEXT("WoodHandle"))) == 1;
	}

	bool IsStableCommandId(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > CommandMaxStableIdLength)
		{
			return false;
		}

		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			const bool bIsAllowed = FChar::IsAlnum(Character)
				|| Character == TEXT('.')
				|| Character == TEXT('_')
				|| Character == TEXT(':')
				|| Character == TEXT('-');
			if (!bIsAllowed || (Index == 0 && !FChar::IsAlnum(Character)))
			{
				return false;
			}
		}

		return true;
	}

	bool IsDirectCommandType(const EAIRECommandType Type)
	{
		return Type == EAIRECommandType::Follow
			|| Type == EAIRECommandType::HoldPosition
			|| Type == EAIRECommandType::ReturnToPlayer;
	}

	bool IsWorkOrderCommandType(const EAIRECommandType Type)
	{
		return Type == EAIRECommandType::GatherResource
			|| Type == EAIRECommandType::CraftItem;
	}

	bool IsSameActiveCommand(
		const FAIRECommandCandidate& Candidate,
		const FAIRECommandCandidate& ActiveCandidate)
	{
		return Candidate.CommandId == ActiveCandidate.CommandId
			&& Candidate.RequestId == ActiveCandidate.RequestId;
	}
}

UAIRECompanionCommandGatewayComponent::UAIRECompanionCommandGatewayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UDataTable> RecipeTableFinder(
		TEXT("DataTable'/Game/Work/OBI/Datas/DT_crafting_recipes.DT_crafting_recipes'"));
	if (RecipeTableFinder.Succeeded())
	{
		CraftingRecipeTable = RecipeTableFinder.Object;
	}
}

FAIREDirectCommandSnapshot
UAIRECompanionCommandGatewayComponent::GetDirectCommandSnapshot() const
{
	return DirectCommandSnapshot;
}

bool UAIRECompanionCommandGatewayComponent::CanAdvertiseCraftItem(
	const FAIREWorldContextV1& WorldContext) const
{
	if (!WorldContext.AvailableWorkstations.Contains(
			TEXT("Workbench.Blacksmith"))
		|| !IsValid(CraftingRecipeTable))
	{
		return false;
	}

	const FAI_RECraftingRecipe* Recipe =
		CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(
			SupportedCraftRecipeRowId,
			TEXT("AIRECompanionCommandAdvertisement"),
			false);
	if (Recipe == nullptr || !IsSupportedCraftRecipe(*Recipe))
	{
		return false;
	}

	const AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	UGameInstance* GameInstance = IsValid(Character)
		? Character->GetGameInstance()
		: nullptr;
	UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
		: nullptr;
	if (!IsValid(ItemSubsystem))
	{
		return false;
	}

	const FName RequiredItemIds[] =
	{
		TEXT("IronIngot"),
		TEXT("WoodHandle"),
		TEXT("Sword_Iron"),
	};
	for (const FName ItemId : RequiredItemIds)
	{
		const UAI_REItemDataAsset* Item =
			ItemSubsystem->GetItemDataAsset(ItemId);
		if (!IsValid(Item)
			|| Item->ItemId != ItemId
			|| Item->MaxStackSize < 1)
		{
			return false;
		}
	}
	return true;
}

bool UAIRECompanionCommandGatewayComponent::HasActiveDirectCommand() const
{
	return bHasActiveCommand && DirectCommandSnapshot.bIsActive;
}

void UAIRECompanionCommandGatewayComponent::BeginPlay()
{
	Super::BeginPlay();
	bIsEndingPlay = false;

	AAIRECompanionCharacter* Character = Cast<AAIRECompanionCharacter>(GetOwner());
	if (!IsValid(Character))
	{
		return;
	}

	UAIRECompanionChatComponent* ChatComponent = Character->GetChatComponent();
	if (IsValid(ChatComponent))
	{
		ChatComponent->OnResponseReceived.AddUniqueDynamic(
			this,
			&UAIRECompanionCommandGatewayComponent::HandleChatResponse);
	}

	UAIRECompanionWorkOrderComponent* WorkOrderComponent =
		Character->GetWorkOrderComponent();
	if (IsValid(WorkOrderComponent))
	{
		WorkOrderComponent->OnWorkOrderChanged.AddUniqueDynamic(
			this,
			&UAIRECompanionCommandGatewayComponent::HandleWorkOrderChanged);
	}
}

void UAIRECompanionCommandGatewayComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownGateway();
	Super::EndPlay(EndPlayReason);
}

void UAIRECompanionCommandGatewayComponent::HandleChatResponse(
	const FAIREChatResult& Result)
{
	if (bIsEndingPlay)
	{
		return;
	}

	const int32 CandidateCount = Result.CommandCandidates.Num();
	UE_LOG(
		LogAIRECompanionCommandGateway,
		Log,
		TEXT("Chat command candidates received. RequestId=%s Count=%d"),
		*Result.RequestId,
		CandidateCount);
	if (CandidateCount == 0)
	{
		return;
	}

	if (CandidateCount > 1)
	{
		for (const FAIRECommandCandidate& Candidate : Result.CommandCandidates)
		{
			if (IsStableCommandId(Candidate.CommandId))
			{
				RememberProcessedCommand(Candidate.CommandId);
			}
			RejectCandidate(
				Candidate,
				EAIRECommandResultReason::MultipleCandidatesNotSupported);
		}
		return;
	}

	const FAIRECommandCandidate& Candidate = Result.CommandCandidates[0];
	if (IsStableCommandId(Candidate.CommandId)
		&& IsProcessedCommand(Candidate.CommandId))
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::DuplicateCommand);
		return;
	}

	EAIRECommandResultReason ValidationReason =
		EAIRECommandResultReason::None;
	const bool bIsValid = ValidateCandidate(
		Candidate,
		Result.RequestId,
		ValidationReason);
	if (IsStableCommandId(Candidate.CommandId))
	{
		RememberProcessedCommand(Candidate.CommandId);
	}

	if (!bIsValid)
	{
		RejectCandidate(Candidate, ValidationReason);
		return;
	}

	TryExecuteCandidate(Candidate);
}

void UAIRECompanionCommandGatewayComponent::EvaluateActiveCommand()
{
	if (bIsEndingPlay || !bHasActiveCommand)
	{
		return;
	}

	const FDateTime NowUtc = FDateTime::UtcNow();
	if (NowUtc >= ActiveCandidate.ExpiresAtUtc)
	{
		if (IsWorkOrderCommandType(ActiveCandidate.Type))
		{
			AAIRECompanionCharacter* Character =
				Cast<AAIRECompanionCharacter>(GetOwner());
			UAIRECompanionWorkOrderComponent* WorkOrderComponent =
				IsValid(Character) ? Character->GetWorkOrderComponent() : nullptr;
			const FGuid ExpiredWorkOrderId = ActiveWorkOrderId;
			ActiveWorkOrderId.Invalidate();
			if (IsValid(WorkOrderComponent) && ExpiredWorkOrderId.IsValid())
			{
				WorkOrderComponent->TryCancelWorkOrder(ExpiredWorkOrderId);
			}
			CompleteActiveCommand(
				EAIRECommandResultStatus::Expired,
				EAIRECommandResultReason::None);
		}
		else if (bActiveCommandRunning
			&& (ActiveCandidate.Type == EAIRECommandType::Follow
				|| ActiveCandidate.Type == EAIRECommandType::HoldPosition))
		{
			CompleteActiveCommand(
				EAIRECommandResultStatus::Succeeded,
				EAIRECommandResultReason::LeaseCompleted);
		}
		else
		{
			CompleteActiveCommand(
				EAIRECommandResultStatus::Expired,
				EAIRECommandResultReason::None);
		}
		return;
	}

	if (ActiveCandidate.Type == EAIRECommandType::CraftItem
		&& HasHigherPriorityBehavior())
	{
		CancelActiveCommand(
			EAIRECommandResultReason::PreemptedByLocalBehavior);
		return;
	}

	if (IsDirectCommandType(ActiveCandidate.Type))
	{
		if (HasHigherPriorityBehavior())
		{
			CompleteActiveCommand(
				EAIRECommandResultStatus::Cancelled,
				EAIRECommandResultReason::PreemptedByLocalBehavior);
		}
		return;
	}

	if (ActiveCandidate.Type != EAIRECommandType::Attack)
	{
		return;
	}

	AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	AAIRECompanionAIController* Controller = IsValid(Character)
		? Cast<AAIRECompanionAIController>(Character->GetController())
		: nullptr;
	UAIRECompanionThreatComponent* ThreatComponent = IsValid(Controller)
		? Controller->GetThreatComponent()
		: nullptr;
	AActor* SelectedThreat = IsValid(ThreatComponent)
		? ThreatComponent->GetSelectedThreatTarget()
		: nullptr;
	AActor* ActiveTarget = ActiveAttackTarget.Get();
	if (!IsValid(ActiveTarget)
		|| !ActiveTarget->GetClass()->ImplementsInterface(
			UAIREThreatTargetInterface::StaticClass()))
	{
		CompleteActiveCommand(
			EAIRECommandResultStatus::Failed,
			EAIRECommandResultReason::ThreatTargetLost);
		return;
	}

	if (!IAIREThreatTargetInterface::Execute_IsAliveThreatTarget(ActiveTarget))
	{
		CompleteActiveCommand(
			EAIRECommandResultStatus::Succeeded,
			EAIRECommandResultReason::None);
		return;
	}

	const bool bTargetIsHostile = IsValid(Character)
		&& IAIREThreatTargetInterface::Execute_IsHostileThreatFor(
			ActiveTarget,
			Character);
	if (!IsValid(Character)
		|| !IsValid(ThreatComponent)
		|| !ThreatComponent->IsCombatRequested()
		|| SelectedThreat != ActiveTarget
		|| !bTargetIsHostile)
	{
		CompleteActiveCommand(
			EAIRECommandResultStatus::Failed,
			EAIRECommandResultReason::ThreatTargetLost);
	}
}

bool UAIRECompanionCommandGatewayComponent::ValidateCandidate(
	const FAIRECommandCandidate& Candidate,
	const FString& ExpectedRequestId,
	EAIRECommandResultReason& OutReason) const
{
	OutReason = EAIRECommandResultReason::None;
	if (!IsStableCommandId(Candidate.CommandId)
		|| !IsStableCommandId(Candidate.RequestId)
		|| !IsStableCommandId(ExpectedRequestId))
	{
		OutReason = EAIRECommandResultReason::MalformedCandidate;
		return false;
	}
	if (Candidate.RequestId != ExpectedRequestId)
	{
		OutReason = EAIRECommandResultReason::RequestMismatch;
		return false;
	}

	if (static_cast<uint8>(Candidate.Type)
		> static_cast<uint8>(EAIRECommandType::CraftItem)
		|| static_cast<uint8>(Candidate.Priority)
		> static_cast<uint8>(EAIRECommandPriority::Critical))
	{
		OutReason = EAIRECommandResultReason::InvalidParameters;
		return false;
	}

	const FDateTime NowUtc = FDateTime::UtcNow();
	const FTimespan Lifetime = Candidate.ExpiresAtUtc - Candidate.IssuedAtUtc;
	if (Candidate.IssuedAtUtc >= Candidate.ExpiresAtUtc
		|| Candidate.ExpiresAtUtc <= NowUtc
		|| Candidate.IssuedAtUtc
			> NowUtc + FTimespan::FromSeconds(FutureToleranceSeconds)
		|| Lifetime > FTimespan::FromSeconds(MaxLeaseSeconds))
	{
		UE_LOG(
			LogAIRECompanionCommandGateway,
			Warning,
			TEXT("Command candidate lifetime rejected. CommandId=%s IssuedAt=%s ExpiresAt=%s Now=%s LifetimeSeconds=%.3f"),
			*Candidate.CommandId,
			*Candidate.IssuedAtUtc.ToIso8601(),
			*Candidate.ExpiresAtUtc.ToIso8601(),
			*NowUtc.ToIso8601(),
			Lifetime.GetTotalSeconds());
		OutReason = EAIRECommandResultReason::InvalidLifetime;
		return false;
	}

	const bool bIsUnsupportedExecutionType = Candidate.Type
		== EAIRECommandType::EngageTarget
		|| Candidate.Type == EAIRECommandType::DistractTarget
		|| Candidate.Type == EAIRECommandType::MoveToLocation
		|| Candidate.Type == EAIRECommandType::Switch;
	if (bIsUnsupportedExecutionType)
	{
		return true;
	}

	if (Candidate.bHasUnsupportedParameters)
	{
		OutReason = EAIRECommandResultReason::InvalidParameters;
		return false;
	}

	const bool bHasTargetId = !Candidate.TargetId.IsEmpty()
		|| !Candidate.ParameterTargetId.IsEmpty();
	const bool bHasGatherFields = Candidate.GatherResource
		!= EAIREGatherResourceKind::None
		|| Candidate.bHasGatherQuantity
		|| Candidate.GatherQuantity != 0;
	const bool bHasCraftFields = !Candidate.CraftRecipeId.IsEmpty()
		|| Candidate.bHasCraftQuantity
		|| Candidate.CraftQuantity != 0;

	if (Candidate.Type == EAIRECommandType::GatherResource)
	{
		const bool bResourceIsValid = Candidate.GatherResource
			== EAIREGatherResourceKind::Wood;
		const bool bQuantityIsAbsent = !Candidate.bHasGatherQuantity
			&& Candidate.GatherQuantity == 0;
		if (!bResourceIsValid || !bQuantityIsAbsent || bHasTargetId)
		{
			OutReason = EAIRECommandResultReason::InvalidParameters;
			return false;
		}
		return true;
	}

	if (Candidate.Type == EAIRECommandType::Attack)
	{
		if (bHasGatherFields || bHasCraftFields)
		{
			OutReason = EAIRECommandResultReason::InvalidParameters;
			return false;
		}
		return true;
	}

	if (Candidate.Type == EAIRECommandType::CraftItem)
	{
		if (bHasTargetId
			|| bHasGatherFields
			|| Candidate.CraftRecipeId.IsEmpty()
			|| !Candidate.bHasCraftQuantity
			|| Candidate.CraftQuantity != 1)
		{
			OutReason = EAIRECommandResultReason::InvalidParameters;
			return false;
		}
		return true;
	}

	if (bHasGatherFields || bHasCraftFields)
	{
		OutReason = EAIRECommandResultReason::InvalidParameters;
		return false;
	}

	if (IsDirectCommandType(Candidate.Type)
		|| Candidate.Type == EAIRECommandType::CancelCurrent
		|| Candidate.Type == EAIRECommandType::Switch)
	{
		if (bHasTargetId)
		{
			OutReason = EAIRECommandResultReason::InvalidParameters;
			return false;
		}
	}

	return true;
}

bool UAIRECompanionCommandGatewayComponent::HasHigherPriorityBehavior() const
{
	const AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	if (!IsValid(Character) || Character->IsAbilitySystemDisabled())
	{
		return true;
	}

	const AAIRECompanionAIController* Controller =
		Cast<AAIRECompanionAIController>(Character->GetController());
	if (IsValid(Controller))
	{
		UAIRECompanionThreatComponent* ThreatComponent =
			Controller->GetThreatComponent();
		if (IsValid(ThreatComponent) && ThreatComponent->IsCombatRequested())
		{
			return true;
		}
	}

	const UAIRECompanionSupportComponent* SupportComponent =
		Character->GetSupportComponent();
	return IsValid(SupportComponent)
		&& SupportComponent->IsSupportRequested();
}

bool UAIRECompanionCommandGatewayComponent::TryExecuteCandidate(
	const FAIRECommandCandidate& Candidate)
{
	switch (Candidate.Type)
	{
	case EAIRECommandType::Follow:
	case EAIRECommandType::HoldPosition:
	case EAIRECommandType::ReturnToPlayer:
		return TryExecuteDirectCommand(Candidate);
	case EAIRECommandType::CancelCurrent:
		return TryExecuteCancelCurrent(Candidate);
	case EAIRECommandType::Attack:
		return TryExecuteAttack(Candidate);
	case EAIRECommandType::GatherResource:
		return TryExecuteGatherResource(Candidate);
	case EAIRECommandType::CraftItem:
		return TryExecuteCraftItem(Candidate);
	case EAIRECommandType::EngageTarget:
	case EAIRECommandType::DistractTarget:
	case EAIRECommandType::MoveToLocation:
	case EAIRECommandType::Switch:
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::UnsupportedExecution);
		return false;
	default:
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::InvalidParameters);
		return false;
	}
}

bool UAIRECompanionCommandGatewayComponent::TryExecuteDirectCommand(
	const FAIRECommandCandidate& Candidate)
{
	if (HasHigherPriorityBehavior())
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::HigherPriorityBehaviorActive);
		return false;
	}

	AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	UWorld* World = IsValid(Character) ? Character->GetWorld() : nullptr;
	if (!IsValid(Character) || !IsValid(World))
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::UnsupportedExecution);
		return false;
	}

	if (Candidate.Type == EAIRECommandType::Follow
		|| Candidate.Type == EAIRECommandType::ReturnToPlayer)
	{
		if (!IsValid(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			RejectCandidate(
				Candidate,
				EAIRECommandResultReason::PlayerUnavailable);
			return false;
		}
	}

	if (bHasActiveCommand)
	{
		CancelActiveCommand(EAIRECommandResultReason::ReplacedByNewCommand);
	}

	ActiveCandidate = Candidate;
	ActiveAttackTarget.Reset();
	bHasActiveCommand = true;
	bActiveCommandRunning = false;
	++Generation;
	DirectCommandSnapshot = FAIREDirectCommandSnapshot();
	DirectCommandSnapshot.CommandId = Candidate.CommandId;
	DirectCommandSnapshot.ExpiresAtUtc = Candidate.ExpiresAtUtc;
	DirectCommandSnapshot.Generation = Generation;
	DirectCommandSnapshot.bIsActive = true;
	switch (Candidate.Type)
	{
	case EAIRECommandType::Follow:
		DirectCommandSnapshot.Intent = EAIREDirectCommandIntent::Follow;
		break;
	case EAIRECommandType::HoldPosition:
		DirectCommandSnapshot.Intent = EAIREDirectCommandIntent::HoldPosition;
		break;
	case EAIRECommandType::ReturnToPlayer:
		DirectCommandSnapshot.Intent = EAIREDirectCommandIntent::ReturnToPlayer;
		break;
	default:
		DirectCommandSnapshot.Intent = EAIREDirectCommandIntent::None;
		break;
	}

	BroadcastResult(
		Candidate,
		EAIRECommandResultStatus::Accepted,
		EAIRECommandResultReason::None);
	if (!bHasActiveCommand || !IsSameActiveCommand(Candidate, ActiveCandidate))
	{
		return true;
	}

	World->GetTimerManager().SetTimer(
		EvaluationTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&UAIRECompanionCommandGatewayComponent::EvaluateActiveCommand),
		EvaluationPeriodSeconds,
		true);
	return true;
}

bool UAIRECompanionCommandGatewayComponent::TryExecuteCancelCurrent(
	const FAIRECommandCandidate& Candidate)
{
	AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	UAIRECompanionWorkOrderComponent* WorkOrderComponent =
		IsValid(Character) ? Character->GetWorkOrderComponent() : nullptr;
	if (!IsValid(WorkOrderComponent)
		|| !WorkOrderComponent->HasActiveWorkOrder())
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::WorkOrderUnavailable);
		return false;
	}

	const FAIRECompanionWorkOrderSnapshot Snapshot =
		WorkOrderComponent->GetWorkOrderSnapshot();
	const bool bCancelsTrackedCommand = bHasActiveCommand
		&& IsWorkOrderCommandType(ActiveCandidate.Type)
		&& Snapshot.WorkOrderId.IsValid()
		&& ActiveWorkOrderId.IsValid()
		&& ActiveWorkOrderId == Snapshot.WorkOrderId;
	if (bCancelsTrackedCommand)
	{
		ActiveWorkOrderId.Invalidate();
	}
	if (!Snapshot.WorkOrderId.IsValid()
		|| !WorkOrderComponent->TryCancelWorkOrder(Snapshot.WorkOrderId))
	{
		if (bCancelsTrackedCommand && bHasActiveCommand)
		{
			ActiveWorkOrderId = Snapshot.WorkOrderId;
		}
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::WorkOrderCancellationFailed);
		return false;
	}

	if (bHasActiveCommand)
	{
		CancelActiveCommand(EAIRECommandResultReason::ReplacedByNewCommand);
	}

	BroadcastResult(
		Candidate,
		EAIRECommandResultStatus::Succeeded,
		EAIRECommandResultReason::None);
	return true;
}

bool UAIRECompanionCommandGatewayComponent::TryExecuteAttack(
	const FAIRECommandCandidate& Candidate)
{
	if (!Candidate.TargetId.IsEmpty() || !Candidate.ParameterTargetId.IsEmpty())
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::TargetIdentityUnavailable);
		return false;
	}

	AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	AAIRECompanionAIController* Controller = IsValid(Character)
		? Cast<AAIRECompanionAIController>(Character->GetController())
		: nullptr;
	UAIRECompanionThreatComponent* ThreatComponent = IsValid(Controller)
		? Controller->GetThreatComponent()
		: nullptr;
	AActor* SelectedThreat = IsValid(ThreatComponent)
		? ThreatComponent->GetSelectedThreatTarget()
		: nullptr;
	if (!IsValid(Character)
		|| !IsValid(ThreatComponent)
		|| !ThreatComponent->IsCombatRequested())
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::ThreatUnavailable);
		return false;
	}

	if (!IsValid(SelectedThreat)
		|| SelectedThreat->IsActorBeingDestroyed()
		|| !SelectedThreat->GetClass()->ImplementsInterface(
			UAIREThreatTargetInterface::StaticClass())
		|| !IAIREThreatTargetInterface::Execute_IsAliveThreatTarget(
			SelectedThreat)
		|| !IAIREThreatTargetInterface::Execute_IsHostileThreatFor(
			SelectedThreat,
			Character))
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::ThreatTargetLost);
		return false;
	}

	UWorld* World = Character->GetWorld();
	if (!IsValid(World))
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::UnsupportedExecution);
		return false;
	}

	if (bHasActiveCommand)
	{
		CancelActiveCommand(EAIRECommandResultReason::ReplacedByNewCommand);
	}

	ActiveCandidate = Candidate;
	ActiveAttackTarget = SelectedThreat;
	DirectCommandSnapshot = FAIREDirectCommandSnapshot();
	bHasActiveCommand = true;
	bActiveCommandRunning = true;
	++Generation;

	BroadcastResult(
		Candidate,
		EAIRECommandResultStatus::Accepted,
		EAIRECommandResultReason::None);
	if (!bHasActiveCommand || !IsSameActiveCommand(Candidate, ActiveCandidate))
	{
		return true;
	}

	BroadcastResult(
		Candidate,
		EAIRECommandResultStatus::Running,
		EAIRECommandResultReason::None);
	if (!bHasActiveCommand || !IsSameActiveCommand(Candidate, ActiveCandidate))
	{
		return true;
	}

	World->GetTimerManager().SetTimer(
		EvaluationTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&UAIRECompanionCommandGatewayComponent::EvaluateActiveCommand),
		EvaluationPeriodSeconds,
		true);
	return true;
}

bool UAIRECompanionCommandGatewayComponent::TryExecuteGatherResource(
	const FAIRECommandCandidate& Candidate)
{
	if (Candidate.GatherResource != EAIREGatherResourceKind::Wood)
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::ResourceUnavailable);
		return false;
	}
	if (HasHigherPriorityBehavior())
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::HigherPriorityBehaviorActive);
		return false;
	}

	AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	UAIRECompanionWorkOrderComponent* WorkOrderComponent =
		IsValid(Character) ? Character->GetWorkOrderComponent() : nullptr;
	if (!IsValid(Character)
		|| !IsValid(WorkOrderComponent)
		|| WorkOrderComponent->HasActiveWorkOrder())
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::WorkOrderUnavailable);
		return false;
	}

	AAI_REHarvestableResourceActor* ResourceActor =
		FAIRECompanionHarvestableResourceQuery::FindNearestCompatible(
			*Character,
			AI_REHarvestGameplayTags::Resource_Wood);
	if (!IsValid(ResourceActor))
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::ResourceUnavailable);
		return false;
	}

	FGuid WorkOrderId;
	if (!FAIRECompanionHarvestWorkRequest::TryRequest(
			WorkOrderComponent,
			ResourceActor,
			WorkOrderId))
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::WorkOrderUnavailable);
		return false;
	}

	if (bHasActiveCommand)
	{
		CancelActiveCommand(EAIRECommandResultReason::ReplacedByNewCommand);
	}
	ActiveCandidate = Candidate;
	ActiveAttackTarget.Reset();
	ActiveWorkOrderId = WorkOrderId;
	DirectCommandSnapshot = FAIREDirectCommandSnapshot();
	bHasActiveCommand = true;
	bActiveCommandRunning = false;
	++Generation;
	BroadcastResult(
		Candidate,
		EAIRECommandResultStatus::Accepted,
		EAIRECommandResultReason::None);
	if (!bHasActiveCommand || !IsSameActiveCommand(Candidate, ActiveCandidate))
	{
		return true;
	}

	UWorld* World = Character->GetWorld();
	if (!IsValid(World))
	{
		CancelActiveCommand(EAIRECommandResultReason::OwnerEndingPlay);
		return false;
	}
	World->GetTimerManager().SetTimer(
		EvaluationTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&UAIRECompanionCommandGatewayComponent::EvaluateActiveCommand),
		EvaluationPeriodSeconds,
		true);
	return true;
}

bool UAIRECompanionCommandGatewayComponent::TryExecuteCraftItem(
	const FAIRECommandCandidate& Candidate)
{
	if (Candidate.CraftRecipeId != SupportedCraftRecipeId)
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::RecipeUnavailable);
		return false;
	}
	if (HasHigherPriorityBehavior())
	{
		RejectCandidate(
			Candidate,
			EAIRECommandResultReason::HigherPriorityBehaviorActive);
		return false;
	}

	AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	UAIRECompanionWorkOrderComponent* WorkOrderComponent =
		IsValid(Character) ? Character->GetWorkOrderComponent() : nullptr;
	UAIRECompanionInventoryComponent* InventoryComponent =
		IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
	if (!IsValid(Character)
		|| !IsValid(WorkOrderComponent)
		|| !IsValid(InventoryComponent)
		|| !IsValid(CraftingRecipeTable)
		|| WorkOrderComponent->HasActiveWorkOrder())
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::WorkOrderUnavailable);
		return false;
	}

	const FAI_RECraftingRecipe* Recipe =
		CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(
			SupportedCraftRecipeRowId,
			TEXT("AIRECompanionCommandGateway"),
			false);
	if (Recipe == nullptr || !IsSupportedCraftRecipe(*Recipe))
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::RecipeUnavailable);
		return false;
	}

	AAI_REWorkBenchBase* Workbench =
		FAIRECompanionWorkbenchQuery::FindNearestCompatible(
			*Character,
			Recipe->RequiredWorkbench);
	if (!IsValid(Workbench)
		|| !FAIRECompanionCraftingWorkRequest::IsValidRequestInputs(
			Workbench,
			CraftingRecipeTable,
			SupportedCraftRecipeRowId))
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::WorkbenchUnavailable);
		return false;
	}

	FAIREMakoCraftWorkRequest PreflightRequest;
	FAIREInventoryWorkResult PreflightResult;
	const bool bBuiltPreflightRequest =
		FAIRECompanionCraftingWorkRequest::BuildInventoryWorkRequest(
			*Character,
			*InventoryComponent,
			FGuid::NewGuid(),
			*Recipe,
			false,
			PreflightRequest);
	const bool bCanCompleteCraft = bBuiltPreflightRequest
		&& InventoryComponent->CanCompleteMakoCraftWork(
			PreflightRequest,
			PreflightResult);
	if (!bBuiltPreflightRequest
		|| !bCanCompleteCraft
		|| PreflightResult.Destination
			!= EAIREInventoryWorkResultDestination::Mako)
	{
		UE_LOG(
			LogAIRECompanionCommandGateway,
			Warning,
			TEXT("Craft preflight rejected. CommandId=%s Built=%s CanComplete=%s MutationCode=%d Destination=%d"),
			*Candidate.CommandId,
			bBuiltPreflightRequest ? TEXT("true") : TEXT("false"),
			bCanCompleteCraft ? TEXT("true") : TEXT("false"),
			static_cast<int32>(PreflightResult.Code),
			static_cast<int32>(PreflightResult.Destination));
		RejectCandidate(Candidate, EAIRECommandResultReason::MaterialsUnavailable);
		return false;
	}

	FGuid WorkOrderId;
	if (!FAIRECompanionCraftingWorkRequest::TryRequest(
			WorkOrderComponent,
			Workbench,
			CraftingRecipeTable,
			SupportedCraftRecipeRowId,
			WorkOrderId,
			true))
	{
		RejectCandidate(Candidate, EAIRECommandResultReason::WorkOrderUnavailable);
		return false;
	}

	if (bHasActiveCommand)
	{
		CancelActiveCommand(EAIRECommandResultReason::ReplacedByNewCommand);
	}
	ActiveCandidate = Candidate;
	ActiveAttackTarget.Reset();
	ActiveWorkOrderId = WorkOrderId;
	DirectCommandSnapshot = FAIREDirectCommandSnapshot();
	bHasActiveCommand = true;
	bActiveCommandRunning = false;
	++Generation;

	BroadcastResult(
		Candidate,
		EAIRECommandResultStatus::Accepted,
		EAIRECommandResultReason::None);
	if (!bHasActiveCommand || !IsSameActiveCommand(Candidate, ActiveCandidate))
	{
		return true;
	}

	UWorld* World = Character->GetWorld();
	if (!IsValid(World))
	{
		CancelActiveCommand(EAIRECommandResultReason::OwnerEndingPlay);
		return false;
	}
	World->GetTimerManager().SetTimer(
		EvaluationTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&UAIRECompanionCommandGatewayComponent::EvaluateActiveCommand),
		EvaluationPeriodSeconds,
		true);
	return true;
}

void UAIRECompanionCommandGatewayComponent::HandleWorkOrderChanged(
	const FAIRECompanionWorkOrderSnapshot PreviousSnapshot,
	const FAIRECompanionWorkOrderSnapshot CurrentSnapshot)
{
	(void)PreviousSnapshot;
	if (!bHasActiveCommand
		|| !IsWorkOrderCommandType(ActiveCandidate.Type)
		|| !ActiveWorkOrderId.IsValid()
		|| CurrentSnapshot.WorkOrderId != ActiveWorkOrderId)
	{
		return;
	}

	switch (CurrentSnapshot.State)
	{
	case EAIRECompanionWorkOrderState::Moving:
	case EAIRECompanionWorkOrderState::Working:
	case EAIRECompanionWorkOrderState::PausedByCombat:
		if (!bActiveCommandRunning)
		{
			bActiveCommandRunning = true;
			BroadcastResult(
				ActiveCandidate,
				EAIRECommandResultStatus::Running,
				EAIRECommandResultReason::None);
		}
		break;
	case EAIRECompanionWorkOrderState::Completed:
		CompleteActiveCommand(
			EAIRECommandResultStatus::Succeeded,
			EAIRECommandResultReason::None);
		break;
	case EAIRECompanionWorkOrderState::Cancelled:
		CompleteActiveCommand(
			EAIRECommandResultStatus::Cancelled,
			EAIRECommandResultReason::None);
		break;
	case EAIRECompanionWorkOrderState::Failed:
		CompleteActiveCommand(
			EAIRECommandResultStatus::Failed,
			EAIRECommandResultReason::WorkOrderFailed);
		break;
	case EAIRECompanionWorkOrderState::None:
	case EAIRECompanionWorkOrderState::Requested:
	default:
		break;
	}
}

bool UAIRECompanionCommandGatewayComponent::ReportDirectCommandRunning(
	const FString& CommandId,
	const int64 InGeneration)
{
	if (!bHasActiveCommand
		|| !DirectCommandSnapshot.bIsActive
		|| ActiveCandidate.CommandId != CommandId
		|| DirectCommandSnapshot.Generation != InGeneration)
	{
		return false;
	}
	if (HasHigherPriorityBehavior())
	{
		CompleteActiveCommand(
			EAIRECommandResultStatus::Cancelled,
			EAIRECommandResultReason::PreemptedByLocalBehavior);
		return false;
	}
	if (!bActiveCommandRunning)
	{
		bActiveCommandRunning = true;
		const FAIRECommandCandidate RunningCandidate = ActiveCandidate;
		BroadcastResult(
			RunningCandidate,
			EAIRECommandResultStatus::Running,
			EAIRECommandResultReason::None);
		return bHasActiveCommand
			&& DirectCommandSnapshot.bIsActive
			&& ActiveCandidate.CommandId == CommandId
			&& DirectCommandSnapshot.Generation == InGeneration;
	}
	return true;
}

bool UAIRECompanionCommandGatewayComponent::ReportDirectCommandSucceeded(
	const FString& CommandId,
	const int64 InGeneration,
	const EAIRECommandResultReason Reason)
{
	if (!bHasActiveCommand
		|| !DirectCommandSnapshot.bIsActive
		|| !bActiveCommandRunning
		|| ActiveCandidate.CommandId != CommandId
		|| DirectCommandSnapshot.Generation != InGeneration)
	{
		return false;
	}

	if (FDateTime::UtcNow() >= DirectCommandSnapshot.ExpiresAtUtc)
	{
		EvaluateActiveCommand();
		return false;
	}

	CompleteActiveCommand(EAIRECommandResultStatus::Succeeded, Reason);
	return true;
}

bool UAIRECompanionCommandGatewayComponent::ReportDirectCommandFailed(
	const FString& CommandId,
	const int64 InGeneration,
	const EAIRECommandResultReason Reason)
{
	if (!bHasActiveCommand
		|| !DirectCommandSnapshot.bIsActive
		|| ActiveCandidate.CommandId != CommandId
		|| DirectCommandSnapshot.Generation != InGeneration)
	{
		return false;
	}

	if (FDateTime::UtcNow() >= DirectCommandSnapshot.ExpiresAtUtc)
	{
		EvaluateActiveCommand();
		return false;
	}

	CompleteActiveCommand(EAIRECommandResultStatus::Failed, Reason);
	return true;
}

bool UAIRECompanionCommandGatewayComponent::ReportDirectCommandPreempted(
	const FString& CommandId,
	const int64 InGeneration)
{
	if (!bHasActiveCommand
		|| !DirectCommandSnapshot.bIsActive
		|| ActiveCandidate.CommandId != CommandId
		|| DirectCommandSnapshot.Generation != InGeneration)
	{
		return false;
	}

	if (FDateTime::UtcNow() >= DirectCommandSnapshot.ExpiresAtUtc)
	{
		EvaluateActiveCommand();
		return false;
	}

	CompleteActiveCommand(
		EAIRECommandResultStatus::Cancelled,
		EAIRECommandResultReason::PreemptedByLocalBehavior);
	return true;
}

void UAIRECompanionCommandGatewayComponent::RejectCandidate(
	const FAIRECommandCandidate& Candidate,
	const EAIRECommandResultReason Reason)
{
	BroadcastResult(
		Candidate,
		EAIRECommandResultStatus::Rejected,
		Reason);
}

void UAIRECompanionCommandGatewayComponent::CompleteActiveCommand(
	const EAIRECommandResultStatus Status,
	const EAIRECommandResultReason Reason)
{
	if (!bHasActiveCommand)
	{
		return;
	}

	const FAIRECommandCandidate CompletedCandidate = ActiveCandidate;
	ClearEvaluationTimer();
	++Generation;
	bHasActiveCommand = false;
	bActiveCommandRunning = false;
	ActiveCandidate = FAIRECommandCandidate();
	DirectCommandSnapshot = FAIREDirectCommandSnapshot();
	ActiveAttackTarget.Reset();
	ActiveWorkOrderId.Invalidate();
	BroadcastResult(CompletedCandidate, Status, Reason);
}

void UAIRECompanionCommandGatewayComponent::CancelActiveCommand(
	const EAIRECommandResultReason Reason)
{
	if (bHasActiveCommand)
	{
		if (IsWorkOrderCommandType(ActiveCandidate.Type)
			&& ActiveWorkOrderId.IsValid())
		{
			AAIRECompanionCharacter* Character =
				Cast<AAIRECompanionCharacter>(GetOwner());
			UAIRECompanionWorkOrderComponent* WorkOrderComponent =
				IsValid(Character) ? Character->GetWorkOrderComponent() : nullptr;
			const FGuid WorkOrderId = ActiveWorkOrderId;
			ActiveWorkOrderId.Invalidate();
			if (IsValid(WorkOrderComponent))
			{
				WorkOrderComponent->TryCancelWorkOrder(WorkOrderId);
			}
		}
		CompleteActiveCommand(EAIRECommandResultStatus::Cancelled, Reason);
	}
}

void UAIRECompanionCommandGatewayComponent::BroadcastResult(
	const FAIRECommandCandidate& Candidate,
	const EAIRECommandResultStatus Status,
	const EAIRECommandResultReason Reason)
{
	FAIRECommandResult Result;
	Result.CommandId = Candidate.CommandId;
	Result.RequestId = Candidate.RequestId;
	Result.Type = Candidate.Type;
	Result.Status = Status;
	Result.Reason = Reason;
	UE_LOG(
		LogAIRECompanionCommandGateway,
		Log,
		TEXT("Command result. RequestId=%s CommandId=%s Type=%d Status=%d Reason=%d"),
		*Result.RequestId,
		*Result.CommandId,
		static_cast<int32>(Result.Type),
		static_cast<int32>(Result.Status),
		static_cast<int32>(Result.Reason));
	OnCommandResult.Broadcast(Result);
}

void UAIRECompanionCommandGatewayComponent::RememberProcessedCommand(
	const FString& CommandId)
{
	if (!IsStableCommandId(CommandId)
		|| ProcessedCommandIds.Contains(CommandId))
	{
		return;
	}

	ProcessedCommandIds.Add(CommandId);
	ProcessedCommandOrder.Add(CommandId);
	while (ProcessedCommandOrder.Num() > MaxProcessedCommandIds)
	{
		const FString OldestCommandId = ProcessedCommandOrder[0];
		ProcessedCommandOrder.RemoveAt(0);
		ProcessedCommandIds.Remove(OldestCommandId);
	}
}

bool UAIRECompanionCommandGatewayComponent::IsProcessedCommand(
	const FString& CommandId) const
{
	return IsStableCommandId(CommandId)
		&& ProcessedCommandIds.Contains(CommandId);
}

void UAIRECompanionCommandGatewayComponent::ClearEvaluationTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
	}
	EvaluationTimerHandle.Invalidate();
}

void UAIRECompanionCommandGatewayComponent::ShutdownGateway()
{
	if (bIsEndingPlay)
	{
		return;
	}

	bIsEndingPlay = true;
	if (bHasActiveCommand)
	{
		CancelActiveCommand(EAIRECommandResultReason::OwnerEndingPlay);
	}
	ClearEvaluationTimer();

	AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	UAIRECompanionChatComponent* ChatComponent = IsValid(Character)
		? Character->GetChatComponent()
		: nullptr;
	if (IsValid(ChatComponent))
	{
		ChatComponent->OnResponseReceived.RemoveDynamic(
			this,
			&UAIRECompanionCommandGatewayComponent::HandleChatResponse);
	}
	UAIRECompanionWorkOrderComponent* WorkOrderComponent =
		IsValid(Character) ? Character->GetWorkOrderComponent() : nullptr;
	if (IsValid(WorkOrderComponent))
	{
		WorkOrderComponent->OnWorkOrderChanged.RemoveDynamic(
			this,
			&UAIRECompanionCommandGatewayComponent::HandleWorkOrderChanged);
	}

	ProcessedCommandIds.Reset();
	ProcessedCommandOrder.Reset();
	ActiveCandidate = FAIRECommandCandidate();
	DirectCommandSnapshot = FAIREDirectCommandSnapshot();
	ActiveAttackTarget.Reset();
	ActiveWorkOrderId.Invalidate();
	bActiveCommandRunning = false;
	OnCommandResult.Clear();
}

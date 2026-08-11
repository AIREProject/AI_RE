#include "Command/AIRECompanionCommandGatewayComponent.h"

#include "Chat/AIRECompanionChatComponent.h"
#include "Core/AIRECompanionAIController.h"
#include "Core/AIRECompanionCharacter.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "Support/AIRECompanionSupportComponent.h"
#include "Threat/AIRECompanionThreatComponent.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 MaxProcessedCommandIds = 256;
	constexpr int32 MaxStableIdLength = 128;
	constexpr int32 MaxGatherQuantity = 50;
	constexpr double FutureToleranceSeconds = 2.0;
	constexpr double MaxLeaseSeconds = 60.0;
	constexpr float EvaluationPeriodSeconds = 0.1f;

	bool IsStableCommandId(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > MaxStableIdLength)
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
}

FAIREDirectCommandSnapshot
UAIRECompanionCommandGatewayComponent::GetDirectCommandSnapshot() const
{
	return DirectCommandSnapshot;
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
		if (bActiveCommandRunning
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
		> static_cast<uint8>(EAIRECommandType::Switch)
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

	if (Candidate.Type == EAIRECommandType::GatherResource)
	{
		const bool bResourceIsValid = Candidate.GatherResource
			== EAIREGatherResourceKind::Wood
			|| Candidate.GatherResource == EAIREGatherResourceKind::Stone;
		const bool bQuantityIsValid = !Candidate.bHasGatherQuantity
			? Candidate.GatherQuantity == 0
			: Candidate.GatherQuantity > 0
				&& Candidate.GatherQuantity <= MaxGatherQuantity;
		if (!bResourceIsValid || !bQuantityIsValid || bHasTargetId)
		{
			OutReason = EAIRECommandResultReason::InvalidParameters;
			return false;
		}
		return true;
	}

	if (Candidate.Type == EAIRECommandType::Attack)
	{
		if (bHasGatherFields)
		{
			OutReason = EAIRECommandResultReason::InvalidParameters;
			return false;
		}
		return true;
	}

	if (bHasGatherFields)
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
	case EAIRECommandType::EngageTarget:
	case EAIRECommandType::DistractTarget:
	case EAIRECommandType::MoveToLocation:
	case EAIRECommandType::GatherResource:
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
	if (!Snapshot.WorkOrderId.IsValid()
		|| !WorkOrderComponent->TryCancelWorkOrder(Snapshot.WorkOrderId))
	{
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
	BroadcastResult(CompletedCandidate, Status, Reason);
}

void UAIRECompanionCommandGatewayComponent::CancelActiveCommand(
	const EAIRECommandResultReason Reason)
{
	if (bHasActiveCommand)
	{
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
		CompleteActiveCommand(
			EAIRECommandResultStatus::Cancelled,
			EAIRECommandResultReason::OwnerEndingPlay);
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

	ProcessedCommandIds.Reset();
	ProcessedCommandOrder.Reset();
	ActiveCandidate = FAIRECommandCandidate();
	DirectCommandSnapshot = FAIREDirectCommandSnapshot();
	ActiveAttackTarget.Reset();
	bActiveCommandRunning = false;
	OnCommandResult.Clear();
}

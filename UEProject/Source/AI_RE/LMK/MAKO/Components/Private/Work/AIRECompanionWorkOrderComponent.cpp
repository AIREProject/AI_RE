#include "Work/AIRECompanionWorkOrderComponent.h"

#include "AI_REHarvestableResourceActor.h"
#include "AI_REWorkBenchBase.h"
#include "AIRESharedStorageActor.h"
#include "GameFramework/Actor.h"

UAIRECompanionWorkOrderComponent::UAIRECompanionWorkOrderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UAIRECompanionWorkOrderComponent::TryRequestWorkOrder(
	const FAIRECompanionWorkOrderRequest& Request,
	FGuid& OutWorkOrderId)
{
	OutWorkOrderId = FGuid();
	AActor* TargetActor = Request.TargetActor.Get();
	const bool bHasValidTypedPayload =
		(Request.WorkType == EAIRECompanionWorkOrderType::Crafting
			&& IsValid(Request.RecipeTable)
			&& !Request.RecipeRowId.IsNone()
			&& IsValid(Cast<AAI_REWorkBenchBase>(TargetActor)))
		|| (Request.WorkType == EAIRECompanionWorkOrderType::Harvesting
			&& !IsValid(Request.RecipeTable)
			&& Request.RecipeRowId.IsNone()
			&& !Request.bRequireMakoDestination
			&& IsValid(Cast<AAI_REHarvestableResourceActor>(TargetActor)))
		|| (Request.WorkType
				== EAIRECompanionWorkOrderType::StorageTransfer
			&& !IsValid(Request.RecipeTable)
			&& Request.RecipeRowId.IsNone()
			&& !Request.bRequireMakoDestination
			&& Request.StorageTransfer.RequestSessionId.IsValid()
			&& !Request.StorageTransfer.ItemId.IsNone()
			&& Request.StorageTransfer.Count > 0
			&& (Request.StorageTransfer.Direction
					== EAIRECompanionStorageTransferDirection::DepositMakoToStorage
				|| Request.StorageTransfer.Direction
					== EAIRECompanionStorageTransferDirection::WithdrawStorageToMako)
			&& IsValid(Cast<AAIRESharedStorageActor>(TargetActor)));
	if (bIsShuttingDown
		|| !IsValid(TargetActor)
		|| TargetActor->IsActorBeingDestroyed()
		|| !bHasValidTypedPayload
		|| HasActiveWorkOrder()
		|| (CurrentWorkOrder.State
				!= EAIRECompanionWorkOrderState::None
			&& !IsTerminalState(CurrentWorkOrder.State)))
	{
		return false;
	}

	const FGuid NewWorkOrderId = FGuid::NewGuid();
	if (!NewWorkOrderId.IsValid())
	{
		return false;
	}

	const FAIRECompanionWorkOrderSnapshot PreviousSnapshot =
		CurrentWorkOrder;
	CurrentWorkOrder.WorkOrderId = NewWorkOrderId;
	CurrentWorkOrder.TargetActor = TargetActor;
	CurrentWorkOrder.WorkType = Request.WorkType;
	CurrentWorkOrder.RecipeTable = Request.RecipeTable;
	CurrentWorkOrder.RecipeRowId = Request.RecipeRowId;
	CurrentWorkOrder.bRequireMakoDestination =
		Request.bRequireMakoDestination;
	CurrentWorkOrder.StorageTransfer = Request.StorageTransfer;
	CurrentWorkOrder.State = EAIRECompanionWorkOrderState::Requested;
	BindTargetDestroyed();

	OutWorkOrderId = NewWorkOrderId;
	OnWorkOrderChanged.Broadcast(PreviousSnapshot, CurrentWorkOrder);
	return true;
}

bool UAIRECompanionWorkOrderComponent::TryStartMoving(
	const FGuid& WorkOrderId)
{
	return TryTransitionTo(
		WorkOrderId,
		EAIRECompanionWorkOrderState::Moving);
}

bool UAIRECompanionWorkOrderComponent::TryStartWorking(
	const FGuid& WorkOrderId)
{
	return TryTransitionTo(
		WorkOrderId,
		EAIRECompanionWorkOrderState::Working);
}

bool UAIRECompanionWorkOrderComponent::TryPauseForCombat(
	const FGuid& WorkOrderId)
{
	return TryTransitionTo(
		WorkOrderId,
		EAIRECompanionWorkOrderState::PausedByCombat);
}

bool UAIRECompanionWorkOrderComponent::TryResumeAfterCombat(
	const FGuid& WorkOrderId)
{
	return TryTransitionTo(
		WorkOrderId,
		EAIRECompanionWorkOrderState::Requested);
}

bool UAIRECompanionWorkOrderComponent::TryCompleteWorkOrder(
	const FGuid& WorkOrderId)
{
	return TryTransitionTo(
		WorkOrderId,
		EAIRECompanionWorkOrderState::Completed);
}

bool UAIRECompanionWorkOrderComponent::TryCancelWorkOrder(
	const FGuid& WorkOrderId)
{
	return TryTransitionTo(
		WorkOrderId,
		EAIRECompanionWorkOrderState::Cancelled);
}

bool UAIRECompanionWorkOrderComponent::TryFailWorkOrder(
	const FGuid& WorkOrderId)
{
	return TryTransitionTo(
		WorkOrderId,
		EAIRECompanionWorkOrderState::Failed);
}

void UAIRECompanionWorkOrderComponent::ShutdownWorkOrder()
{
	if (bIsShuttingDown)
	{
		return;
	}

	const bool bHadActiveWorkOrder = HasActiveWorkOrder();
	bIsShuttingDown = true;
	UnbindTargetDestroyed();
	if (bHadActiveWorkOrder)
	{
		CurrentWorkOrder.State = EAIRECompanionWorkOrderState::Failed;
	}
	OnWorkOrderChanged.Clear();
}

FAIRECompanionWorkOrderSnapshot
UAIRECompanionWorkOrderComponent::GetWorkOrderSnapshot() const
{
	return CurrentWorkOrder;
}

bool UAIRECompanionWorkOrderComponent::HasActiveWorkOrder() const
{
	return CurrentWorkOrder.WorkOrderId.IsValid()
		&& IsActiveState(CurrentWorkOrder.State);
}

void UAIRECompanionWorkOrderComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownWorkOrder();
	Super::EndPlay(EndPlayReason);
}

bool UAIRECompanionWorkOrderComponent::IsActiveState(
	const EAIRECompanionWorkOrderState State)
{
	return State == EAIRECompanionWorkOrderState::Requested
		|| State == EAIRECompanionWorkOrderState::Moving
		|| State == EAIRECompanionWorkOrderState::Working
		|| State == EAIRECompanionWorkOrderState::PausedByCombat;
}

bool UAIRECompanionWorkOrderComponent::IsTerminalState(
	const EAIRECompanionWorkOrderState State)
{
	return State == EAIRECompanionWorkOrderState::Completed
		|| State == EAIRECompanionWorkOrderState::Cancelled
		|| State == EAIRECompanionWorkOrderState::Failed;
}

bool UAIRECompanionWorkOrderComponent::CanTransition(
	const EAIRECompanionWorkOrderState CurrentState,
	const EAIRECompanionWorkOrderState NewState)
{
	switch (NewState)
	{
	case EAIRECompanionWorkOrderState::Requested:
		return CurrentState
			== EAIRECompanionWorkOrderState::PausedByCombat;
	case EAIRECompanionWorkOrderState::Moving:
		return CurrentState
			== EAIRECompanionWorkOrderState::Requested;
	case EAIRECompanionWorkOrderState::Working:
		return CurrentState
			== EAIRECompanionWorkOrderState::Moving;
	case EAIRECompanionWorkOrderState::PausedByCombat:
		return CurrentState == EAIRECompanionWorkOrderState::Moving
			|| CurrentState == EAIRECompanionWorkOrderState::Working;
	case EAIRECompanionWorkOrderState::Completed:
		return CurrentState
			== EAIRECompanionWorkOrderState::Working;
	case EAIRECompanionWorkOrderState::Cancelled:
	case EAIRECompanionWorkOrderState::Failed:
		return IsActiveState(CurrentState);
	case EAIRECompanionWorkOrderState::None:
	default:
		return false;
	}
}

bool UAIRECompanionWorkOrderComponent::IsCurrentTargetUsable() const
{
	const AActor* TargetActor = CurrentWorkOrder.TargetActor.Get();
	return IsValid(TargetActor) && !TargetActor->IsActorBeingDestroyed();
}

bool UAIRECompanionWorkOrderComponent::TryTransitionTo(
	const FGuid& WorkOrderId,
	const EAIRECompanionWorkOrderState NewState)
{
	if (bIsShuttingDown
		|| !WorkOrderId.IsValid()
		|| CurrentWorkOrder.WorkOrderId != WorkOrderId
		|| !IsActiveState(CurrentWorkOrder.State)
		|| !CanTransition(CurrentWorkOrder.State, NewState))
	{
		return false;
	}

	const bool bRequiresUsableTarget =
		NewState != EAIRECompanionWorkOrderState::Cancelled
		&& NewState != EAIRECompanionWorkOrderState::Failed;
	if (bRequiresUsableTarget && !IsCurrentTargetUsable())
	{
		ApplyState(EAIRECompanionWorkOrderState::Failed);
		return false;
	}

	ApplyState(NewState);
	return true;
}

void UAIRECompanionWorkOrderComponent::ApplyState(
	const EAIRECompanionWorkOrderState NewState)
{
	const FAIRECompanionWorkOrderSnapshot PreviousSnapshot =
		CurrentWorkOrder;
	if (IsTerminalState(NewState))
	{
		UnbindTargetDestroyed();
	}

	CurrentWorkOrder.State = NewState;
	if (!bIsShuttingDown)
	{
		OnWorkOrderChanged.Broadcast(PreviousSnapshot, CurrentWorkOrder);
	}
}

void UAIRECompanionWorkOrderComponent::BindTargetDestroyed()
{
	if (AActor* TargetActor = CurrentWorkOrder.TargetActor.Get();
		IsValid(TargetActor))
	{
		TargetActor->OnDestroyed.AddUniqueDynamic(
			this,
			&UAIRECompanionWorkOrderComponent::HandleTargetDestroyed);
	}
}

void UAIRECompanionWorkOrderComponent::UnbindTargetDestroyed()
{
	if (AActor* TargetActor = CurrentWorkOrder.TargetActor.Get();
		IsValid(TargetActor))
	{
		TargetActor->OnDestroyed.RemoveDynamic(
			this,
			&UAIRECompanionWorkOrderComponent::HandleTargetDestroyed);
	}
}

void UAIRECompanionWorkOrderComponent::HandleTargetDestroyed(
	AActor* DestroyedActor)
{
	if (bIsShuttingDown
		|| CurrentWorkOrder.TargetActor.Get() != DestroyedActor
		|| !HasActiveWorkOrder())
	{
		return;
	}

	TryFailWorkOrder(CurrentWorkOrder.WorkOrderId);
}

#include "Work/AIRECompanionStorageWorkRequest.h"

#include "AIRESharedStorageActor.h"
#include "Work/AIRECompanionWorkOrderComponent.h"

bool FAIRECompanionStorageWorkRequest::TryRequest(
	UAIRECompanionWorkOrderComponent* WorkOrderComponent,
	AAIRESharedStorageActor* StorageActor,
	const FGuid RequestSessionId,
	const EAIRECompanionStorageTransferDirection Direction,
	const FName ItemId,
	const int32 Count,
	FGuid& OutWorkOrderId)
{
	OutWorkOrderId.Invalidate();
	const bool bIsSupportedDirection =
		Direction
			== EAIRECompanionStorageTransferDirection::DepositMakoToStorage
		|| Direction
			== EAIRECompanionStorageTransferDirection::WithdrawStorageToMako;
	if (!IsValid(WorkOrderComponent)
		|| !IsValid(StorageActor)
		|| StorageActor->IsActorBeingDestroyed()
		|| !RequestSessionId.IsValid()
		|| !bIsSupportedDirection
		|| ItemId.IsNone()
		|| Count <= 0)
	{
		return false;
	}

	FAIRECompanionWorkOrderRequest Request;
	Request.WorkType = EAIRECompanionWorkOrderType::StorageTransfer;
	Request.TargetActor = StorageActor;
	Request.StorageTransfer.RequestSessionId = RequestSessionId;
	Request.StorageTransfer.Direction = Direction;
	Request.StorageTransfer.ItemId = ItemId;
	Request.StorageTransfer.Count = Count;
	return WorkOrderComponent->TryRequestWorkOrder(Request, OutWorkOrderId);
}

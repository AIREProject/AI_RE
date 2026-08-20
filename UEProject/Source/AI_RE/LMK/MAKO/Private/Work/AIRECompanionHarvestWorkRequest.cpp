#include "Work/AIRECompanionHarvestWorkRequest.h"

#include "AI_REHarvestableResourceActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "AI_REItemActor.h"
#include "AI_REItemDataAsset.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "Work/AIRECompanionWorkOrderTypes.h"

bool FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(
	const AAI_REHarvestableResourceActor* ResourceActor)
{
	if (!IsValid(ResourceActor) || ResourceActor->IsActorBeingDestroyed())
	{
		return false;
	}

	const UAI_REHarvestableResourceComponent* ResourceComponent =
		ResourceActor->GetHarvestableResourceComponent();
	if (!IsValid(ResourceComponent)
		|| ResourceComponent->IsDepleted()
		|| !FMath::IsFinite(ResourceComponent->GetCurrentHealth())
		|| ResourceComponent->GetCurrentHealth() <= 0.0f)
	{
		return false;
	}

	const UAI_REItemDataAsset* RewardItemAsset = ResourceComponent->GetRewardItemAsset();
	return ResourceActor->ItemActorClass != nullptr
		&& IsValid(RewardItemAsset)
		&& !RewardItemAsset->ItemId.IsNone()
		&& ResourceComponent->GetRewardAmount() > 0
		&& FMath::IsFinite(ResourceComponent->GetRewardDamageInterval())
		&& ResourceComponent->GetRewardDamageInterval() >= 0.0f;
}

bool FAIRECompanionHarvestWorkRequest::TryRequest(
	UAIRECompanionWorkOrderComponent* WorkOrderComponent,
	AAI_REHarvestableResourceActor* ResourceActor,
	FGuid& OutWorkOrderId)
{
	OutWorkOrderId.Invalidate();
	if (!IsValid(WorkOrderComponent)
		|| WorkOrderComponent->HasActiveWorkOrder()
		|| !IsValidRequestInputs(ResourceActor))
	{
		return false;
	}

	FAIRECompanionWorkOrderRequest Request;
	Request.WorkType = EAIRECompanionWorkOrderType::Harvesting;
	Request.TargetActor = ResourceActor;
	return WorkOrderComponent->TryRequestWorkOrder(Request, OutWorkOrderId);
}

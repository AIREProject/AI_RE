#pragma once

#include "CoreMinimal.h"

class AAI_REHarvestableResourceActor;
class UAIRECompanionWorkOrderComponent;

class AI_RE_API FAIRECompanionHarvestWorkRequest final
{
public:
	static bool IsValidRequestInputs(const AAI_REHarvestableResourceActor* ResourceActor);

	static bool TryRequest(
		UAIRECompanionWorkOrderComponent* WorkOrderComponent,
		AAI_REHarvestableResourceActor* ResourceActor,
		FGuid& OutWorkOrderId);
};

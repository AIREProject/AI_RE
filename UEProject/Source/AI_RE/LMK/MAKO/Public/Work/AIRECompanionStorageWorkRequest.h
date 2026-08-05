#pragma once

#include "CoreMinimal.h"

class AAIRESharedStorageActor;
class UAIRECompanionWorkOrderComponent;
enum class EAIRECompanionStorageTransferDirection : uint8;

class AI_RE_API FAIRECompanionStorageWorkRequest
{
public:
	static bool TryRequest(
		UAIRECompanionWorkOrderComponent* WorkOrderComponent,
		AAIRESharedStorageActor* StorageActor,
		FGuid RequestSessionId,
		EAIRECompanionStorageTransferDirection Direction,
		FName ItemId,
		int32 Count,
		FGuid& OutWorkOrderId);
};

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AIREHarvestRewardReceiver.generated.h"

UINTERFACE(BlueprintType)
class AI_RE_API UAIREHarvestRewardReceiver : public UInterface
{
	GENERATED_BODY()
};

class AI_RE_API IAIREHarvestRewardReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AI_RE|Harvest")
	bool TryReceiveHarvestReward(FGuid DeliveryId, FName ItemId, int32 Count);
};

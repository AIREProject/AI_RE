#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AI_REHarvestDamageTarget.generated.h"

UINTERFACE(BlueprintType)
class AI_RE_API UAI_REHarvestDamageTarget : public UInterface
{
	GENERATED_BODY()
};

class AI_RE_API IAI_REHarvestDamageTarget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AI_RE|Harvest")
	bool ApplyHarvestDamage(float DamageAmount, AActor* InstigatorActor);
};

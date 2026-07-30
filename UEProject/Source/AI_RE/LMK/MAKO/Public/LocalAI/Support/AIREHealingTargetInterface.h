#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "UObject/Interface.h"
#include "AIREHealingTargetInterface.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class AI_RE_API UAIREHealingTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class AI_RE_API IAIREHealingTargetInterface
{
	GENERATED_BODY()

public:
	virtual FGameplayAttribute GetHealingHealthAttribute() const = 0;
	virtual FGameplayAttribute GetHealingMaxHealthAttribute() const = 0;
	virtual bool CanReceiveHealingFrom(const AActor* Healer) const = 0;
};

namespace AIREHealingTarget
{
	AI_RE_API bool GetMissingHealth(
		const AActor* TargetActor,
		const AActor* Healer,
		float& OutMissingHealth);
}

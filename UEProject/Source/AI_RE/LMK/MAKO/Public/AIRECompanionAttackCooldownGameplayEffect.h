#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AIRECompanionAttackCooldownGameplayEffect.generated.h"

UCLASS()
class AI_RE_API UAIRECompanionAttackCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAIRECompanionAttackCooldownGameplayEffect(
		const FObjectInitializer& ObjectInitializer);
};

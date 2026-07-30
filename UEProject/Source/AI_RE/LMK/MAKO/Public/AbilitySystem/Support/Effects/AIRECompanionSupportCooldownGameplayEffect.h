#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AIRECompanionSupportCooldownGameplayEffect.generated.h"

UCLASS()
class AI_RE_API UAIRECompanionSupportCooldownGameplayEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAIRECompanionSupportCooldownGameplayEffect(
		const FObjectInitializer& ObjectInitializer);
};

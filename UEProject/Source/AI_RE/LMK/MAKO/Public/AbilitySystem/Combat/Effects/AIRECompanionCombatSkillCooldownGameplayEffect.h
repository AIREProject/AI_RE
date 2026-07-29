#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AIRECompanionCombatSkillCooldownGameplayEffect.generated.h"

UCLASS()
class AI_RE_API UAIRECompanionCombatSkillCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAIRECompanionCombatSkillCooldownGameplayEffect(
		const FObjectInitializer& ObjectInitializer);
};

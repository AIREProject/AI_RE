#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AIRECompanionAutonomousEvadeGameplayEffects.generated.h"

UCLASS()
class AI_RE_API UAIRECompanionAutonomousEvadeCostGameplayEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAIRECompanionAutonomousEvadeCostGameplayEffect();
};

UCLASS()
class AI_RE_API UAIRECompanionAutonomousEvadeCooldownGameplayEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAIRECompanionAutonomousEvadeCooldownGameplayEffect(
		const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class AI_RE_API UAIRECompanionAutonomousEvadeRegenBlockGameplayEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAIRECompanionAutonomousEvadeRegenBlockGameplayEffect(
		const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class AI_RE_API UAIRECompanionAutonomousEvadeInvulnerabilityGameplayEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAIRECompanionAutonomousEvadeInvulnerabilityGameplayEffect(
		const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class AI_RE_API UAIRECompanionStaminaRegenGameplayEffect
	: public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAIRECompanionStaminaRegenGameplayEffect(
		const FObjectInitializer& ObjectInitializer);
};

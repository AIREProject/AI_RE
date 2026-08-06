#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AIRECombatDamageTypes.h"
#include "UObject/Interface.h"
#include "AIRECombatDamageTargetInterface.generated.h"

class UAbilitySystemComponent;

UENUM(BlueprintType)
enum class EAIRECombatAffiliation : uint8
{
	PlayerParty,
	Enemy
};

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class AI_RE_API UAIRECombatDamageTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class AI_RE_API IAIRECombatDamageTargetInterface
{
	GENERATED_BODY()

public:
	virtual FGameplayAttribute GetCombatHealthAttribute() const = 0;
	virtual EAIRECombatAffiliation GetCombatAffiliation() const = 0;
	virtual FGameplayAttribute GetCombatFlinchAttribute() const;
	virtual FGameplayAttribute GetCombatStunAttribute() const;
	virtual bool CanReceiveCombatDamageFrom(const AActor* Source) const;
	virtual bool IsCombatTargetAlive() const;
	virtual void NotifyCombatDamageApplied(const FAIRECombatDamageRequest& Request);
};

namespace AIRECombatDamageTarget
{
	AI_RE_API bool ResolveAbilitySystemAndHealth(
		const AActor* TargetActor,
		UAbilitySystemComponent*& OutAbilitySystem,
		FGameplayAttribute& OutHealthAttribute);

	AI_RE_API bool IsAlive(const AActor* TargetActor);
}

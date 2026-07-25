#include "AIREThreatTargetInterface.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIRECompanionGameplayTags.h"

bool IAIREThreatTargetInterface::IsHostileThreatFor_Implementation(const AActor*) const
{
	return false;
}

bool IAIREThreatTargetInterface::IsAliveThreatTarget_Implementation() const
{
	const AActor* TargetActor = Cast<AActor>(this);
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(
		TargetActor,
		true);
	if (!IsValid(AbilitySystem))
	{
		return true;
	}

	return !AbilitySystem->HasMatchingGameplayTag(AIRECompanionGameplayTags::StateDisabledDead);
}

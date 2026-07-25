#include "AIRECompanionDamageGameplayEffect.h"

#include "AIRECompanionAttributeSet.h"
#include "AIRECompanionGameplayTags.h"

UAIRECompanionDamageGameplayEffect::UAIRECompanionDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataTag = AIRECompanionGameplayTags::DataDamage;

	FGameplayModifierInfo& HealthModifier = Modifiers.AddDefaulted_GetRef();
	HealthModifier.Attribute = UAIRECompanionAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageMagnitude);
}

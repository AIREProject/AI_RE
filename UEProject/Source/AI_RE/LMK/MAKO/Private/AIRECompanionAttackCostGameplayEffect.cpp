#include "AIRECompanionAttackCostGameplayEffect.h"

#include "AIRECompanionAttributeSet.h"
#include "AIRECompanionGameplayTags.h"

UAIRECompanionAttackCostGameplayEffect::UAIRECompanionAttackCostGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat CostMagnitude;
	CostMagnitude.DataTag = AIRECompanionGameplayTags::DataAttackStaminaCost;

	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute = UAIRECompanionAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::Additive;
	StaminaModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(CostMagnitude);
}

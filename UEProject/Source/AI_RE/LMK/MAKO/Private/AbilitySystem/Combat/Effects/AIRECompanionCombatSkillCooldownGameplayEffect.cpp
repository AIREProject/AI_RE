#include "AbilitySystem/Combat/Effects/AIRECompanionCombatSkillCooldownGameplayEffect.h"

#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UAIRECompanionCombatSkillCooldownGameplayEffect::
UAIRECompanionCombatSkillCooldownGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat CooldownMagnitude;
	CooldownMagnitude.DataTag =
		AIRECompanionGameplayTags::DataCombatSkillCooldownDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownMagnitude);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(AIRECompanionGameplayTags::CooldownCombatSkill);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<
			UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}

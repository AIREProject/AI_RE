#include "AIRECompanionAttackCooldownGameplayEffect.h"

#include "AIRECompanionGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UAIRECompanionAttackCooldownGameplayEffect::UAIRECompanionAttackCooldownGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat CooldownMagnitude;
	CooldownMagnitude.DataTag = AIRECompanionGameplayTags::DataAttackCooldownDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownMagnitude);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(AIRECompanionGameplayTags::CooldownBasicAttack);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}

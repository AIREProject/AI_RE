#include "AbilitySystem/Support/Effects/AIRECompanionSupportCooldownGameplayEffect.h"

#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UAIRECompanionSupportCooldownGameplayEffect::
UAIRECompanionSupportCooldownGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat CooldownMagnitude;
	CooldownMagnitude.DataTag =
		AIRECompanionGameplayTags::DataSupportCooldownDuration;
	DurationMagnitude =
		FGameplayEffectModifierMagnitude(CooldownMagnitude);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(
		AIRECompanionGameplayTags::CooldownSupportHealingItem);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<
			UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}

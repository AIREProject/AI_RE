#include "AbilitySystem/Combat/Effects/AIRECompanionAutonomousEvadeGameplayEffects.h"

#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "AIRECombatGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UAIRECompanionAutonomousEvadeCostGameplayEffect::
UAIRECompanionAutonomousEvadeCostGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat CostMagnitude;
	CostMagnitude.DataTag =
		AIRECompanionGameplayTags::DataAutonomousEvadeStaminaCost;

	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute =
		UAIRECompanionAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::Additive;
	StaminaModifier.ModifierMagnitude =
		FGameplayEffectModifierMagnitude(CostMagnitude);
}

UAIRECompanionAutonomousEvadeCooldownGameplayEffect::
UAIRECompanionAutonomousEvadeCooldownGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataTag =
		AIRECompanionGameplayTags::DataAutonomousEvadeCooldownDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(
		AIRECompanionGameplayTags::CooldownAutonomousEvade);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<
			UTargetTagsGameplayEffectComponent>(this, TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}

UAIRECompanionAutonomousEvadeRegenBlockGameplayEffect::
UAIRECompanionAutonomousEvadeRegenBlockGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataTag =
		AIRECompanionGameplayTags::DataAutonomousEvadeRegenBlockDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(
		AIRECompanionGameplayTags::StateStaminaRegenBlocked);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<
			UTargetTagsGameplayEffectComponent>(this, TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}

UAIRECompanionAutonomousEvadeInvulnerabilityGameplayEffect::
UAIRECompanionAutonomousEvadeInvulnerabilityGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataTag = AIRECompanionGameplayTags::
		DataAutonomousEvadeInvulnerabilityDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(AIRECombatGameplayTags::StateInvulnerable);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<
			UTargetTagsGameplayEffectComponent>(this, TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}

UAIRECompanionStaminaRegenGameplayEffect::
UAIRECompanionStaminaRegenGameplayEffect(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(0.1f);
	bExecutePeriodicEffectOnApplication = false;
	PeriodicInhibitionPolicy =
		EGameplayEffectPeriodInhibitionRemovedPolicy::NeverReset;

	FSetByCallerFloat RegenMagnitude;
	RegenMagnitude.DataTag =
		AIRECompanionGameplayTags::DataStaminaRegenPerTick;
	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute =
		UAIRECompanionAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::Additive;
	StaminaModifier.ModifierMagnitude =
		FGameplayEffectModifierMagnitude(RegenMagnitude);

	UTargetTagRequirementsGameplayEffectComponent* RequirementsComponent =
		ObjectInitializer.CreateDefaultSubobject<
			UTargetTagRequirementsGameplayEffectComponent>(
				this,
				TEXT("TargetRequirements"));
	RequirementsComponent->OngoingTagRequirements.IgnoreTags.AddTag(
		AIRECompanionGameplayTags::StateStaminaRegenBlocked);
	RequirementsComponent->OngoingTagRequirements.IgnoreTags.AddTag(
		AIRECompanionGameplayTags::StateDisabled);
	GEComponents.Add(RequirementsComponent);
}

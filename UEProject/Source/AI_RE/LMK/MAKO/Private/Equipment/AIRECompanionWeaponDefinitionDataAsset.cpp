#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"

#include "Equipment/AIRECompanionAbilitySetDataAsset.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "GameplayTagsManager.h"
#include "Misc/DataValidation.h"

bool UAIRECompanionWeaponDefinitionDataAsset::IsWeaponDefinitionValid(FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();
	if (!WeaponTag.IsValid()
		|| !WeaponTag.MatchesTag(AIRECompanionGameplayTags::WeaponCompanion)
		|| WeaponTag.MatchesTagExact(AIRECompanionGameplayTags::WeaponCompanion)
		|| WeaponTag.MatchesTagExact(AIRECompanionGameplayTags::WeaponCompanionMelee))
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidWeaponTag",
			"Weapon Tag must be a concrete child of Weapon.Companion.");
		return false;
	}

	if (!UGameplayTagsManager::Get().RequestGameplayTagChildren(WeaponTag).IsEmpty())
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"NonLeafWeaponTag",
			"Weapon Tag must be a leaf identity rather than a weapon category.");
		return false;
	}

	if (AbilitySet.IsNull())
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"MissingAbilitySet",
			"Weapon Definition must reference an Ability Set.");
		return false;
	}

	if (!FMath::IsFinite(Damage) || Damage < 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidDamage",
			"Damage must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(StaminaCost) || StaminaCost < 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidStaminaCost",
			"Stamina Cost must be finite and non-negative.");
		return false;
	}

	TSet<FName> ComboMontageSections;
	for (int32 StepIndex = 0; StepIndex < ComboSteps.Num(); ++StepIndex)
	{
		const FAIREWeaponComboStepDefinition& ComboStep = ComboSteps[StepIndex];
		if (ComboStep.MontageSection.IsNone())
		{
			OutValidationError = FText::Format(
				NSLOCTEXT(
					"AIRECompanionWeaponDefinition",
					"MissingComboMontageSection",
					"Combo Step {0} must specify a Montage Section."),
				FText::AsNumber(StepIndex));
			return false;
		}

		if (ComboMontageSections.Contains(ComboStep.MontageSection))
		{
			OutValidationError = FText::Format(
				NSLOCTEXT(
					"AIRECompanionWeaponDefinition",
					"DuplicateComboMontageSection",
					"Combo Step {0} uses duplicate Montage Section '{1}'."),
				FText::AsNumber(StepIndex),
				FText::FromName(ComboStep.MontageSection));
			return false;
		}
		ComboMontageSections.Add(ComboStep.MontageSection);

		if (!FMath::IsFinite(ComboStep.Damage) || ComboStep.Damage < 0.0f)
		{
			OutValidationError = FText::Format(
				NSLOCTEXT(
					"AIRECompanionWeaponDefinition",
					"InvalidComboDamage",
					"Combo Step {0} Damage must be finite and non-negative."),
				FText::AsNumber(StepIndex));
			return false;
		}

		if (!FMath::IsFinite(ComboStep.StaminaCost) || ComboStep.StaminaCost < 0.0f)
		{
			OutValidationError = FText::Format(
				NSLOCTEXT(
					"AIRECompanionWeaponDefinition",
					"InvalidComboStaminaCost",
					"Combo Step {0} Stamina Cost must be finite and non-negative."),
				FText::AsNumber(StepIndex));
			return false;
		}
	}

	if (!FMath::IsFinite(AttackRange) || AttackRange < 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidAttackRange",
			"Attack Range must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(CooldownDuration) || CooldownDuration < 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidCooldownDuration",
			"Cooldown Duration must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(AttackHalfAngleDegrees)
		|| AttackHalfAngleDegrees < 0.0f
		|| AttackHalfAngleDegrees > 180.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidAttackHalfAngle",
			"Attack Half Angle must be finite and between 0 and 180 degrees.");
		return false;
	}

	if (!FMath::IsFinite(FallbackHitDelay) || FallbackHitDelay < 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidFallbackHitDelay",
			"Fallback Hit Delay must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(FallbackRecoveryDuration) || FallbackRecoveryDuration < 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidFallbackRecovery",
			"Fallback Recovery Duration must be finite and non-negative.");
		return false;
	}

	return true;
}

bool UAIRECompanionWeaponDefinitionDataAsset::IsMeleeWeapon() const
{
	return WeaponTag.IsValid()
		&& WeaponTag.MatchesTag(AIRECompanionGameplayTags::WeaponCompanionMelee);
}

#if WITH_EDITOR
EDataValidationResult UAIRECompanionWeaponDefinitionDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	FText ValidationError;
	if (!IsWeaponDefinitionValid(ValidationError))
	{
		Context.AddError(ValidationError);
		return EDataValidationResult::Invalid;
	}

	return SuperResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

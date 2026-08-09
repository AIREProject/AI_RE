#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"

#include "AI_REAbilitySetDataAsset.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "GameplayTagsManager.h"
#include "Misc/DataValidation.h"

bool UAIRECompanionWeaponDefinitionDataAsset::IsWeaponDefinitionValid(FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();
	if (!WeaponTag.IsValid()
		|| !WeaponTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Weapon")))
		|| WeaponTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(FName("Weapon")))
		|| WeaponTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(FName("Weapon.Companion")))
		|| WeaponTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(FName("Weapon.Player"))))
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidWeaponTag",
			"Weapon Tag must be a concrete child of Weapon (e.g. Weapon.Companion.Melee or Weapon.Player.Melee).");
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
	if (!FMath::IsFinite(StaggerValue)
		|| StaggerValue < 0.0f
		|| (Damage <= 0.0f && StaggerValue <= 0.0f))
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidStagger",
			"Stagger must be finite and non-negative, and Damage and Stagger cannot both be zero.");
		return false;
	}
	// TargetingMode 제한 해제: 플레이어 광역(MultiTarget) 공격 허용을 위해 임시 주석 처리
	/*
	if (TargetingMode != EAIRECombatTargetingMode::SingleTarget)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"UnsupportedTargetingMode",
			"Current companion damage execution supports SingleTarget attacks only.");
		return false;
	}
	*/

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
		if (!FMath::IsFinite(ComboStep.StaggerValue)
			|| ComboStep.StaggerValue < 0.0f
			|| (ComboStep.Damage <= 0.0f
				&& ComboStep.StaggerValue <= 0.0f))
		{
			OutValidationError = FText::Format(
				NSLOCTEXT(
					"AIRECompanionWeaponDefinition",
					"InvalidComboStagger",
					"Combo Step {0} Stagger must be finite and non-negative, and Damage and Stagger cannot both be zero."),
				FText::AsNumber(StepIndex));
			return false;
		}
		/*
		if (ComboStep.TargetingMode
			!= EAIRECombatTargetingMode::SingleTarget)
		{
			OutValidationError = FText::Format(
				NSLOCTEXT(
					"AIRECompanionWeaponDefinition",
					"UnsupportedComboTargetingMode",
					"Combo Step {0} must use SingleTarget until area damage fan-out is implemented."),
				FText::AsNumber(StepIndex));
			return false;
		}
		*/

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

	if (CombatSkill.bEnabled)
	{
		if (!FMath::IsFinite(CombatSkill.Damage) || CombatSkill.Damage < 0.0f)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"InvalidCombatSkillDamage",
				"Combat Skill Damage must be finite and non-negative.");
			return false;
		}
		if (!FMath::IsFinite(CombatSkill.StaggerValue)
			|| CombatSkill.StaggerValue < 0.0f
			|| (CombatSkill.Damage <= 0.0f
				&& CombatSkill.StaggerValue <= 0.0f))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"InvalidCombatSkillStagger",
				"Combat Skill Stagger must be finite and non-negative, and Damage and Stagger cannot both be zero.");
			return false;
		}
		/*
		if (CombatSkill.TargetingMode
			!= EAIRECombatTargetingMode::SingleTarget)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"UnsupportedCombatSkillTargetingMode",
				"Current companion combat skills support SingleTarget only.");
			return false;
		}
		*/

		if (!FMath::IsFinite(CombatSkill.CooldownDuration)
			|| CombatSkill.CooldownDuration < 0.0f)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"InvalidCombatSkillCooldown",
				"Combat Skill Cooldown Duration must be finite and non-negative.");
			return false;
		}

		if (!FMath::IsFinite(CombatSkill.AttackRange)
			|| CombatSkill.AttackRange < 0.0f)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"InvalidCombatSkillRange",
				"Combat Skill Attack Range must be finite and non-negative.");
			return false;
		}

		if (!FMath::IsFinite(CombatSkill.SelectionChance)
			|| CombatSkill.SelectionChance < 0.0f
			|| CombatSkill.SelectionChance > 1.0f)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"InvalidCombatSkillSelectionChance",
				"Combat Skill Selection Chance must be finite and between 0 and 1.");
			return false;
		}

		if (!FMath::IsFinite(CombatSkill.FallbackHitDelay)
			|| CombatSkill.FallbackHitDelay < 0.0f
			|| !FMath::IsFinite(CombatSkill.FallbackRecoveryDuration)
			|| CombatSkill.FallbackRecoveryDuration < 0.0f)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"InvalidCombatSkillFallback",
				"Combat Skill fallback timings must be finite and non-negative.");
			return false;
		}
	}

	return true;
}

bool UAIRECompanionWeaponDefinitionDataAsset::IsMeleeWeapon() const
{
	return WeaponTag.IsValid()
		&& (WeaponTag.MatchesTag(AIRECompanionGameplayTags::WeaponCompanionMelee) || WeaponTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Weapon.Player.Melee"))));
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

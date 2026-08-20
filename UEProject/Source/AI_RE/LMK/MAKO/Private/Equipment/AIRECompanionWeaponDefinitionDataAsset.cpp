#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"

#include "AI_REAbilitySetDataAsset.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "GameplayTagsManager.h"
#include "Misc/DataValidation.h"

bool FAIREWeaponTraceSocketPair::IsConfigured() const
{
	return !TraceStartSocket.IsNone() && !TraceEndSocket.IsNone();
}

bool FAIREWeaponTraceSocketPair::IsPartiallyConfigured() const
{
	return TraceStartSocket.IsNone() != TraceEndSocket.IsNone();
}

UAIRECompanionWeaponDefinitionDataAsset::UAIRECompanionWeaponDefinitionDataAsset()
{
	LeftTraceSockets.TraceStartSocket = FName(TEXT("weapon_l"));
	LeftTraceSockets.TraceEndSocket = FName(TEXT("weapon_trace_tip_l"));
	RightTraceSockets.TraceStartSocket = FName(TEXT("weapon_r"));
	RightTraceSockets.TraceEndSocket = FName(TEXT("weapon_trace_tip_r"));
}

bool UAIRECompanionWeaponDefinitionDataAsset::IsWeaponDefinitionValid(FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();
	if (!WeaponTag.IsValid())
	{
		OutValidationError = NSLOCTEXT("AIRECompanionWeaponDefinition", "InvalidWeaponTag", "Weapon Tag is invalid.");
		return false;
	}

	bool bIsCompanionWeapon = WeaponTag.MatchesTag(AIRECompanionGameplayTags::WeaponCompanion);
	bool bIsPlayerWeapon = WeaponTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Weapon.Player")));

	if (!bIsCompanionWeapon && !bIsPlayerWeapon)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidWeaponTag",
			"Weapon Tag must be a child of Weapon.Companion or Weapon.Player.");
		return false;
	}

	if (WeaponTag.MatchesTagExact(AIRECompanionGameplayTags::WeaponCompanion) ||
		WeaponTag.MatchesTagExact(AIRECompanionGameplayTags::WeaponCompanionMelee) ||
		WeaponTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(FName("Weapon.Player"))) ||
		WeaponTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(FName("Weapon.Player.Melee"))))
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidWeaponTag",
			"Weapon Tag must be a concrete child (e.g. Weapon.Player.Melee.GreatSword), not a base category.");
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

	if (!LeftTraceSockets.IsConfigured()
		|| LeftTraceSockets.TraceStartSocket
			== LeftTraceSockets.TraceEndSocket
		|| !RightTraceSockets.IsConfigured()
		|| RightTraceSockets.TraceStartSocket
			== RightTraceSockets.TraceEndSocket)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidDefaultTraceSockets",
			"Left and Right Trace Sockets must each specify distinct start and end sockets.");
		return false;
	}

	if (!FMath::IsFinite(TraceRadius) || TraceRadius <= 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidTraceRadius",
			"Trace Radius must be finite and greater than zero.");
		return false;
	}

	if (!FMath::IsFinite(TraceCapsuleRadius)
		|| TraceCapsuleRadius <= 0.0f
		|| !FMath::IsFinite(TraceCapsuleHalfHeight)
		|| TraceCapsuleHalfHeight < TraceCapsuleRadius)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidTraceCapsule",
			"Trace Capsule Radius must be positive and Half Height must be at least the Radius.");
		return false;
	}

	if (TraceChannel.GetValue() >= ECC_MAX)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidTraceChannel",
			"Trace Channel must be a valid collision channel.");
		return false;
	}

	if (!FMath::IsFinite(HarvestAttackRange)
		|| HarvestAttackRange < 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"InvalidHarvestAttackRange",
			"Harvest Attack Range must be finite and non-negative.");
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
	if (TargetingMode != EAIRECombatTargetingMode::SingleTarget)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionWeaponDefinition",
			"UnsupportedTargetingMode",
			"Current companion damage execution supports SingleTarget attacks only.");
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
		if (ComboStep.TraceSocketOverride.IsPartiallyConfigured()
			|| (ComboStep.TraceSocketOverride.IsConfigured()
				&& ComboStep.TraceSocketOverride.TraceStartSocket
					== ComboStep.TraceSocketOverride.TraceEndSocket))
		{
			OutValidationError = FText::Format(
				NSLOCTEXT(
					"AIRECompanionWeaponDefinition",
					"InvalidComboTraceSocketOverride",
					"Combo Step {0} Trace Socket Override must be empty or specify distinct start and end sockets."),
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
		if (CombatSkill.TargetingMode
			!= EAIRECombatTargetingMode::SingleTarget)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"UnsupportedCombatSkillTargetingMode",
				"Current companion combat skills support SingleTarget only.");
			return false;
		}
		if (CombatSkill.TraceSocketOverride.IsPartiallyConfigured()
			|| (CombatSkill.TraceSocketOverride.IsConfigured()
				&& CombatSkill.TraceSocketOverride.TraceStartSocket
					== CombatSkill.TraceSocketOverride.TraceEndSocket))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionWeaponDefinition",
				"InvalidCombatSkillTraceSocketOverride",
				"Combat Skill Trace Socket Override must be empty or specify distinct start and end sockets.");
			return false;
		}

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
		&& WeaponTag.MatchesTag(AIRECompanionGameplayTags::WeaponCompanionMelee);
}

FAIREWeaponTraceSocketPair
UAIRECompanionWeaponDefinitionDataAsset::ResolveTraceSockets(
	const EAIRECompanionWeaponTraceSide TraceSide,
	const FAIREWeaponTraceSocketPair& TraceSocketOverride) const
{
	if (TraceSocketOverride.IsConfigured())
	{
		return TraceSocketOverride;
	}

	return TraceSide == EAIRECompanionWeaponTraceSide::Left
		? LeftTraceSockets
		: RightTraceSockets;
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

#include "AIRECompanionWeaponDefinitionDataAsset.h"

#include "AIRECompanionAbilitySetDataAsset.h"
#include "AIRECompanionGameplayTags.h"
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

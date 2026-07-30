#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"

#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Misc/DataValidation.h"

bool UAIRECompanionItemDefinitionDataAsset::IsCompanionItemDefinitionValid(
	FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();
	if (ItemId.IsNone())
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionItemDefinition",
			"MissingItemId",
			"A Companion Item Definition must specify an Item ID.");
		return false;
	}

	if (MaxStackSize < 1)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionItemDefinition",
			"InvalidMaxStack",
			"A Companion Item Definition must have a Max Stack Size of at least one.");
		return false;
	}

	if (ItemType == EAI_REItemType::Consumable)
	{
		if (!FMath::IsFinite(HealingAmount) || HealingAmount <= 0.0f
			|| !FMath::IsFinite(TreatmentDuration) || TreatmentDuration < 0.0f
			|| !FMath::IsFinite(SupportRange) || SupportRange < 0.0f
			|| !FMath::IsFinite(CooldownDuration) || CooldownDuration < 0.0f)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionItemDefinition",
				"InvalidHealingConsumable",
				"A healing consumable must use finite positive Healing Amount and non-negative Treatment, Range, and Cooldown values.");
			return false;
		}

		if (IsValid(WeaponDefinition))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionItemDefinition",
				"ConsumableHasWeapon",
				"A Companion consumable must not reference a Weapon Definition.");
			return false;
		}
		return true;
	}

	if (ItemType == EAI_REItemType::Weapon)
	{
		if (MaxStackSize != 1 || !IsValid(WeaponDefinition))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionItemDefinition",
				"InvalidWeaponItem",
				"A Companion weapon item must have Max Stack Size 1 and reference a Weapon Definition.");
			return false;
		}

		return WeaponDefinition->IsWeaponDefinitionValid(OutValidationError);
	}

	OutValidationError = NSLOCTEXT(
		"AIRECompanionItemDefinition",
		"UnsupportedItemType",
		"The first Companion inventory supports only Consumable and Weapon item types.");
	return false;
}

#if WITH_EDITOR
EDataValidationResult UAIRECompanionItemDefinitionDataAsset::IsDataValid(
	FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	FText ValidationError;
	if (!IsCompanionItemDefinitionValid(ValidationError))
	{
		Context.AddError(ValidationError);
		return EDataValidationResult::Invalid;
	}

	return SuperResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

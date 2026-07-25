#include "AIRECompanionAbilitySetDataAsset.h"

#include "Abilities/GameplayAbility.h"
#include "AIRECompanionGameplayAbility.h"
#include "Misc/DataValidation.h"

bool UAIRECompanionAbilitySetDataAsset::IsAbilitySetValid(FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();
	if (Abilities.IsEmpty())
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionAbilitySet",
			"EmptyAbilitySet",
			"An Ability Set must contain at least one ability.");
		return false;
	}

	TSet<TSubclassOf<UGameplayAbility>> UniqueAbilityClasses;
	for (const FAIRECompanionAbilitySetEntry& Entry : Abilities)
	{
		if (!Entry.AbilityClass)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionAbilitySet",
				"MissingAbilityClass",
				"Every Ability Set entry must specify an ability class.");
			return false;
		}

		if (Entry.AbilityLevel < 1)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionAbilitySet",
				"InvalidAbilityLevel",
				"Every Ability Set entry must use an ability level of at least one.");
			return false;
		}

		if (!Entry.AbilityClass->IsChildOf(
			UAIRECompanionGameplayAbility::StaticClass()))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionAbilitySet",
				"InvalidAbilityBase",
				"Ability Set entries must derive from AIRE Companion Gameplay Ability.");
			return false;
		}

		if (UniqueAbilityClasses.Contains(Entry.AbilityClass))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionAbilitySet",
				"DuplicateAbilityClass",
				"An Ability Set must not grant the same ability class more than once.");
			return false;
		}

		UniqueAbilityClasses.Add(Entry.AbilityClass);
	}

	return true;
}

#if WITH_EDITOR
EDataValidationResult UAIRECompanionAbilitySetDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	FText ValidationError;
	if (!IsAbilitySetValid(ValidationError))
	{
		Context.AddError(ValidationError);
		return EDataValidationResult::Invalid;
	}

	return SuperResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

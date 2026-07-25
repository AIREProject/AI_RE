#include "AIRECompanionEquipmentComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AIRECompanionAbilitySetDataAsset.h"
#include "AIRECompanionGameplayTags.h"
#include "AIRECompanionWeaponDefinitionDataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionEquipment, Log, All);

UAIRECompanionEquipmentComponent::UAIRECompanionEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIRECompanionEquipmentComponent::InitializeEquipment(
	UAbilitySystemComponent* InAbilitySystem,
	const float BasicAttackCooldown)
{
	ShutdownEquipment();
	if (!IsValid(InAbilitySystem)
		|| !FMath::IsFinite(BasicAttackCooldown)
		|| BasicAttackCooldown < 0.0f)
	{
		return false;
	}

	AbilitySystem = InAbilitySystem;
	if (!IsValid(DefaultWeaponDefinition))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Companion %s has no default Weapon Definition. No weapon abilities were granted."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	return EquipWeapon(DefaultWeaponDefinition, BasicAttackCooldown);
}

void UAIRECompanionEquipmentComponent::ShutdownEquipment()
{
	UnequipCurrentWeapon();
	AbilitySystem.Reset();
}

const UAIRECompanionWeaponDefinitionDataAsset*
UAIRECompanionEquipmentComponent::GetCurrentWeaponDefinition() const
{
	return CurrentWeaponDefinition;
}

FGameplayTag UAIRECompanionEquipmentComponent::GetCurrentWeaponTag() const
{
	return IsValid(CurrentWeaponDefinition)
		? CurrentWeaponDefinition->WeaponTag
		: FGameplayTag();
}

bool UAIRECompanionEquipmentComponent::IsCurrentWeaponInCategory(
	const FGameplayTag WeaponCategory) const
{
	const FGameplayTag CurrentWeaponTag = GetCurrentWeaponTag();
	return WeaponCategory.IsValid()
		&& CurrentWeaponTag.IsValid()
		&& CurrentWeaponTag.MatchesTag(WeaponCategory);
}

FGameplayAbilitySpecHandle UAIRECompanionEquipmentComponent::FindGrantedAbilityHandle(
	const FGameplayTag AbilityTag) const
{
	if (!AbilitySystem.IsValid() || !AbilityTag.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	for (const FGameplayAbilitySpecHandle& AbilityHandle : GrantedAbilityHandles)
	{
		const FGameplayAbilitySpec* AbilitySpec =
			AbilitySystem->FindAbilitySpecFromHandle(AbilityHandle);
		if (AbilitySpec
			&& IsValid(AbilitySpec->Ability.Get())
			&& AbilitySpec->Ability->GetAssetTags().HasTagExact(AbilityTag))
		{
			return AbilityHandle;
		}
	}

	return FGameplayAbilitySpecHandle();
}

void UAIRECompanionEquipmentComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownEquipment();
	Super::EndPlay(EndPlayReason);
}

bool UAIRECompanionEquipmentComponent::EquipWeapon(
	UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
	const float BasicAttackCooldown)
{
	if (!AbilitySystem.IsValid() || !IsValid(WeaponDefinition))
	{
		return false;
	}

	FText ValidationError;
	if (!WeaponDefinition->IsWeaponDefinitionValid(ValidationError))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Weapon Definition %s is invalid: %s"),
			*GetNameSafe(WeaponDefinition),
			*ValidationError.ToString());
		return false;
	}

	UAIRECompanionAbilitySetDataAsset* AbilitySet =
		WeaponDefinition->AbilitySet.LoadSynchronous();
	if (!IsValid(AbilitySet) || !AbilitySet->IsAbilitySetValid(ValidationError))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Weapon Definition %s could not load a valid Ability Set: %s"),
			*GetNameSafe(WeaponDefinition),
			*ValidationError.ToString());
		return false;
	}

	UnequipCurrentWeapon();
	CurrentWeaponDefinition = WeaponDefinition;

	for (const FAIRECompanionAbilitySetEntry& Entry : AbilitySet->Abilities)
	{
		FGameplayAbilitySpec AbilitySpec(
			Entry.AbilityClass,
			Entry.AbilityLevel,
			INDEX_NONE,
			WeaponDefinition);
		AbilitySpec.SetByCallerTagMagnitudes.Add(
			AIRECompanionGameplayTags::DataAttackStaminaCost,
			-WeaponDefinition->StaminaCost);
		AbilitySpec.SetByCallerTagMagnitudes.Add(
			AIRECompanionGameplayTags::DataAttackCooldownDuration,
			BasicAttackCooldown);

		const FGameplayAbilitySpecHandle GrantedHandle =
			AbilitySystem->GiveAbility(AbilitySpec);
		if (!GrantedHandle.IsValid())
		{
			UE_LOG(
				LogAIRECompanionEquipment,
				Warning,
				TEXT("Failed to grant ability %s for Weapon Definition %s."),
				*GetNameSafe(Entry.AbilityClass.Get()),
				*GetNameSafe(WeaponDefinition));
			UnequipCurrentWeapon();
			return false;
		}

		GrantedAbilityHandles.Add(GrantedHandle);
	}

	if (WeaponDefinition->IsMeleeWeapon()
		&& !FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatBasicAttack).IsValid())
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Melee Weapon Definition %s did not grant a Basic Attack ability."),
			*GetNameSafe(WeaponDefinition));
		UnequipCurrentWeapon();
		return false;
	}

	UE_LOG(
		LogAIRECompanionEquipment,
		Log,
		TEXT("Companion weapon equipped. Companion=%s Weapon=%s Tag=%s Abilities=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(CurrentWeaponDefinition),
		*CurrentWeaponDefinition->WeaponTag.ToString(),
		GrantedAbilityHandles.Num());
	return true;
}

void UAIRECompanionEquipmentComponent::UnequipCurrentWeapon()
{
	if (AbilitySystem.IsValid())
	{
		for (const FGameplayAbilitySpecHandle& AbilityHandle : GrantedAbilityHandles)
		{
			if (AbilityHandle.IsValid())
			{
				AbilitySystem->CancelAbilityHandle(AbilityHandle);
				AbilitySystem->ClearAbility(AbilityHandle);
			}
		}
	}

	GrantedAbilityHandles.Reset();
	CurrentWeaponDefinition = nullptr;
}

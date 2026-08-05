#include "Core/AIRECompanionConfigDataAsset.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Equipment/AIRECompanionAbilitySetDataAsset.h"
#include "AIREGameplayInventoryTypes.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"
#include "Misc/DataValidation.h"

bool UAIRECompanionConfigDataAsset::IsConfigurationValid(FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();

	if (!FMath::IsFinite(MovementSpeed) || MovementSpeed <= 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidMovementSpeed", "Run Speed must be finite and greater than zero.");
		return false;
	}

	if (!FMath::IsFinite(WalkSpeed) || WalkSpeed <= 0.0f || WalkSpeed > MovementSpeed)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidWalkSpeed", "Walk Speed must be finite, greater than zero, and not exceed Run Speed.");
		return false;
	}

	if (!FMath::IsFinite(FollowStopDistance) || FollowStopDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidFollowStopDistance", "Follow Stop Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(RunStartDistance) || RunStartDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidRunStartDistance", "Run Start Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(WalkResumeDistance) || WalkResumeDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidWalkResumeDistance", "Walk Resume Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(ReturnStartDistance) || ReturnStartDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidReturnStartDistance", "Return Start Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(ThreatDetectionDistance) || ThreatDetectionDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidThreatDetectionDistance", "Threat Detection Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(MaxChaseDistanceFromPlayer) || MaxChaseDistanceFromPlayer < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidMaxChaseDistance", "Max Chase Distance From Player must be finite and non-negative.");
		return false;
	}

	FAIRECompanionLocalBehaviorPolicy DefaultPolicy;
	DefaultPolicy.EngagementPolicy = DefaultEngagementPolicy;
	DefaultPolicy.RolePreference = DefaultRolePreference;
	if (!DefaultPolicy.IsValid())
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionConfig",
			"InvalidDefaultLocalBehaviorPolicy",
			"Default local behavior policy values must be supported.");
		return false;
	}

	if (!FMath::IsFinite(DefendPlayerRadius)
		|| DefendPlayerRadius < 0.0f
		|| DefendPlayerRadius >= MaxChaseDistanceFromPlayer)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionConfig",
			"InvalidDefendPlayerRadius",
			"Defend Player Radius must be finite, non-negative, and less than Max Chase Distance From Player.");
		return false;
	}

	if (!FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidMaxHealth", "Max Health must be finite and greater than zero.");
		return false;
	}

	if (!FMath::IsFinite(InitialHealth) || InitialHealth < 0.0f || InitialHealth > MaxHealth)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidInitialHealth", "Initial Health must be finite and between zero and Max Health.");
		return false;
	}

	if (!FMath::IsFinite(MaxStamina) || MaxStamina <= 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidMaxStamina", "Max Stamina must be finite and greater than zero.");
		return false;
	}

	if (!FMath::IsFinite(InitialStamina) || InitialStamina < 0.0f || InitialStamina > MaxStamina)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidInitialStamina", "Initial Stamina must be finite and between zero and Max Stamina.");
		return false;
	}

	if (FollowStopDistance >= WalkResumeDistance
		|| WalkResumeDistance >= RunStartDistance)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionConfig",
			"InvalidWalkRunThresholds",
			"Movement distances must satisfy Follow Stop Distance < Walk Resume Distance < Run Start Distance.");
		return false;
	}

	if (FollowStopDistance >= ReturnStartDistance)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidFollowReturnThresholds", "Follow Stop Distance must be less than Return Start Distance.");
		return false;
	}

	if (!FMath::IsFinite(StorageWorkDuration)
		|| StorageWorkDuration < 0.0f
		|| !FMath::IsFinite(StorageAcceptanceRadius)
		|| StorageAcceptanceRadius < 0.0f
		|| !FMath::IsFinite(StorageMovementTimeout)
		|| StorageMovementTimeout <= 0.0f)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionConfig",
			"InvalidStorageWorkSettings",
			"Storage work duration and acceptance radius must be finite and non-negative, and movement timeout must be finite and greater than zero.");
		return false;
	}

	TSet<FName> StorageRuleItemIds;
	for (const FAIRECompanionStorageRule& Rule
		: StorageRules)
	{
		if (!IsValid(Rule.ItemDefinition)
			|| Rule.MinimumCarryCount < 0
			|| Rule.MaximumCarryCount < Rule.MinimumCarryCount)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionConfig",
				"InvalidStorageRule",
				"Every storage rule must specify a valid Companion Item Definition and satisfy 0 <= Minimum Carry Count <= Maximum Carry Count.");
			return false;
		}

		FText ItemValidationError;
		if (!Rule.ItemDefinition->IsCompanionItemDefinitionValid(
				ItemValidationError))
		{
			OutValidationError = ItemValidationError;
			return false;
		}

		if (StorageRuleItemIds.Contains(Rule.ItemDefinition->ItemId))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionConfig",
				"DuplicateStorageRule",
				"Storage rules must not list the same Item ID more than once.");
			return false;
		}
		StorageRuleItemIds.Add(Rule.ItemDefinition->ItemId);
	}

	int32 RequiredSlots = 0;
	TSet<FName> InitialItemIds;
	for (const FAIRECompanionInitialInventoryEntry& Entry : InitialInventory)
	{
		if (!IsValid(Entry.ItemDefinition) || Entry.Count < 1)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionConfig",
				"InvalidInitialInventoryEntry",
				"Every initial inventory entry must specify a valid Companion Item Definition and positive Count.");
			return false;
		}

		FText ItemValidationError;
		if (!Entry.ItemDefinition->IsCompanionItemDefinitionValid(
				ItemValidationError))
		{
			OutValidationError = ItemValidationError;
			return false;
		}

		if (InitialItemIds.Contains(Entry.ItemDefinition->ItemId))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionConfig",
				"DuplicateInitialItem",
				"Initial inventory must not list the same Item ID more than once.");
			return false;
		}

		InitialItemIds.Add(Entry.ItemDefinition->ItemId);
		const int32 GeneralItemCount =
			Entry.ItemDefinition->ItemId == DefaultEquippedWeaponItemId
			? FMath::Max(0, Entry.Count - 1)
			: Entry.Count;
		RequiredSlots += FMath::DivideAndRoundUp(
			GeneralItemCount,
			Entry.ItemDefinition->MaxStackSize);
	}

	if (RequiredSlots > AIREGameplayInventory::MakoItemSlotCapacity)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionConfig",
			"InitialInventoryTooLarge",
			"Initial inventory requires more than 20 general MAKO item slots after the equipped weapon is separated.");
		return false;
	}

	if (!InitialInventory.IsEmpty())
	{
		if (DefaultEquippedWeaponItemId.IsNone()
			|| !InitialItemIds.Contains(DefaultEquippedWeaponItemId)
			|| DefaultHealingItemId.IsNone()
			|| !InitialItemIds.Contains(DefaultHealingItemId)
			|| !IsValid(SupportAbilitySet))
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionConfig",
				"InvalidCompanionLoadout",
				"A configured Companion inventory must contain the default weapon and healing item and reference a Support Ability Set.");
			return false;
		}

		const UAIRECompanionItemDefinitionDataAsset*
			DefaultWeaponItem = nullptr;
		const UAIRECompanionItemDefinitionDataAsset*
			DefaultHealingItem = nullptr;
		for (const FAIRECompanionInitialInventoryEntry& Entry
			: InitialInventory)
		{
			if (Entry.ItemDefinition->ItemId
				== DefaultEquippedWeaponItemId)
			{
				DefaultWeaponItem = Entry.ItemDefinition;
			}
			if (Entry.ItemDefinition->ItemId
				== DefaultHealingItemId)
			{
				DefaultHealingItem = Entry.ItemDefinition;
			}
		}
		if (!IsValid(DefaultWeaponItem)
			|| DefaultWeaponItem->ItemType
				!= EAI_REItemType::Weapon
			|| !IsValid(DefaultHealingItem)
			|| DefaultHealingItem->ItemType
				!= EAI_REItemType::Consumable)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionConfig",
				"InvalidDefaultItemRoles",
				"Default Equipped Weapon must reference a Weapon item and Default Healing Item must reference a Consumable item.");
			return false;
		}

		FText AbilitySetValidationError;
		if (!SupportAbilitySet->IsAbilitySetValid(AbilitySetValidationError))
		{
			OutValidationError = AbilitySetValidationError;
			return false;
		}

		int32 SupportHealingAbilityCount = 0;
		for (const FAIRECompanionAbilitySetEntry& Entry
			: SupportAbilitySet->Abilities)
		{
			const UGameplayAbility* AbilityDefaultObject =
				Entry.AbilityClass
					? Entry.AbilityClass
						->GetDefaultObject<UGameplayAbility>()
					: nullptr;
			SupportHealingAbilityCount +=
				IsValid(AbilityDefaultObject)
				&& AbilityDefaultObject->GetAssetTags()
					.HasTagExact(
						AIRECompanionGameplayTags::
							AbilitySupportHealingItem)
					? 1
					: 0;
		}
		if (SupportAbilitySet->Abilities.Num() != 1
			|| SupportHealingAbilityCount != 1)
		{
			OutValidationError = NSLOCTEXT(
				"AIRECompanionConfig",
				"InvalidSupportAbilitySetRole",
				"Support Ability Set must contain exactly one Support Healing ability.");
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
EDataValidationResult UAIRECompanionConfigDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);

	FText ValidationError;
	if (!IsConfigurationValid(ValidationError))
	{
		Context.AddError(ValidationError);
		return EDataValidationResult::Invalid;
	}

	return SuperResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

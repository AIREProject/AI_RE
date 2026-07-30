#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AIRECompanionConfigDataAsset.generated.h"

class UAIRECompanionAbilitySetDataAsset;
class UAIRECompanionItemDefinitionDataAsset;

USTRUCT(BlueprintType)
struct FAIRECompanionInitialInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UAIRECompanionItemDefinitionDataAsset> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

UCLASS(BlueprintType)
class AI_RE_API UAIRECompanionConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool IsConfigurationValid(FText& OutValidationError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Run speed. The property name is retained for existing Data Asset compatibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (DisplayName = "Run Speed", ClampMin = "0.0", UIMin = "0.0", Units = "cm/s"))
	float MovementSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm/s"))
	float WalkSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (UIMin = "0.0", Units = "cm"))
	float FollowStopDistance = 200.0f;

	/** Switches from walking to running beyond this player distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float RunStartDistance = 500.0f;

	/** Switches back to walking below this player distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float WalkResumeDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (UIMin = "0.0", Units = "cm"))
	float ReturnStartDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (UIMin = "0.0", Units = "cm"))
	float ThreatDetectionDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (UIMin = "0.0", Units = "cm"))
	float MaxChaseDistanceFromPlayer = 1500.0f;

	/** Deprecated. Attack range is owned by the equipped Weapon Definition. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "AIRE|Companion|Deprecated",
		meta = (DeprecatedProperty, DeprecationMessage = "Use Weapon Definition AttackRange.", UIMin = "0.0", Units = "cm"))
	float CombatDistance = 150.0f;

	/** Deprecated. Attack cooldown is owned by the equipped Weapon Definition. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "AIRE|Companion|Deprecated",
		meta = (DeprecatedProperty, DeprecationMessage = "Use Weapon Definition CooldownDuration.", UIMin = "0.0", Units = "s"))
	float CombatCooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (UIMin = "0.0"))
	float InitialHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (UIMin = "0.0"))
	float InitialStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Inventory", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxInventorySlots = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Inventory")
	TArray<FAIRECompanionInitialInventoryEntry> InitialInventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Inventory")
	FName DefaultEquippedWeaponItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Support")
	FName DefaultHealingItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Support")
	TObjectPtr<UAIRECompanionAbilitySetDataAsset> SupportAbilitySet;
};

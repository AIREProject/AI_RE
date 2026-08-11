#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LocalAI/Policy/AIRECompanionLocalBehaviorPolicy.h"
#include "AIRECompanionConfigDataAsset.generated.h"

class UAI_REAbilitySetDataAsset;
class UAIRECompanionItemDefinitionDataAsset;
class UAnimMontage;

USTRUCT(BlueprintType)
struct FAIRECompanionInitialInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UAIRECompanionItemDefinitionDataAsset> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct FAIRECompanionStorageRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage")
	TObjectPtr<UAIRECompanionItemDefinitionDataAsset> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage", meta = (ClampMin = "0", UIMin = "0"))
	int32 MinimumCarryCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaximumCarryCount = 0;
};

USTRUCT(BlueprintType)
struct FAIRECompanionAutonomousEvadeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SelectionChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float ReactionDelayMin = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float ReactionDelayMax = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float CooldownDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "cm"))
	float MinimumClearance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float StaminaCost = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float StaminaRegenDelay = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float StaminaRegenRate = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float InvulnerabilityStartDelay = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomous Evade", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float InvulnerabilityDuration = 0.12f;

	bool IsValid() const;
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

	/** Once returning starts, keep returning until this surface distance is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (UIMin = "0.0", Units = "cm"))
	float ReturnStopDistance = 400.0f;

	/** Minimum player-centered radius used by the idle-near-player wander task. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float IdleWanderMinDistance = 150.0f;

	/** Maximum player-centered radius used by the idle-near-player wander task. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float IdleWanderMaxDistance = 350.0f;

	/** Minimum pause after reaching an idle-near-player wander point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float IdleWanderWaitMin = 3.0f;

	/** Maximum pause after reaching an idle-near-player wander point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float IdleWanderWaitMax = 6.0f;

	/** Maximum number of reachable points sampled for one idle-near-player move. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "1", ClampMax = "8", UIMin = "1", UIMax = "8"))
	int32 IdleWanderSampleCount = 8;

	/** Acceptance radius for an idle-near-player wander move. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float IdleWanderAcceptanceRadius = 75.0f;

	/** Delay before the single additional idle-near-player move attempt after a failure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float IdleWanderRetryDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (UIMin = "0.0", Units = "cm"))
	float ThreatDetectionDistance = 1000.0f;

	/** Extra distance retained only for the currently selected threat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float ThreatLoseSightDistance = 200.0f;

	/** Grace period before a sight-loss event releases the current threat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float ThreatSightMemoryDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (UIMin = "0.0", Units = "cm"))
	float MaxChaseDistanceFromPlayer = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Policy")
	EAIRECompanionEngagementPolicy DefaultEngagementPolicy =
		EAIRECompanionEngagementPolicy::Aggressive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Policy")
	EAIRECompanionRolePreference DefaultRolePreference =
		EAIRECompanionRolePreference::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Policy", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float DefendPlayerRadius = 600.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Combat")
	FAIRECompanionAutonomousEvadeSettings AutonomousEvade;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "AIRE|Companion|Deprecated",
		meta = (DeprecatedProperty, DeprecationMessage = "MAKO uses the fixed 20-slot Gameplay Inventory container."))
	int32 MaxInventorySlots = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Inventory")
	TArray<FAIRECompanionInitialInventoryEntry> InitialInventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Inventory")
	FName DefaultEquippedWeaponItemId;

	/** Ordered rules used by physical MAKO storage work. An empty list disables automation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Storage")
	TArray<FAIRECompanionStorageRule> StorageRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Storage")
	TObjectPtr<UAnimMontage> StorageWorkMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Storage", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float StorageWorkDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Storage", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float StorageAcceptanceRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Storage", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float StorageMovementTimeout = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Support")
	FName DefaultHealingItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Support")
	TObjectPtr<UAI_REAbilitySetDataAsset> SupportAbilitySet;
};

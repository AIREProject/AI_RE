#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AIRECombatDamageTypes.h"
#include "AIRECompanionWeaponDefinitionDataAsset.generated.h"

class UAI_REAbilitySetDataAsset;
class UAnimInstance;
class UAnimMontage;

UENUM(BlueprintType)
enum class EAIRECompanionWeaponTraceSide : uint8
{
	Left,
	Right
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREWeaponTraceSocketPair
{
	GENERATED_BODY()

	bool IsConfigured() const;
	bool IsPartiallyConfigured() const;

	/** Socket nearest the handle or striking hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Trace")
	FName TraceStartSocket = NAME_None;

	/** Socket at the visible weapon tip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Trace")
	FName TraceEndSocket = NAME_None;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREWeaponComboStepDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FName MontageSection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaggerValue = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	EAIRECombatTargetingMode TargetingMode =
		EAIRECombatTargetingMode::SingleTarget;

	/** Selects the default blade for this combo step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Trace")
	EAIRECompanionWeaponTraceSide TraceSide =
		EAIRECompanionWeaponTraceSide::Right;

	/** Optional pair replacing the selected default blade sockets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Trace")
	FAIREWeaponTraceSocketPair TraceSocketOverride;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Deprecated",
		meta = (DeprecatedProperty, DeprecationMessage = "Ground basic attacks no longer consume Stamina."))
	float StaminaCost = 20.0f;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREWeaponCombatSkillDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill")
	TSoftObjectPtr<UAnimMontage> SkillMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damage = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaggerValue = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill")
	EAIRECombatTargetingMode TargetingMode =
		EAIRECombatTargetingMode::SingleTarget;

	/** Selects the default blade for this skill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill|Trace")
	EAIRECompanionWeaponTraceSide TraceSide =
		EAIRECompanionWeaponTraceSide::Right;

	/** Optional pair replacing the selected default blade sockets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill|Trace")
	FAIREWeaponTraceSocketPair TraceSocketOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CooldownDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AttackRange = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SelectionChance = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FallbackHitDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Skill", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FallbackRecoveryDuration = 0.6f;
};

UCLASS(BlueprintType)
class AI_RE_API UAIRECompanionWeaponDefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UAIRECompanionWeaponDefinitionDataAsset();

	bool IsWeaponDefinitionValid(FText& OutValidationError) const;
	bool IsMeleeWeapon() const;
	FAIREWeaponTraceSocketPair ResolveTraceSockets(
		EAIRECompanionWeaponTraceSide TraceSide,
		const FAIREWeaponTraceSocketPair& TraceSocketOverride) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Weapon.Companion"))
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TSoftObjectPtr<UAI_REAbilitySetDataAsset> AbilitySet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * Optional ordered combo steps. An empty array preserves the single-attack
	 * Damage and default Montage section contract. Deprecated Stamina cost data is ignored.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Combo")
	TArray<FAIREWeaponComboStepDefinition> ComboSteps;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftClassPtr<UAnimInstance> LinkedAnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaggerValue = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	EAIRECombatTargetingMode TargetingMode =
		EAIRECombatTargetingMode::SingleTarget;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Deprecated",
		meta = (DeprecatedProperty, DeprecationMessage = "Ground basic attacks no longer consume Stamina."))
	float StaminaCost = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AttackRange = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CooldownDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FallbackHitDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FallbackRecoveryDuration = 0.6f;

	/** Default project-owned sockets for the visible left-hand blade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Trace")
	FAIREWeaponTraceSocketPair LeftTraceSockets;

	/** Default project-owned sockets for the visible right-hand blade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Trace")
	FAIREWeaponTraceSocketPair RightTraceSockets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Trace", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "cm"))
	float TraceRadius = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	/** Harvest remains a direct point hit and intentionally uses a shorter range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Harvest", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float HarvestAttackRange = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Skill")
	FAIREWeaponCombatSkillDefinition CombatSkill;
};

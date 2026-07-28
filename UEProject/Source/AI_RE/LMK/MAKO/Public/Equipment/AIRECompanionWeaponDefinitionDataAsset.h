#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AIRECompanionWeaponDefinitionDataAsset.generated.h"

class UAIRECompanionAbilitySetDataAsset;
class UAnimInstance;
class UAnimMontage;

USTRUCT(BlueprintType)
struct AI_RE_API FAIREWeaponComboStepDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FName MontageSection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaminaCost = 20.0f;
};

UCLASS(BlueprintType)
class AI_RE_API UAIRECompanionWeaponDefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool IsWeaponDefinitionValid(FText& OutValidationError) const;
	bool IsMeleeWeapon() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Weapon.Companion"))
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TSoftObjectPtr<UAIRECompanionAbilitySetDataAsset> AbilitySet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * Optional ordered combo steps. An empty array preserves the legacy single-attack
	 * Damage, StaminaCost, and default Montage section contract.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Combo")
	TArray<FAIREWeaponComboStepDefinition> ComboSteps;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftClassPtr<UAnimInstance> LinkedAnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaminaCost = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AttackRange = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CooldownDuration = 1.5f;

	/** Maximum angle from the attacker's forward direction to a valid target. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Attack",
		meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0", Units = "deg"))
	float AttackHalfAngleDegrees = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FallbackHitDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FallbackRecoveryDuration = 0.6f;
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AIRECompanionWeaponDefinitionDataAsset.generated.h"

class UAIRECompanionAbilitySetDataAsset;
class UAnimInstance;
class UAnimMontage;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftClassPtr<UAnimInstance> LinkedAnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaminaCost = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FallbackHitDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float FallbackRecoveryDuration = 0.6f;
};

#pragma once

#include "CoreMinimal.h"
#include "AI_REItemDataAsset.h"
#include "AIRECompanionItemDefinitionDataAsset.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;

UCLASS(BlueprintType)
class AI_RE_API UAIRECompanionItemDefinitionDataAsset : public UAI_REItemDataAsset
{
	GENERATED_BODY()

public:
	bool IsCompanionItemDefinitionValid(FText& OutValidationError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Consumable", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HealingAmount = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Consumable", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float TreatmentDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Consumable", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float SupportRange = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Consumable", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CooldownDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Weapon")
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> WeaponDefinition;
};

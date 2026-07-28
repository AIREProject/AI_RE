#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AIRECompanionAbilitySetDataAsset.generated.h"

class UGameplayAbility;

USTRUCT(BlueprintType)
struct FAIRECompanionAbilitySetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "1", UIMin = "1"))
	int32 AbilityLevel = 1;
};

UCLASS(BlueprintType)
class AI_RE_API UAIRECompanionAbilitySetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool IsAbilitySetValid(FText& OutValidationError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<FAIRECompanionAbilitySetEntry> Abilities;
};

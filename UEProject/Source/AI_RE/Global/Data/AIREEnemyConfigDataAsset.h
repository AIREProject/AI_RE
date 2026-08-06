#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AIREEnemyConfigDataAsset.generated.h"

UCLASS(BlueprintType)
class AI_RE_API UAIREEnemyConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool IsConfigurationValid(FText& OutValidationError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Movement", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm/s"))
	float MovementSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Vitality", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxHealth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Vitality", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialHealth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Vitality", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DeathRemovalDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FlinchThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float FlinchDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float StunThreshold = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float StunDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackStaggerValue = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float AttackCooldownDuration = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float AttackFallbackHitDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float AttackFallbackRecoveryDuration = 0.8f;
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AIRECompanionConfigDataAsset.generated.h"

UCLASS(BlueprintType)
class AI_RE_API UAIRECompanionConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool IsConfigurationValid(FText& OutValidationError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (UIMin = "0.0", Units = "cm/s"))
	float MovementSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (UIMin = "0.0", Units = "cm"))
	float FollowStopDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Movement", meta = (UIMin = "0.0", Units = "cm"))
	float ReturnStartDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (UIMin = "0.0", Units = "cm"))
	float ThreatDetectionDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (UIMin = "0.0", Units = "cm"))
	float MaxChaseDistanceFromPlayer = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Combat", meta = (UIMin = "0.0", Units = "cm"))
	float CombatDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Combat", meta = (UIMin = "0.0", Units = "s"))
	float CombatCooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (UIMin = "0.0"))
	float InitialHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (UIMin = "0.0"))
	float InitialStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxStamina = 100.0f;
};

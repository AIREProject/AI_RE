// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI_REItemEffect.h"
#include "AI_REItemEffect_Consumable.generated.h"

/**
 * Item Effect for restoring character stats (HP, SP, Hunger, Thirst).
 * Can be assigned in DataAssets to provide numeric healing without Blueprints.
 */
UCLASS(DisplayName = "Consumable Status Recovery Effect")
class AI_RE_API UAI_REItemEffect_Consumable : public UAI_REItemEffect
{
	GENERATED_BODY()

public:
	// 체력 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery")
	float HealAmount = 0.0f;

	// 기력 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery")
	float SPRecoveryAmount = 0.0f;

	// 포만감 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery")
	float HungerRecoveryAmount = 0.0f;

	// 수분 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery")
	float ThirstyRecoveryAmount = 0.0f;

	virtual bool ApplyEffect_Implementation(AAI_RECharacterBase* TargetCharacter) override;
};

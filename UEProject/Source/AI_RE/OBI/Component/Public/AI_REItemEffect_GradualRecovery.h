// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI_REItemEffect.h"
#include "AI_REItemEffect_GradualRecovery.generated.h"

/**
 * Item Effect for restoring character stats gradually over time (HoT).
 * Can be assigned in DataAssets to provide numeric healing without Blueprints.
 */
UCLASS(DisplayName = "Gradual Status Recovery Effect")
class AI_RE_API UAI_REItemEffect_GradualRecovery : public UAI_REItemEffect
{
	GENERATED_BODY()

public:
	// 전체 회복 지속 시간 (초 단위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery|Gradual")
	float Duration = 10.0f;

	// 총 체력 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery|Gradual")
	float TotalHealAmount = 0.0f;

	// 총 기력 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery|Gradual")
	float TotalSPAmount = 0.0f;

	// 총 포만감 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery|Gradual")
	float TotalHungerAmount = 0.0f;

	// 총 수분 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery|Gradual")
	float TotalThirstyAmount = 0.0f;

	virtual bool ApplyEffect_Implementation(AAI_RECharacterBase* TargetCharacter) override;
};

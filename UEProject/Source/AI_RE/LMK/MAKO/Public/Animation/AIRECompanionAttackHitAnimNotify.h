#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AIRECompanionAttackHitAnimNotify.generated.h"

UCLASS(meta = (DisplayName = "AIRE Companion Attack Hit"))
class AI_RE_API UAIRECompanionAttackHitAnimNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/**
	 * Zero-based Combo Step index. Variant montages use the cumulative Section
	 * index across all preceding variable-length variants.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0", UIMin = "0"))
	int32 ComboStepIndex = 0;
};

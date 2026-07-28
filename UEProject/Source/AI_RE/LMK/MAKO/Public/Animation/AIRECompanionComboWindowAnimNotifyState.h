#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AIRECompanionComboWindowAnimNotifyState.generated.h"

UCLASS(meta = (DisplayName = "AIRE Companion Combo Window"))
class AI_RE_API UAIRECompanionComboWindowAnimNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** Zero-based index matching the Weapon Definition Combo Steps array. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0", UIMin = "0"))
	int32 ComboStepIndex = 0;
};

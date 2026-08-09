#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AIREEnemyAttackMovementAnimNotifyState.generated.h"

class USkeletalMeshComponent;

/** Drives collision-aware attack movement independently from the damage trace window. */
UCLASS(meta = (DisplayName = "AIRE Enemy Attack Movement Window"))
class AI_RE_API UAIREEnemyAttackMovementAnimNotifyState final
	: public UAnimNotifyState
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

private:
	void RemoveStaleExecutionIds();

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FGuid> ActiveExecutionIds;
};

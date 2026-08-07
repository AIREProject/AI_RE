#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AIREEnemyMeleeTraceAnimNotifyState.generated.h"

class USkeletalMeshComponent;

/** Opens and updates an Enemy attack trace window without owning hit resolution. */
UCLASS(meta = (DisplayName = "AIRE Enemy Melee Trace Window"))
class AI_RE_API UAIREEnemyMeleeTraceAnimNotifyState final
	: public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

private:
	void RemoveStaleExecutionIds();

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FGuid> ActiveExecutionIds;
};

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
	/** Unique index for one damage opportunity inside this montage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0", UIMin = "0"))
	int32 StrikeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaggerScale = 1.0f;

	/** Optional per-strike socket pair, useful for alternating hands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	FName TraceStartSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	FName TraceEndSocket = NAME_None;

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

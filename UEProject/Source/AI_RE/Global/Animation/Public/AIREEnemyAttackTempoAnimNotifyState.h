#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AIREEnemyAttackTempoAnimNotifyState.generated.h"

class USkeletalMeshComponent;

/** Redistributes one Enemy attack's playback time without owning hit resolution. */
UCLASS(meta = (DisplayName = "AIRE Enemy Attack Tempo Window"))
class AI_RE_API UAIREEnemyAttackTempoAnimNotifyState final
	: public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Label matching the damage opportunity that follows this anticipation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0", UIMin = "0"))
	int32 StrikeIndex = 0;

	/** Montage position at which anticipation switches to the strike rate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float StrikeStartTime = 0.0f;

	/** Authored montage position at which the tempo window ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float WindowEndTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AnticipationPlayRateMultiplier = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float StrikePlayRateMultiplier = 1.60f;

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

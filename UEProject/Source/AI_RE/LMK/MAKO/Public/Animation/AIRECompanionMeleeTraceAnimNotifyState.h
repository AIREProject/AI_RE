#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AIRECompanionMeleeTraceAnimNotifyState.generated.h"

class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EAIRECompanionMeleeTraceMode : uint8
{
	BasicAttack,
	CombatSkill
};

/** Requests spatial trace samples while an active MAKO attack window is open. */
UCLASS(meta = (DisplayName = "AIRE Companion Melee Trace Window"))
class AI_RE_API UAIRECompanionMeleeTraceAnimNotifyState final
	: public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attack")
	EAIRECompanionMeleeTraceMode TraceMode =
		EAIRECompanionMeleeTraceMode::BasicAttack;

	/**
	 * Used only for Basic Attack combo sections. Variant montages use the
	 * cumulative Section index across all preceding variable-length variants.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Attack", meta = (ClampMin = "0", UIMin = "0"))
	int32 ComboStepIndex = 0;

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
};

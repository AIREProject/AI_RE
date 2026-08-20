// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AI_REAnimNotifyState_ActiveHit.generated.h"

/**
 * 활성화된 동안 어빌리티 시스템에 ActiveHit.Start / ActiveHit.End 이벤트를 발송하여
 * 연속 판정(다단 히트/트레이스)이 가능하게 만들어주는 NotifyState 입니다.
 */
UCLASS()
class AI_RE_API UAI_REAnimNotifyState_ActiveHit : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

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
};

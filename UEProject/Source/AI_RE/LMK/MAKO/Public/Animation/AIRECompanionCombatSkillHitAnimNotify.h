#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AIRECompanionCombatSkillHitAnimNotify.generated.h"

UCLASS(meta = (DisplayName = "AIRE Companion Combat Skill Hit"))
class AI_RE_API UAIRECompanionCombatSkillHitAnimNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};

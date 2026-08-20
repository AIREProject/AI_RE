#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AI_REAnimNotify_SendGameplayEvent.generated.h"

UCLASS()
class AI_RE_API UAI_REAnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAI_REAnimNotify_SendGameplayEvent();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTag EventTag;
};

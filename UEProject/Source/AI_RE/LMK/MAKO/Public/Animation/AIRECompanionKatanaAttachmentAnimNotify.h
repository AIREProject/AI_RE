#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AIRECompanionKatanaAttachmentAnimNotify.generated.h"

UCLASS(meta = (DisplayName = "AIRE Companion Katana Attachment"))
class AI_RE_API UAIRECompanionKatanaAttachmentAnimNotify final
	: public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	bool ShouldAttachToHand() const { return bAttachToHand; }
	void SetAttachToHand(bool bInAttachToHand) { bAttachToHand = bInAttachToHand; }

private:
	UPROPERTY(EditAnywhere, Category = "AIRE|Equipment|Katana")
	bool bAttachToHand = true;
};

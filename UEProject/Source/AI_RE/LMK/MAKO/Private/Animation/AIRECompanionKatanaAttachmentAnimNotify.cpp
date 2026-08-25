#include "Animation/AIRECompanionKatanaAttachmentAnimNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Core/AIRECompanionCharacter.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"

void UAIRECompanionKatanaAttachmentAnimNotify::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AAIRECompanionCharacter* Companion = IsValid(MeshComp)
		? Cast<AAIRECompanionCharacter>(MeshComp->GetOwner())
		: nullptr;
	UAIRECompanionEquipmentComponent* Equipment = IsValid(Companion)
		? Companion->GetEquipmentComponent()
		: nullptr;
	if (IsValid(Equipment))
	{
		Equipment->SetKatanaBladeDrawn(bAttachToHand);
	}
}

FString UAIRECompanionKatanaAttachmentAnimNotify::
GetNotifyName_Implementation() const
{
	return bAttachToHand
		? TEXT("Katana To Hand")
		: TEXT("Katana To Sheath");
}

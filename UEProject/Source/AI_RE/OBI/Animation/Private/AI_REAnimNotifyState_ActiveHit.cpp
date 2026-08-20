// Copyright MixUpProject. All Rights Reserved.

#include "AI_REAnimNotifyState_ActiveHit.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

void UAI_REAnimNotifyState_ActiveHit::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		FGameplayEventData Payload;
		Payload.Instigator = MeshComp->GetOwner();
		Payload.Target = MeshComp->GetOwner();

		// ActiveHit 시작 이벤트 발송
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			MeshComp->GetOwner(),
			FGameplayTag::RequestGameplayTag(FName("Event.Combat.ActiveHit.Start")),
			Payload
		);
	}
}

void UAI_REAnimNotifyState_ActiveHit::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		FGameplayEventData Payload;
		Payload.Instigator = MeshComp->GetOwner();
		Payload.Target = MeshComp->GetOwner();

		// ActiveHit 종료 이벤트 발송
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			MeshComp->GetOwner(),
			FGameplayTag::RequestGameplayTag(FName("Event.Combat.ActiveHit.End")),
			Payload
		);
	}
}

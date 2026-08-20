#include "AI_REAnimNotify_SendGameplayEvent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

UAI_REAnimNotify_SendGameplayEvent::UAI_REAnimNotify_SendGameplayEvent()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 0, 0, 255); // 빨간색 점
#endif
}

void UAI_REAnimNotify_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner() && EventTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()))
		{
			FGameplayEventData Payload;
			Payload.EventTag = EventTag;
			ASC->HandleGameplayEvent(EventTag, &Payload);
		}
	}
}

#include "AI_REAnimNotifyState_ComboWindow.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

UAI_REAnimNotifyState_ComboWindow::UAI_REAnimNotifyState_ComboWindow()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 128, 0, 255); // 주황색 띠
#endif
}

void UAI_REAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()))
		{
			FGameplayEventData Payload;
			Payload.EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.ComboWindowOpen"));
			ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		}
	}
}

void UAI_REAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()))
		{
			FGameplayEventData Payload;
			Payload.EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.ComboWindowClose"));
			ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		}
	}
}

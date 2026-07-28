#include "Animation/AIRECompanionComboWindowAnimNotifyState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
void SendComboWindowEvent(
	USkeletalMeshComponent* MeshComp,
	const FGameplayTag EventTag,
	const int32 ComboStepIndex)
{
	if (!IsValid(MeshComp) || !IsValid(MeshComp->GetOwner()))
	{
		return;
	}

	FGameplayEventData WindowEvent;
	WindowEvent.EventTag = EventTag;
	WindowEvent.Instigator = MeshComp->GetOwner();
	WindowEvent.EventMagnitude = static_cast<float>(ComboStepIndex);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		MeshComp->GetOwner(),
		EventTag,
		WindowEvent);
}
}

void UAIRECompanionComboWindowAnimNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SendComboWindowEvent(
		MeshComp,
		AIRECompanionGameplayTags::EventAttackComboWindowBegin,
		ComboStepIndex);
}

void UAIRECompanionComboWindowAnimNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	SendComboWindowEvent(
		MeshComp,
		AIRECompanionGameplayTags::EventAttackComboWindowEnd,
		ComboStepIndex);
}

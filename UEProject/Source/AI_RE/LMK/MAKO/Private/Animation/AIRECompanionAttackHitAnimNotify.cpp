#include "Animation/AIRECompanionAttackHitAnimNotify.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

void UAIRECompanionAttackHitAnimNotify::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp) || !IsValid(MeshComp->GetOwner()))
	{
		return;
	}

	FGameplayEventData HitEvent;
	HitEvent.EventTag = AIRECompanionGameplayTags::EventAttackHit;
	HitEvent.Instigator = MeshComp->GetOwner();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		MeshComp->GetOwner(),
		AIRECompanionGameplayTags::EventAttackHit,
		HitEvent);
}

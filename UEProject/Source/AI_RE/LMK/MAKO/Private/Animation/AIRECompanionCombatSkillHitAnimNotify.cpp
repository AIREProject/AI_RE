#include "Animation/AIRECompanionCombatSkillHitAnimNotify.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

void UAIRECompanionCombatSkillHitAnimNotify::Notify(
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
	HitEvent.EventTag = AIRECompanionGameplayTags::EventCombatSkillHit;
	HitEvent.Instigator = MeshComp->GetOwner();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		MeshComp->GetOwner(),
		AIRECompanionGameplayTags::EventCombatSkillHit,
		HitEvent);
}

#include "Animation/AIRECompanionMeleeTraceAnimNotifyState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
enum class EAIRECompanionTraceNotifyPhase : uint8
{
	Begin,
	Sample,
	End
};

FGameplayTag ResolveTraceEventTag(
	const EAIRECompanionMeleeTraceMode TraceMode,
	const EAIRECompanionTraceNotifyPhase Phase)
{
	if (TraceMode == EAIRECompanionMeleeTraceMode::CombatSkill)
	{
		switch (Phase)
		{
		case EAIRECompanionTraceNotifyPhase::Begin:
			return AIRECompanionGameplayTags::EventCombatSkillTraceBegin;
		case EAIRECompanionTraceNotifyPhase::Sample:
			return AIRECompanionGameplayTags::EventCombatSkillTraceSample;
		case EAIRECompanionTraceNotifyPhase::End:
			return AIRECompanionGameplayTags::EventCombatSkillTraceEnd;
		default:
			return FGameplayTag();
		}
	}

	switch (Phase)
	{
	case EAIRECompanionTraceNotifyPhase::Begin:
		return AIRECompanionGameplayTags::EventAttackTraceBegin;
	case EAIRECompanionTraceNotifyPhase::Sample:
		return AIRECompanionGameplayTags::EventAttackTraceSample;
	case EAIRECompanionTraceNotifyPhase::End:
		return AIRECompanionGameplayTags::EventAttackTraceEnd;
	default:
		return FGameplayTag();
	}
}

void SendTraceEvent(
	USkeletalMeshComponent* MeshComp,
	const EAIRECompanionMeleeTraceMode TraceMode,
	const EAIRECompanionTraceNotifyPhase Phase,
	const int32 ComboStepIndex)
{
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	const FGameplayTag EventTag = ResolveTraceEventTag(TraceMode, Phase);
	if (!IsValid(Owner) || !EventTag.IsValid())
	{
		return;
	}

	FGameplayEventData TraceEvent;
	TraceEvent.EventTag = EventTag;
	TraceEvent.Instigator = Owner;
	TraceEvent.OptionalObject = MeshComp;
	TraceEvent.EventMagnitude = static_cast<float>(ComboStepIndex);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Owner,
		EventTag,
		TraceEvent);
}
}

void UAIRECompanionMeleeTraceAnimNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SendTraceEvent(
		MeshComp,
		TraceMode,
		EAIRECompanionTraceNotifyPhase::Begin,
		ComboStepIndex);
}

void UAIRECompanionMeleeTraceAnimNotifyState::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	SendTraceEvent(
		MeshComp,
		TraceMode,
		EAIRECompanionTraceNotifyPhase::Sample,
		ComboStepIndex);
}

void UAIRECompanionMeleeTraceAnimNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	SendTraceEvent(
		MeshComp,
		TraceMode,
		EAIRECompanionTraceNotifyPhase::End,
		ComboStepIndex);
}

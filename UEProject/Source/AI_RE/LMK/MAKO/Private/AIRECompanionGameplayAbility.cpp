#include "AIRECompanionGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AIRECompanionGameplayTags.h"

UAIRECompanionGameplayAbility::UAIRECompanionGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationBlockedTags.AddTag(AIRECompanionGameplayTags::StateDisabled);
}

bool UAIRECompanionGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateDisabled))
	{
		return false;
	}

	return Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags);
}

void UAIRECompanionGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	EventTarget.Reset();
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

bool UAIRECompanionGameplayAbility::InitializeEventTarget(
	const FGameplayEventData* TriggerEventData)
{
	EventTarget.Reset();
	if (!TriggerEventData || !IsValid(TriggerEventData->Target.Get()))
	{
		return false;
	}

	EventTarget = const_cast<AActor*>(TriggerEventData->Target.Get());
	return EventTarget.IsValid();
}

AActor* UAIRECompanionGameplayAbility::GetEventTarget() const
{
	return EventTarget.Get();
}

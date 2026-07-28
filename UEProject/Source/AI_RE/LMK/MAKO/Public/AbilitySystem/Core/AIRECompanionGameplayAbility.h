#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AIRECompanionGameplayAbility.generated.h"

UCLASS(Abstract)
class AI_RE_API UAIRECompanionGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UAIRECompanionGameplayAbility();

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	bool InitializeEventTarget(const FGameplayEventData* TriggerEventData);
	AActor* GetEventTarget() const;

private:
	TWeakObjectPtr<AActor> EventTarget;
};

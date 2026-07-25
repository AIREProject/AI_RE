#pragma once

#include "CoreMinimal.h"
#include "AIRECompanionGameplayAbility.h"
#include "AIRECompanionMeleeAttackAbility.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

UCLASS()
class AI_RE_API UAIRECompanionMeleeAttackAbility : public UAIRECompanionGameplayAbility
{
	GENERATED_BODY()

public:
	UAIRECompanionMeleeAttackAbility();

protected:
	virtual bool CheckCost(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UAIRECompanionWeaponDefinitionDataAsset* GetWeaponDefinition(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsTargetValidForAttack(const AActor* TargetActor) const;
	bool IsTargetInRange(const AActor* TargetActor) const;
	void StartHitEventWait();
	void StartFallbackAttack();
	void SendFallbackHitEvent();
	void ScheduleFallbackRecovery();
	void FinishFallbackAttack();
	void FinishAbility(bool bWasCancelled);

	UFUNCTION()
	void HandleHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> ActiveWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitEventTask;

	FTimerHandle FallbackHitTimerHandle;
	FTimerHandle FallbackRecoveryTimerHandle;
	float AttackRange = 0.0f;
	bool bHitConsumed = false;
	bool bIsEnding = false;
};

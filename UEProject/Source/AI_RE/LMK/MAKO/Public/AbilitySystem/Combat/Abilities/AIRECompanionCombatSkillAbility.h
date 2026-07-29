#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Core/AIRECompanionGameplayAbility.h"
#include "AIRECompanionCombatSkillAbility.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

UCLASS()
class AI_RE_API UAIRECompanionCombatSkillAbility : public UAIRECompanionGameplayAbility
{
	GENERATED_BODY()

public:
	UAIRECompanionCombatSkillAbility();

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
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
	bool IsTargetValidForSkill(const AActor* TargetActor) const;
	bool IsTargetInRange(const AActor* TargetActor) const;
	void FaceTarget(const AActor* TargetActor) const;
	bool IsActiveExecutionValid() const;
	bool ResolveSkillHit();
	void StartHitEventWait();
	void StartFallback();
	void SendFallbackHitEvent();
	void FinishFallback();
	void SendTransitionEvent(FGameplayTag EventTag, bool bCompleted);
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
	bool bHitConsumed = false;
	bool bTransitionStarted = false;
	bool bIsEnding = false;
};

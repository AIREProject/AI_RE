#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Core/AIRECompanionGameplayAbility.h"
#include "AIRECompanionMeleeAttackAbility.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;

UCLASS()
class AI_RE_API UAIRECompanionMeleeAttackAbility : public UAIRECompanionGameplayAbility
{
	GENERATED_BODY()

public:
	UAIRECompanionMeleeAttackAbility();

protected:
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
	int32 GetAttackStepCount() const;
	bool IsAttackStepIndexValid(int32 StepIndex) const;
	float GetAttackStepDamage(int32 StepIndex) const;
	FName GetAttackStepMontageSection(int32 StepIndex) const;
	bool IsTargetValidForAttack(const AActor* TargetActor) const;
	bool IsTargetInRange(const AActor* TargetActor) const;
	void FaceTarget(const AActor* TargetActor) const;
	bool IsActiveExecutionValid() const;
	bool AreComboMontageSectionsValid(const UAnimMontage* AttackMontage) const;
	bool TryStartNextStep();
	bool StartAttackMontage();
	bool ResumeAfterCombatSkill();
	bool TryGetEventStepIndex(const FGameplayEventData& Payload, int32& OutStepIndex) const;
	bool ResolveCurrentStepHit();
	void ResetCurrentStepState();
	void StartHitEventWait();
	void StartComboWindowEventWait();
	void StartCombatSkillTransitionEventWait();
	void SetSkillCancelWindowTag(bool bEnabled);
	void StartFallbackStep();
	void SendFallbackHitEvent();
	void ScheduleFallbackRecovery();
	void FinishFallbackStep();
	void FinishAbility(bool bWasCancelled);

	UFUNCTION()
	void HandleHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleComboWindowEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleCombatSkillTransitionEvent(FGameplayEventData Payload);

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

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboWindowEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CombatSkillTransitionEventTask;

	FTimerHandle FallbackHitTimerHandle;
	FTimerHandle FallbackRecoveryTimerHandle;
	float AttackRange = 0.0f;
	int32 CurrentStepIndex = INDEX_NONE;
	int32 ResumeStepIndex = INDEX_NONE;
	bool bCurrentStepHitConsumed = false;
	bool bComboWindowOpen = false;
	bool bNextStepQueued = false;
	bool bUsingFallback = false;
	bool bSuspendedForCombatSkill = false;
	bool bSkillCancelWindowTagApplied = false;
	bool bIsEnding = false;
};

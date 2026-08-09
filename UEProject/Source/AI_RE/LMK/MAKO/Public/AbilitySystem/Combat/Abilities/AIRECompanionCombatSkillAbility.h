#pragma once

#include "CoreMinimal.h"
#include "AIRECombatDamageTypes.h"
#include "AIRECombatMeleeTraceResolver.h"
#include "AbilitySystem/Core/AIRECompanionGameplayAbility.h"
#include "AIRECompanionCombatSkillAbility.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class USkeletalMeshComponent;

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
	EAIRECombatMeleeTraceResult SampleSkillTrace(
		USkeletalMeshComponent* MeshComponent,
		FHitResult& OutTargetHit);
	bool CommitSkillHit(const FHitResult& TargetHit);
	void ResolveSkillTraceSample(
		EAIRECombatMeleeTraceResult TraceResult,
		const FHitResult& TargetHit);
	bool TryBeginSkillTrace(USkeletalMeshComponent* MeshComponent);
	void CloseSkillTrace();
	void StartHitEventWait();
	void StartTraceEventWait();
	void StartFallback();
	void SendFallbackHitEvent();
	void FinishFallback();
	void SendTransitionEvent(FGameplayTag EventTag, bool bCompleted);
	void FinishAbility(bool bWasCancelled);

	UFUNCTION()
	void HandleHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTraceEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

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
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceEventTask;

	FTimerHandle FallbackHitTimerHandle;
	FTimerHandle FallbackRecoveryTimerHandle;
	FGuid ActiveExecutionId;
	TWeakObjectPtr<USkeletalMeshComponent> ActiveTraceMesh;
	FVector PreviousTraceStart = FVector::ZeroVector;
	FVector PreviousTraceEnd = FVector::ZeroVector;
	FName ActiveTraceStartSocket = NAME_None;
	FName ActiveTraceEndSocket = NAME_None;
	float ActiveDamage = 0.0f;
	float ActiveStaggerValue = 0.0f;
	float ActiveAttackRange = 0.0f;
	float ActiveTraceRadius = 0.0f;
	TEnumAsByte<ECollisionChannel> ActiveTraceChannel = ECC_MAX;
	EAIRECombatTargetingMode ActiveTargetingMode =
		EAIRECombatTargetingMode::SingleTarget;
	bool bHitConsumed = false;
	bool bPointSampleConsumed = false;
	bool bTraceWindowOpen = false;
	bool bTraceWindowEverOpened = false;
	bool bUsingFallback = false;
	bool bTransitionStarted = false;
	bool bIsEnding = false;
};

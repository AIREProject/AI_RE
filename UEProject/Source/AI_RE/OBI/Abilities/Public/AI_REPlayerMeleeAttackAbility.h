#pragma once

#include "CoreMinimal.h"
#include "AI_REPlayerGameplayAbility.h"
#include "AIRECombatMeleeTraceResolver.h"
#include "AI_REPlayerMeleeAttackAbility.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

UCLASS()
class AI_RE_API UAI_REPlayerMeleeAttackAbility : public UAI_REPlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UAI_REPlayerMeleeAttackAbility();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float TraceDistance = 300.0f;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleMontageCompleted();
	
	UFUNCTION()
	void HandleMontageInterrupted();

	UFUNCTION()
	void HandleHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTraceEvent(FGameplayEventData Payload);

	void PerformTraceHit();

	void TryBeginCurrentStepTrace(USceneComponent* MeshComponent);
	EAIRECombatMeleeTraceResult SampleCurrentStepCombatTrace(USceneComponent* MeshComponent, FHitResult& OutTargetHit);
	void ResolveCurrentStepTraceSample(EAIRECombatMeleeTraceResult TraceResult, const FHitResult& TargetHit);
	void CloseCurrentStepTrace();

	UFUNCTION()
	void HandleComboWindowOpen(FGameplayEventData Payload);

	UFUNCTION()
	void HandleComboWindowClose(FGameplayEventData Payload);

	UFUNCTION()
	void HandleComboInput(FGameplayEventData Payload);

	void ProcessHit(const FHitResult& HitResult, float Dmg, ACharacter* Character);
	void TryComboTransition();

	int32 CurrentComboIndex = 1;
	bool bIsComboWindowOpen = false;
	bool bHasComboInput = false;
	bool bIsActiveHitEnded = false;

	bool bUseFallbackTrace = false;
	bool bTraceWindowOpen = false;
	FVector PreviousTraceStart = FVector::ZeroVector;
	FVector PreviousTraceEnd = FVector::ZeroVector;
	FName CurrentTraceStartSocket = NAME_None;
	FName CurrentTraceEndSocket = NAME_None;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	// 콤보 윈도우 타이밍 감지 태스크
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> ActiveTraceMesh;

	UPROPERTY()
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> HitEventTask;
	UPROPERTY()
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> TraceEventTask;
	UPROPERTY()
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> TraceSampleTask;
	UPROPERTY()
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> TraceEndTask;

	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ActiveHitStartTask;

	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ActiveHitEndTask;

	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ComboWindowOpenTask;
	
	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ComboWindowCloseTask;
	
	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ComboInputTask;

	// 현재 스윙에서 이미 타격한 적들을 기억하는 배열 (중복 타격 방지)
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;

};

#pragma once

#include "CoreMinimal.h"
#include "AI_REPlayerGameplayAbility.h"
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
	void HandleActiveHitStart(FGameplayEventData Payload);

	UFUNCTION()
	void HandleActiveHitEnd(FGameplayEventData Payload);

	UFUNCTION()
	void HandleComboWindowOpen(FGameplayEventData Payload);

	UFUNCTION()
	void HandleComboWindowClose(FGameplayEventData Payload);

	UFUNCTION()
	void HandleComboInput(FGameplayEventData Payload);

	void PerformTraceHit();
	void ProcessHit(AActor* HitActor, float Dmg, ACharacter* Character);
	void TryComboTransition();

	int32 CurrentComboIndex = 1;
	bool bIsComboWindowOpen = false;
	bool bHasComboInput = false;
	bool bIsActiveHitEnded = false;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	// 콤보 윈도우 타이밍 감지 태스크
	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ComboWindowOpenTask;
	
	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ComboWindowCloseTask;
	
	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ComboInputTask;

	// 연속 타격(Active Hit) 감지 태스크
	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ActiveHitStartTask;

	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ActiveHitEndTask;

	// 지속 판정용 타이머 핸들
	FTimerHandle ActiveHitTimerHandle;

	// 현재 스윙에서 이미 타격한 적들을 기억하는 배열 (중복 타격 방지)
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;

	// 기존의 단발성 타격 이벤트 (가벼운 무기/맨손용)
	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> HitEventTask;
};

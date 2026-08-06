#pragma once

#include "CoreMinimal.h"
#include "AIRECombatDamageTypes.h"
#include "Components/ActorComponent.h"
#include "AIREEnemyAttackComponent.generated.h"

class ACharacter;
class UAnimMontage;

USTRUCT(BlueprintType)
struct AI_RE_API FAIREEnemyAttackSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	bool bOpportunityOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	bool bHitCommitted = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	bool bDamageCancelled = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	EAIRECombatTargetingMode TargetingMode =
		EAIRECombatTargetingMode::SingleTarget;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	TObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	FGuid ExecutionId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREEnemyAttackStartedSignature,
	AActor*,
	Target,
	FGuid,
	ExecutionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIREEnemyAttackOpportunityClosedSignature,
	FGuid,
	ExecutionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREEnemyAttackFinishedSignature,
	FGuid,
	ExecutionId,
	bool,
	bDamageCommitted);

UCLASS(ClassGroup = (AIRE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIREEnemyAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIREEnemyAttackComponent();

	bool InitializeAttack();
	void ShutdownAttack();
	void ConfigureDefaults(
		float InAttackRange,
		float InDamage,
		float InStaggerValue,
		float InCooldownDuration,
		float InFallbackHitDelay,
		float InFallbackRecoveryDuration);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Attack")
	bool TryStartMeleeAttack(AActor* Target);

	/** Call from an attack AnimNotify. The fallback timer calls the same seam. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Attack")
	bool CommitActiveMeleeHit();

	bool TryCancelDamageForAggroSwap(const FGuid& ExecutionId);
	void CancelCurrentAttack();

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Attack")
	FAIREEnemyAttackSnapshot GetAttackSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Attack")
	float GetAttackRange() const;

	bool IsTargetWithinAttackRange(const AActor* Target) const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Enemy|Attack")
	FAIREEnemyAttackStartedSignature OnAttackStarted;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Enemy|Attack")
	FAIREEnemyAttackOpportunityClosedSignature OnOpportunityClosed;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Enemy|Attack")
	FAIREEnemyAttackFinishedSignature OnAttackFinished;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleFallbackHit();
	void CloseOpportunity();
	void FinishAttack();

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damage = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaggerValue = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Attack")
	EAIRECombatTargetingMode TargetingMode =
		EAIRECombatTargetingMode::SingleTarget;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CooldownDuration = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float FallbackHitDelay = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float FallbackRecoveryDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	TWeakObjectPtr<ACharacter> OwnerCharacter;
	TWeakObjectPtr<AActor> AttackTarget;
	FTimerHandle HitTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FGuid ActiveExecutionId;
	double NextAllowedAttackTime = 0.0;
	bool bAttackActive = false;
	bool bOpportunityOpen = false;
	bool bHitCommitted = false;
	bool bDamageApplied = false;
	bool bDamageCancelled = false;
	bool bFinishing = false;
};

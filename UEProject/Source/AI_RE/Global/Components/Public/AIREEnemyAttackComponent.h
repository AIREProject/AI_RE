#pragma once

#include "CoreMinimal.h"
#include "AIRECombatDamageTypes.h"
#include "AIREEnemyConfigDataAsset.h"
#include "Components/ActorComponent.h"
#include "AIREEnemyAttackComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;
class USkeletalMeshComponent;

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
		float InFallbackRecoveryDuration,
		const FAIREEnemyMeleeTraceSettings& InMeleeTraceSettings);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Attack")
	bool TryStartMeleeAttack(AActor* Target);

	/** Deprecated point-notify seam. It performs a spatial sample and never commits by target alone. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Attack", meta = (DeprecatedFunction, DeprecationMessage = "Use AIREEnemyMeleeTraceAnimNotifyState for a swept trace window."))
	bool CommitActiveMeleeHit();

	void BeginMeleeTraceWindow(const FGuid& ExecutionId);
	void UpdateMeleeTraceWindow(const FGuid& ExecutionId);
	void EndMeleeTraceWindow(const FGuid& ExecutionId);

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
	enum class ETraceSampleResult : uint8
	{
		NoHit,
		TargetHit,
		Blocked
	};

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleFallbackHit();
	void HandleRecoveryExpired();
	bool IsTraceCallbackCurrent(const FGuid& ExecutionId) const;
	bool CanResolveActiveHit() const;
	bool CommitResolvedHit(const FHitResult& HitResult);
	ETraceSampleResult PerformSocketTraceSample(
		USkeletalMeshComponent* MeshComponent,
		FHitResult& OutTargetHit);
	ETraceSampleResult PerformFallbackTraceSample(FHitResult& OutTargetHit) const;
	ETraceSampleResult SweepTraceSegments(
		const TArray<TPair<FVector, FVector>>& Segments,
		FHitResult& OutTargetHit) const;
	bool SweepTraceSegment(
		const FVector& Start,
		const FVector& End,
		FHitResult& OutHit) const;
	void ResolveTraceSample(ETraceSampleResult Result, const FHitResult& TargetHit);
	void CloseTraceWindow();
	void ResetTraceState();
	void ClearMontageEndDelegate();
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
	TWeakObjectPtr<USkeletalMeshComponent> ActiveTraceMesh;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
	FTimerHandle HitTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FGuid ActiveExecutionId;
	FGuid TraceWindowExecutionId;
	FAIREEnemyMeleeTraceSettings MeleeTraceSettings;
	FAIREEnemyMeleeTraceSettings ActiveMeleeTraceSettings;
	FVector AttackForward = FVector::ForwardVector;
	FVector PreviousTraceStart = FVector::ZeroVector;
	FVector PreviousTraceEnd = FVector::ZeroVector;
	double NextAllowedAttackTime = 0.0;
	bool bAttackActive = false;
	bool bOpportunityOpen = false;
	bool bHitCommitted = false;
	bool bDamageApplied = false;
	bool bDamageCancelled = false;
	bool bFinishing = false;
	bool bMontagePlayed = false;
	bool bTraceWindowOpen = false;
	bool bTraceWindowEverOpened = false;
	bool bUseSocketTrace = false;
};

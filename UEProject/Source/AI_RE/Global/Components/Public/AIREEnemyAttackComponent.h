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

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	FName PatternId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	float PlayRate = 1.0f;

	/** True when the selected pattern owns code-driven forward movement. */
	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	bool bGapCloser = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	int32 CommittedStrikeCount = 0;
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
	void ConfigureAttackPatterns(
		const TArray<FAIREEnemyAttackPattern>& InAttackPatterns);
	void ResetAttackSequence();
	bool RequiresNonGapCloserFollowUp() const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Attack")
	bool TryStartMeleeAttack(AActor* Target);

	/** Deprecated point-notify seam. It performs a spatial sample and never commits by target alone. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Attack", meta = (DeprecatedFunction, DeprecationMessage = "Use AIREEnemyMeleeTraceAnimNotifyState for a swept trace window."))
	bool CommitActiveMeleeHit();

	void BeginMeleeTraceWindow(
		const FGuid& ExecutionId,
		int32 StrikeIndex = 0,
		float DamageScale = 1.0f,
		float StaggerScale = 1.0f,
		FName TraceStartSocket = NAME_None,
		FName TraceEndSocket = NAME_None,
		float TraceWindowDuration = 0.0f);
	void UpdateMeleeTraceWindow(
		const FGuid& ExecutionId,
		int32 StrikeIndex = 0);
	void EndMeleeTraceWindow(
		const FGuid& ExecutionId,
		int32 StrikeIndex = 0);
	void BeginAttackMovementWindow(
		const FGuid& ExecutionId,
		float MovementWindowDuration);
	void EndAttackMovementWindow(const FGuid& ExecutionId);

	bool TryCancelDamageForAggroSwap(const FGuid& ExecutionId);
	void CancelCurrentAttack();

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Attack")
	FAIREEnemyAttackSnapshot GetAttackSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Attack")
	float GetAttackRange() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Attack")
	float GetPreferredAttackRange() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Attack")
	float GetRemainingAttackCooldown() const;

	float GetTargetSurfaceDistance(const AActor* Target) const;

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
	bool IsTraceCallbackCurrent(
		const FGuid& ExecutionId,
		int32 StrikeIndex) const;
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
	void PrepareFallbackStrike();
	const FAIREEnemyAttackPattern* SelectAttackPattern(
		const AActor* Target) const;
	float GetSurfaceDistanceToTarget(const AActor* Target) const;
	float GetOwnerHealthRatio() const;
	void StartAttackMovement(float TraceWindowDuration);
	void StopAttackMovement();
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

	UPROPERTY(Transient)
	TArray<FAIREEnemyAttackPattern> AttackPatterns;
	TArray<FName> RecentPatternIds;
	TMap<FName, double> PatternNextAllowedTimes;
	bool bRequiresNonGapCloserFollowUp = false;

	TWeakObjectPtr<ACharacter> OwnerCharacter;
	TWeakObjectPtr<AActor> AttackTarget;
	TWeakObjectPtr<USkeletalMeshComponent> ActiveTraceMesh;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
	FTimerHandle HitTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FGuid ActiveExecutionId;
	FGuid TraceWindowExecutionId;
	FAIREEnemyMeleeTraceSettings MeleeTraceSettings;
	FAIREEnemyMeleeTraceSettings ActiveAttackTraceSettings;
	FAIREEnemyMeleeTraceSettings ActiveMeleeTraceSettings;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAttackMontage;
	FName ActivePatternId = NAME_None;
	float ActivePlayRate = 1.0f;
	float ActivePatternDamageScale = 1.0f;
	float ActivePatternStaggerScale = 1.0f;
	float ActiveCooldownScale = 1.0f;
	float ActiveForwardMoveDistance = 0.0f;
	float ActiveStrikeDamageScale = 1.0f;
	float ActiveStrikeStaggerScale = 1.0f;
	uint16 ActiveMovementRootMotionSourceId = 0;
	int32 ActiveStrikeIndex = INDEX_NONE;
	TSet<int32> CommittedStrikeIndices;
	TMap<int32, FGuid> StrikeExecutionIds;
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
	bool bAttackMovementWindowEverOpened = false;
	bool bUseSocketTrace = false;
	bool bAttackMovementStarted = false;
};

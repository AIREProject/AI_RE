#include "AIREEnemyAttackComponent.h"

#include "AIRECombatDamageSubsystem.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AIRECombatMeleeTraceResolver.h"
#include "AIREEnemyVitalityComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING
DEFINE_LOG_CATEGORY_STATIC(LogAIREEnemyAttack, Log, All);
#endif

UAIREEnemyAttackComponent::UAIREEnemyAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIREEnemyAttackComponent::InitializeAttack()
{
	ShutdownAttack();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character))
	{
		return false;
	}
	OwnerCharacter = Character;
	return true;
}

void UAIREEnemyAttackComponent::ShutdownAttack()
{
	CancelCurrentAttack();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}
	ClearMontageEndDelegate();
	ResetTraceState();
	PatternNextAllowedTimes.Reset();
	ResetAttackSequence();
	OwnerCharacter.Reset();
	OnAttackStarted.Clear();
	OnOpportunityClosed.Clear();
	OnAttackFinished.Clear();
}

void UAIREEnemyAttackComponent::ConfigureDefaults(
	const float InAttackRange,
	const float InDamage,
	const float InStaggerValue,
	const float InCooldownDuration,
	const float InFallbackHitDelay,
	const float InFallbackRecoveryDuration,
	const FAIREEnemyMeleeTraceSettings& InMeleeTraceSettings)
{
	if (FMath::IsFinite(InAttackRange) && InAttackRange >= 0.0f)
	{
		AttackRange = InAttackRange;
	}
	if (FMath::IsFinite(InDamage) && InDamage >= 0.0f)
	{
		Damage = InDamage;
	}
	if (FMath::IsFinite(InStaggerValue) && InStaggerValue >= 0.0f)
	{
		StaggerValue = InStaggerValue;
	}
	if (FMath::IsFinite(InCooldownDuration) && InCooldownDuration >= 0.0f)
	{
		CooldownDuration = InCooldownDuration;
	}
	if (FMath::IsFinite(InFallbackHitDelay) && InFallbackHitDelay > 0.0f)
	{
		FallbackHitDelay = InFallbackHitDelay;
	}
	if (FMath::IsFinite(InFallbackRecoveryDuration)
		&& InFallbackRecoveryDuration > 0.0f)
	{
		FallbackRecoveryDuration = InFallbackRecoveryDuration;
	}

	const bool bHasTraceStartSocket =
		!InMeleeTraceSettings.TraceStartSocket.IsNone();
	const bool bHasTraceEndSocket =
		!InMeleeTraceSettings.TraceEndSocket.IsNone();
	const bool bValidTraceSettings =
		bHasTraceStartSocket == bHasTraceEndSocket
		&& FMath::IsFinite(InMeleeTraceSettings.TraceRadius)
		&& InMeleeTraceSettings.TraceRadius > 0.0f
		&& FMath::IsFinite(InMeleeTraceSettings.FallbackTraceDistance)
		&& InMeleeTraceSettings.FallbackTraceDistance
			>= InMeleeTraceSettings.TraceRadius
		&& InMeleeTraceSettings.TraceChannel.GetValue() < ECC_MAX;
	if (bValidTraceSettings)
	{
		MeleeTraceSettings = InMeleeTraceSettings;
	}
}

void UAIREEnemyAttackComponent::ConfigureAttackPatterns(
	const TArray<FAIREEnemyAttackPattern>& InAttackPatterns)
{
	AttackPatterns = InAttackPatterns;
	PatternNextAllowedTimes.Reset();
	ResetAttackSequence();
}

void UAIREEnemyAttackComponent::ResetAttackSequence()
{
	RecentPatternIds.Reset();
	bRequiresNonGapCloserFollowUp = false;
}

bool UAIREEnemyAttackComponent::RequiresNonGapCloserFollowUp() const
{
	return bRequiresNonGapCloserFollowUp;
}

bool UAIREEnemyAttackComponent::TryStartMeleeAttack(AActor* Target)
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !OwnerCharacter.IsValid()
		|| bAttackActive
		|| World->GetTimeSeconds() < NextAllowedAttackTime
		|| !FMath::IsFinite(Damage)
		|| !FMath::IsFinite(StaggerValue)
		|| (Damage <= 0.0f && StaggerValue <= 0.0f)
		|| !IsTargetWithinAttackRange(Target)
		|| !AIRECombatDamageTarget::IsAlive(Target))
	{
		return false;
	}

	const float TargetDistance = GetSurfaceDistanceToTarget(Target);
	const FAIREEnemyAttackPattern* SelectedPattern =
		SelectAttackPattern(Target);
	if (!SelectedPattern && TargetDistance > AttackRange)
	{
		return false;
	}

	FVector DesiredForward = Target->GetActorLocation()
		- OwnerCharacter->GetActorLocation();
	DesiredForward.Z = 0.0f;
	if (DesiredForward.Normalize())
	{
		OwnerCharacter->SetActorRotation(DesiredForward.Rotation());
		AttackForward = DesiredForward;
	}
	else
	{
		AttackForward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	ResetTraceState();
	ActiveAttackTraceSettings = MeleeTraceSettings;
	ActiveMeleeTraceSettings = ActiveAttackTraceSettings;
	ActiveAttackMontage = SelectedPattern
		? SelectedPattern->Montage
		: AttackMontage;
	ActivePatternId = SelectedPattern
		? SelectedPattern->PatternId
		: NAME_None;
	ActivePlayRate = SelectedPattern
		? FMath::FRandRange(
			SelectedPattern->MinPlayRate,
			SelectedPattern->MaxPlayRate)
		: 1.0f;
	ActivePatternDamageScale = SelectedPattern
		? SelectedPattern->DamageScale
		: 1.0f;
	ActivePatternStaggerScale = SelectedPattern
		? SelectedPattern->StaggerScale
		: 1.0f;
	ActiveCooldownScale = SelectedPattern
		? SelectedPattern->CooldownScale
		: 1.0f;
	const float MaximumForwardMoveDistance = SelectedPattern
		? SelectedPattern->ForwardMoveDistance
		: 0.0f;
	const float ForwardMoveStopDistance = SelectedPattern
		? SelectedPattern->ForwardMoveStopDistance
		: 0.0f;
	ActiveForwardMoveDistance = MaximumForwardMoveDistance;
	if (ForwardMoveStopDistance > 0.0f)
	{
		ActiveForwardMoveDistance = FMath::Min(
			MaximumForwardMoveDistance,
			FMath::Max(0.0f, TargetDistance - ForwardMoveStopDistance));
	}
	if (SelectedPattern)
	{
		RecentPatternIds.Add(SelectedPattern->PatternId);
		if (RecentPatternIds.Num() > 2)
		{
			RecentPatternIds.RemoveAt(0, RecentPatternIds.Num() - 2);
		}
		if (SelectedPattern->ReuseCooldown > 0.0f)
		{
			PatternNextAllowedTimes.Add(
				SelectedPattern->PatternId,
				World->GetTimeSeconds() + SelectedPattern->ReuseCooldown);
		}
	}
	bAttackActive = true;
	bOpportunityOpen =
		TargetingMode == EAIRECombatTargetingMode::SingleTarget;
	bHitCommitted = false;
	bDamageApplied = false;
	bDamageCancelled = false;
	bFinishing = false;
	ActiveExecutionId = FGuid::NewGuid();
	AttackTarget = Target;
	Target->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIREEnemyAttackComponent::HandleTargetDestroyed);

	float PresentationDuration = 0.0f;
	if (IsValid(ActiveAttackMontage))
	{
		PresentationDuration = OwnerCharacter->PlayAnimMontage(
			ActiveAttackMontage,
			ActivePlayRate);
		bMontagePlayed = PresentationDuration > 0.0f;
		if (bMontagePlayed)
		{
			if (UAnimInstance* AnimInstance =
				OwnerCharacter->GetMesh()->GetAnimInstance())
			{
				BoundAnimInstance = AnimInstance;
				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(
					this,
					&UAIREEnemyAttackComponent::HandleMontageEnded);
				AnimInstance->Montage_SetEndDelegate(
					EndDelegate,
					ActiveAttackMontage);
			}
		}
	}

	if (!bMontagePlayed)
	{
		World->GetTimerManager().SetTimer(
			HitTimerHandle,
			this,
			&UAIREEnemyAttackComponent::HandleFallbackHit,
			FallbackHitDelay,
			false);
	}
	const float RecoveryDuration = bMontagePlayed
		? FMath::Max(
			FallbackRecoveryDuration,
			PresentationDuration + ActiveTempoRecoveryExtension)
		: FMath::Max(FallbackRecoveryDuration, FallbackHitDelay);
	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UAIREEnemyAttackComponent::HandleRecoveryExpired,
		RecoveryDuration,
		false);
	const bool bStartedGapCloser =
		ActiveForwardMoveDistance > UE_SMALL_NUMBER;
	bRequiresNonGapCloserFollowUp = bStartedGapCloser;
#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogAIREEnemyAttack,
		Log,
		TEXT("Attack started Enemy=%s Target=%s Pattern=%s Distance=%.1f PlayRate=%.2f Cooldown=%.2f ForwardMove=%.1f Montage=%s"),
		*GetNameSafe(OwnerCharacter.Get()),
		*GetNameSafe(Target),
		*ActivePatternId.ToString(),
		TargetDistance,
		ActivePlayRate,
		CooldownDuration * ActiveCooldownScale,
		ActiveForwardMoveDistance,
		*GetNameSafe(ActiveAttackMontage));
#endif
	OnAttackStarted.Broadcast(Target, ActiveExecutionId);
	return true;
}

bool UAIREEnemyAttackComponent::CommitActiveMeleeHit()
{
	PrepareFallbackStrike();
	if (!CanResolveActiveHit())
	{
		if (bAttackActive)
		{
			CloseTraceWindow();
			CloseOpportunity();
		}
		return false;
	}

	FHitResult TargetHit;
	const EAIRECombatMeleeTraceResult Result =
		PerformFallbackTraceSample(TargetHit);
	ResolveTraceSample(Result, TargetHit);
	if (Result == EAIRECombatMeleeTraceResult::NoHit
		|| Result == EAIRECombatMeleeTraceResult::Invalid)
	{
		CloseOpportunity();
	}
	return bDamageApplied;
}

void UAIREEnemyAttackComponent::BeginMeleeTraceWindow(
	const FGuid& ExecutionId,
	const int32 StrikeIndex,
	const float DamageScale,
	const float StaggerScale,
	const FName TraceStartSocket,
	const FName TraceEndSocket,
	const float TraceWindowDuration)
{
	if (!bAttackActive
		|| bDamageCancelled
		|| !ExecutionId.IsValid()
		|| ExecutionId != ActiveExecutionId
		|| StrikeIndex < 0
		|| CommittedStrikeIndices.Contains(StrikeIndex)
		|| !FMath::IsFinite(DamageScale)
		|| DamageScale < 0.0f
		|| !FMath::IsFinite(StaggerScale)
		|| StaggerScale < 0.0f
		|| (DamageScale <= 0.0f && StaggerScale <= 0.0f)
		|| !OwnerCharacter.IsValid())
	{
		return;
	}

	CloseTraceWindow();
	bTraceWindowOpen = true;
	bTraceWindowEverOpened = true;
	TraceWindowExecutionId = ExecutionId;
	ActiveStrikeIndex = StrikeIndex;
	ActiveStrikeDamageScale = DamageScale;
	ActiveStrikeStaggerScale = StaggerScale;
	ActiveMeleeTraceSettings = ActiveAttackTraceSettings;
	if (!TraceStartSocket.IsNone() && !TraceEndSocket.IsNone())
	{
		ActiveMeleeTraceSettings.TraceStartSocket = TraceStartSocket;
		ActiveMeleeTraceSettings.TraceEndSocket = TraceEndSocket;
	}
	ActiveTraceMesh = OwnerCharacter->GetMesh();
	USkeletalMeshComponent* MeshComponent = ActiveTraceMesh.Get();
	const bool bHasConfiguredSockets =
		!ActiveMeleeTraceSettings.TraceStartSocket.IsNone()
		&& !ActiveMeleeTraceSettings.TraceEndSocket.IsNone();
	bUseSocketTrace = IsValid(MeshComponent)
		&& bHasConfiguredSockets
		&& MeshComponent->DoesSocketExist(
			ActiveMeleeTraceSettings.TraceStartSocket)
		&& MeshComponent->DoesSocketExist(
			ActiveMeleeTraceSettings.TraceEndSocket);
	if (bUseSocketTrace)
	{
		PreviousTraceStart = MeshComponent->GetSocketLocation(
			ActiveMeleeTraceSettings.TraceStartSocket);
		PreviousTraceEnd = MeshComponent->GetSocketLocation(
			ActiveMeleeTraceSettings.TraceEndSocket);
	}
#if !UE_BUILD_SHIPPING
	else if (bHasConfiguredSockets)
	{
		UE_LOG(
			LogAIREEnemyAttack,
			Warning,
			TEXT("Enemy %s cannot resolve trace bones/sockets %s -> %s; using the forward fallback shape for execution %s."),
			*GetNameSafe(GetOwner()),
			*ActiveMeleeTraceSettings.TraceStartSocket.ToString(),
			*ActiveMeleeTraceSettings.TraceEndSocket.ToString(),
			*ExecutionId.ToString());
	}
#endif
	if (!bAttackMovementWindowEverOpened)
	{
		StartAttackMovement(TraceWindowDuration);
	}
}

void UAIREEnemyAttackComponent::UpdateMeleeTraceWindow(
	const FGuid& ExecutionId,
	const int32 StrikeIndex)
{
	if (!IsTraceCallbackCurrent(ExecutionId, StrikeIndex))
	{
		return;
	}
	if (!CanResolveActiveHit())
	{
		CloseTraceWindow();
		return;
	}

	FHitResult TargetHit;
	const EAIRECombatMeleeTraceResult Result = bUseSocketTrace
		? PerformSocketTraceSample(ActiveTraceMesh.Get(), TargetHit)
		: PerformFallbackTraceSample(TargetHit);
	ResolveTraceSample(Result, TargetHit);
}

void UAIREEnemyAttackComponent::EndMeleeTraceWindow(
	const FGuid& ExecutionId,
	const int32 StrikeIndex)
{
	if (!IsTraceCallbackCurrent(ExecutionId, StrikeIndex))
	{
		return;
	}
	CloseTraceWindow();
}

void UAIREEnemyAttackComponent::BeginAttackMovementWindow(
	const FGuid& ExecutionId,
	const float MovementWindowDuration)
{
	if (!bAttackActive
		|| !ExecutionId.IsValid()
		|| ExecutionId != ActiveExecutionId
		|| !FMath::IsFinite(MovementWindowDuration)
		|| MovementWindowDuration <= 0.0f)
	{
		return;
	}

	bAttackMovementWindowEverOpened = true;
	StartAttackMovement(MovementWindowDuration);
}

void UAIREEnemyAttackComponent::EndAttackMovementWindow(
	const FGuid& ExecutionId)
{
	if (!bAttackActive
		|| !ExecutionId.IsValid()
		|| ExecutionId != ActiveExecutionId)
	{
		return;
	}
	StopAttackMovement();
}

bool UAIREEnemyAttackComponent::BeginAttackTempoWindow(
	const FGuid& ExecutionId,
	UAnimMontage* Montage,
	const int32 StrikeIndex,
	const float StrikeStartTime,
	const float WindowEndTime,
	const float AnticipationPlayRateMultiplier,
	const float StrikePlayRateMultiplier)
{
	if (!bAttackActive
		|| ExecutionId != ActiveExecutionId
		|| bAttackTempoWindowOpen
		|| Montage != ActiveAttackMontage
		|| StrikeIndex < 0
		|| !BoundAnimInstance.IsValid()
		|| !IsValid(ActiveAttackMontage)
		|| !BoundAnimInstance->Montage_IsActive(ActiveAttackMontage)
		|| !FMath::IsFinite(StrikeStartTime)
		|| !FMath::IsFinite(WindowEndTime)
		|| !FMath::IsFinite(AnticipationPlayRateMultiplier)
		|| AnticipationPlayRateMultiplier < 0.01f
		|| !FMath::IsFinite(StrikePlayRateMultiplier)
		|| StrikePlayRateMultiplier < 0.01f)
	{
		return false;
	}

	const float CurrentPosition = BoundAnimInstance->Montage_GetPosition(
		ActiveAttackMontage);
	if (WindowEndTime <= CurrentPosition || WindowEndTime <= StrikeStartTime)
	{
		return false;
	}

	TempoWindowExecutionId = ExecutionId;
	ActiveTempoStrikeIndex = StrikeIndex;
	ActiveTempoStrikeStartTime = StrikeStartTime;
	ActiveTempoStrikePlayRateMultiplier = StrikePlayRateMultiplier;
	bAttackTempoWindowOpen = true;
	bAttackTempoStrikeRateApplied = CurrentPosition >= StrikeStartTime;
	SetAttackTempoPlayRate(
		bAttackTempoStrikeRateApplied
			? StrikePlayRateMultiplier
			: AnticipationPlayRateMultiplier);

	float AdditionalDuration = 0.0f;
	if (bAttackTempoStrikeRateApplied)
	{
		const float BaseWindowDuration =
			(WindowEndTime - CurrentPosition) / ActivePlayRate;
		const float TempoWindowDuration =
			(WindowEndTime - CurrentPosition)
				/ (ActivePlayRate * StrikePlayRateMultiplier);
		AdditionalDuration = FMath::Max(
			0.0f,
			TempoWindowDuration - BaseWindowDuration);
	}
	else
	{
		const float BaseWindowDuration =
			(WindowEndTime - CurrentPosition) / ActivePlayRate;
		const float TempoWindowDuration =
			(StrikeStartTime - CurrentPosition)
				/ (ActivePlayRate * AnticipationPlayRateMultiplier)
			+ (WindowEndTime - StrikeStartTime)
				/ (ActivePlayRate * StrikePlayRateMultiplier);
		AdditionalDuration = FMath::Max(
			0.0f,
			TempoWindowDuration - BaseWindowDuration);
	}
	ActiveTempoRecoveryExtension += AdditionalDuration;
	if (UWorld* World = GetWorld())
	{
		const float RemainingRecovery = World->GetTimerManager().GetTimerRemaining(
			RecoveryTimerHandle);
		if (RemainingRecovery > 0.0f && AdditionalDuration > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				RecoveryTimerHandle,
				this,
				&UAIREEnemyAttackComponent::HandleRecoveryExpired,
				RemainingRecovery + AdditionalDuration,
				false);
		}
	}
	return true;
}

void UAIREEnemyAttackComponent::UpdateAttackTempoWindow(
	const FGuid& ExecutionId,
	const int32 StrikeIndex)
{
	if (!bAttackTempoWindowOpen
		|| ExecutionId != TempoWindowExecutionId
		|| StrikeIndex != ActiveTempoStrikeIndex
		|| !BoundAnimInstance.IsValid()
		|| !IsValid(ActiveAttackMontage))
	{
		return;
	}

	if (!bAttackTempoStrikeRateApplied
		&& BoundAnimInstance->Montage_GetPosition(ActiveAttackMontage)
			>= ActiveTempoStrikeStartTime)
	{
		SetAttackTempoPlayRate(ActiveTempoStrikePlayRateMultiplier);
		bAttackTempoStrikeRateApplied = true;
	}
}

void UAIREEnemyAttackComponent::EndAttackTempoWindow(
	const FGuid& ExecutionId,
	const int32 StrikeIndex)
{
	if (!bAttackTempoWindowOpen
		|| ExecutionId != TempoWindowExecutionId
		|| StrikeIndex != ActiveTempoStrikeIndex)
	{
		return;
	}
	ResetAttackTempo(true);
}

bool UAIREEnemyAttackComponent::TryCancelDamageForAggroSwap(
	const FGuid& ExecutionId)
{
	if (!bAttackActive
		|| !bOpportunityOpen
		|| bHitCommitted
		|| bDamageCancelled
		|| !ExecutionId.IsValid()
		|| ExecutionId != ActiveExecutionId)
	{
		return false;
	}

	bDamageCancelled = true;
	CloseTraceWindow();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
	}
	CloseOpportunity();
	return true;
}

void UAIREEnemyAttackComponent::CancelCurrentAttack()
{
	if (!bAttackActive || bFinishing)
	{
		return;
	}
	bDamageCancelled = true;
	CloseTraceWindow();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
	}
	if (OwnerCharacter.IsValid() && IsValid(ActiveAttackMontage))
	{
		ResetAttackTempo(true);
		OwnerCharacter->StopAnimMontage(ActiveAttackMontage);
	}
	if (bAttackActive)
	{
		FinishAttack();
	}
}

FAIREEnemyAttackSnapshot
UAIREEnemyAttackComponent::GetAttackSnapshot() const
{
	FAIREEnemyAttackSnapshot Snapshot;
	Snapshot.bActive = bAttackActive;
	Snapshot.bOpportunityOpen = bOpportunityOpen;
	Snapshot.bHitCommitted = bHitCommitted;
	Snapshot.bDamageCancelled = bDamageCancelled;
	Snapshot.TargetingMode = TargetingMode;
	Snapshot.Target = AttackTarget.Get();
	Snapshot.ExecutionId = ActiveExecutionId;
	Snapshot.PatternId = ActivePatternId;
	Snapshot.PlayRate = ActivePlayRate;
	Snapshot.bGapCloser = ActiveForwardMoveDistance > UE_SMALL_NUMBER;
	Snapshot.CommittedStrikeCount = CommittedStrikeIndices.Num();
	return Snapshot;
}

float UAIREEnemyAttackComponent::GetAttackRange() const
{
	float MaximumRange = AttackRange;
	const float HealthRatio = GetOwnerHealthRatio();
	for (const FAIREEnemyAttackPattern& Pattern : AttackPatterns)
	{
		if (IsValid(Pattern.Montage)
			&& HealthRatio >= Pattern.MinHealthRatio
			&& HealthRatio <= Pattern.MaxHealthRatio)
		{
			MaximumRange = FMath::Max(MaximumRange, Pattern.MaxRange);
		}
	}
	return MaximumRange;
}

bool UAIREEnemyAttackComponent::IsTargetWithinAttackRange(
	const AActor* Target) const
{
	if (!OwnerCharacter.IsValid()
		|| !IsValid(Target)
		|| Target->IsActorBeingDestroyed())
	{
		return false;
	}

	return GetSurfaceDistanceToTarget(Target) <= GetAttackRange();
}

void UAIREEnemyAttackComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownAttack();
	Super::EndPlay(EndPlayReason);
}

void UAIREEnemyAttackComponent::HandleTargetDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == AttackTarget.Get())
	{
		CancelCurrentAttack();
	}
}

void UAIREEnemyAttackComponent::HandleMontageEnded(
	UAnimMontage* Montage,
	const bool bInterrupted)
{
	if (Montage != ActiveAttackMontage || !bAttackActive)
	{
		return;
	}
	BoundAnimInstance.Reset();
	if (bInterrupted)
	{
		bDamageCancelled = true;
	}
#if !UE_BUILD_SHIPPING
	else if (!bTraceWindowEverOpened)
	{
		UE_LOG(
			LogAIREEnemyAttack,
			Warning,
			TEXT("Enemy %s completed attack montage %s without opening a melee trace window. Execution %s is a miss."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(ActiveAttackMontage),
			*ActiveExecutionId.ToString());
	}
#endif
	CloseTraceWindow();
	CloseOpportunity();
	FinishAttack();
}

void UAIREEnemyAttackComponent::HandleFallbackHit()
{
	PrepareFallbackStrike();
	if (!CanResolveActiveHit())
	{
		CloseOpportunity();
		return;
	}
	FHitResult TargetHit;
	const EAIRECombatMeleeTraceResult Result =
		PerformFallbackTraceSample(TargetHit);
	ResolveTraceSample(Result, TargetHit);
	if (Result == EAIRECombatMeleeTraceResult::NoHit
		|| Result == EAIRECombatMeleeTraceResult::Invalid)
	{
		CloseOpportunity();
	}
}

void UAIREEnemyAttackComponent::HandleRecoveryExpired()
{
#if !UE_BUILD_SHIPPING
	if (bAttackActive && bMontagePlayed && !bTraceWindowEverOpened)
	{
		UE_LOG(
			LogAIREEnemyAttack,
			Warning,
			TEXT("Enemy %s attack recovery expired without a melee trace window. Execution %s is a miss."),
			*GetNameSafe(GetOwner()),
			*ActiveExecutionId.ToString());
	}
#endif
	CloseTraceWindow();
	CloseOpportunity();
	FinishAttack();
}

bool UAIREEnemyAttackComponent::IsTraceCallbackCurrent(
	const FGuid& ExecutionId,
	const int32 StrikeIndex) const
{
	return bTraceWindowOpen
		&& ExecutionId.IsValid()
		&& ExecutionId == TraceWindowExecutionId
		&& ExecutionId == ActiveExecutionId
		&& StrikeIndex == ActiveStrikeIndex;
}

float UAIREEnemyAttackComponent::GetPreferredAttackRange() const
{
	return AttackRange;
}

float UAIREEnemyAttackComponent::GetRemainingAttackCooldown() const
{
	const UWorld* World = GetWorld();
	return IsValid(World)
		? static_cast<float>(FMath::Max(
			0.0,
			NextAllowedAttackTime - World->GetTimeSeconds()))
		: 0.0f;
}

float UAIREEnemyAttackComponent::GetTargetSurfaceDistance(
	const AActor* Target) const
{
	return GetSurfaceDistanceToTarget(Target);
}

bool UAIREEnemyAttackComponent::CanResolveActiveHit() const
{
	return bAttackActive
		&& !bDamageCancelled
		&& ActiveStrikeIndex >= 0
		&& !CommittedStrikeIndices.Contains(ActiveStrikeIndex)
		&& TargetingMode == EAIRECombatTargetingMode::SingleTarget
		&& OwnerCharacter.IsValid()
		&& AttackTarget.IsValid()
		&& AIRECombatDamageTarget::IsAlive(AttackTarget.Get());
}

bool UAIREEnemyAttackComponent::CommitResolvedHit(
	const FHitResult& HitResult)
{
	if (!CanResolveActiveHit()
		|| HitResult.GetActor() != AttackTarget.Get())
	{
		return false;
	}

	const int32 CommittedStrikeIndex = ActiveStrikeIndex;
	const float StrikeDamageScale = ActiveStrikeDamageScale;
	const float StrikeStaggerScale = ActiveStrikeStaggerScale;
	FGuid& StrikeExecutionId = StrikeExecutionIds.FindOrAdd(
		CommittedStrikeIndex);
	if (!StrikeExecutionId.IsValid())
	{
		StrikeExecutionId = FGuid::NewGuid();
	}
	CommittedStrikeIndices.Add(CommittedStrikeIndex);
	bHitCommitted = true;
	CloseTraceWindow();
	CloseOpportunity();
	UAIRECombatDamageSubsystem* DamageSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UAIRECombatDamageSubsystem>()
		: nullptr;
	if (!IsValid(DamageSubsystem))
	{
		return false;
	}

	FAIRECombatDamageRequest Request;
	Request.Source = OwnerCharacter.Get();
	Request.Target = AttackTarget.Get();
	Request.Damage = Damage
		* ActivePatternDamageScale
		* StrikeDamageScale;
	Request.StaggerValue = StaggerValue
		* ActivePatternStaggerScale
		* StrikeStaggerScale;
	Request.ExecutionId = StrikeExecutionId;
	Request.bHasHitResult = true;
	Request.HitResult = HitResult;
	const EAIRECombatDamageResult DamageResult =
		DamageSubsystem->ApplyDamageRequest(Request);
#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogAIREEnemyAttack,
		Log,
		TEXT("[ENEMY ATTACK] Phase=DamageCommit Source=%s Target=%s StrikeIndex=%d ExecutionId=%s Damage=%.2f Stagger=%.2f Result=%s"),
		*GetNameSafe(Request.Source.Get()),
		*GetNameSafe(Request.Target.Get()),
		CommittedStrikeIndex,
		*Request.ExecutionId.ToString(),
		Request.Damage,
		Request.StaggerValue,
		*StaticEnum<EAIRECombatDamageResult>()->GetNameStringByValue(
			static_cast<int64>(DamageResult)));
#endif
	const bool bStrikeDamageApplied =
		DamageResult == EAIRECombatDamageResult::Applied;
	bDamageApplied = bDamageApplied || bStrikeDamageApplied;
	return bStrikeDamageApplied;
}

EAIRECombatMeleeTraceResult
UAIREEnemyAttackComponent::PerformSocketTraceSample(
	USkeletalMeshComponent* MeshComponent,
	FHitResult& OutTargetHit)
{
	if (!IsValid(MeshComponent)
		|| !MeshComponent->DoesSocketExist(
			ActiveMeleeTraceSettings.TraceStartSocket)
		|| !MeshComponent->DoesSocketExist(
			ActiveMeleeTraceSettings.TraceEndSocket))
	{
		bUseSocketTrace = false;
		return PerformFallbackTraceSample(OutTargetHit);
	}

	const FVector CurrentTraceStart = MeshComponent->GetSocketLocation(
		ActiveMeleeTraceSettings.TraceStartSocket);
	const FVector CurrentTraceEnd = MeshComponent->GetSocketLocation(
		ActiveMeleeTraceSettings.TraceEndSocket);
	FAIRECombatMeleeTraceRequest Request;
	Request.World = GetWorld();
	Request.Source = OwnerCharacter.Get();
	Request.Target = AttackTarget.Get();
	Request.Shape = EAIRECombatMeleeTraceShape::Sphere;
	Request.Radius = ActiveMeleeTraceSettings.TraceRadius;
	Request.TraceChannel =
		ActiveMeleeTraceSettings.TraceChannel.GetValue();
	Request.Segments.Reserve(4);
	Request.Segments.Emplace(PreviousTraceStart, CurrentTraceStart);
	Request.Segments.Emplace(PreviousTraceEnd, CurrentTraceEnd);
	Request.Segments.Emplace(PreviousTraceStart, PreviousTraceEnd);
	Request.Segments.Emplace(CurrentTraceStart, CurrentTraceEnd);
	const FAIRECombatMeleeTraceResolution Resolution =
		FAIRECombatMeleeTraceResolver::Resolve(Request);
	PreviousTraceStart = CurrentTraceStart;
	PreviousTraceEnd = CurrentTraceEnd;
	if (Resolution.Result == EAIRECombatMeleeTraceResult::TargetHit)
	{
		OutTargetHit = Resolution.HitResult;
	}
	return Resolution.Result;
}

EAIRECombatMeleeTraceResult
UAIREEnemyAttackComponent::PerformFallbackTraceSample(
	FHitResult& OutTargetHit) const
{
	if (!OwnerCharacter.IsValid())
	{
		return EAIRECombatMeleeTraceResult::NoHit;
	}

	FVector Forward = AttackForward.GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float OwnerRadius = IsValid(Capsule)
		? Capsule->GetScaledCapsuleRadius()
		: 0.0f;
	const FVector Start = OwnerCharacter->GetActorLocation()
		+ Forward * OwnerRadius;
	const float CenterTravelDistance = FMath::Max(
		0.0f,
		ActiveMeleeTraceSettings.FallbackTraceDistance
			- ActiveMeleeTraceSettings.TraceRadius);
	const FVector End = Start + Forward * CenterTravelDistance;
	FAIRECombatMeleeTraceRequest Request;
	Request.World = GetWorld();
	Request.Source = OwnerCharacter.Get();
	Request.Target = AttackTarget.Get();
	Request.Shape = EAIRECombatMeleeTraceShape::Sphere;
	Request.Radius = ActiveMeleeTraceSettings.TraceRadius;
	Request.TraceChannel =
		ActiveMeleeTraceSettings.TraceChannel.GetValue();
	Request.Segments.Emplace(Start, End);
	const FAIRECombatMeleeTraceResolution Resolution =
		FAIRECombatMeleeTraceResolver::Resolve(Request);
	if (Resolution.Result == EAIRECombatMeleeTraceResult::TargetHit)
	{
		OutTargetHit = Resolution.HitResult;
	}
	return Resolution.Result;
}

void UAIREEnemyAttackComponent::ResolveTraceSample(
	const EAIRECombatMeleeTraceResult Result,
	const FHitResult& TargetHit)
{
	if (Result == EAIRECombatMeleeTraceResult::TargetHit)
	{
		CommitResolvedHit(TargetHit);
	}
	else if (Result == EAIRECombatMeleeTraceResult::Blocked)
	{
		CloseTraceWindow();
		CloseOpportunity();
	}
}

void UAIREEnemyAttackComponent::PrepareFallbackStrike()
{
	if (!bAttackActive || ActiveStrikeIndex >= 0)
	{
		return;
	}
	ActiveStrikeIndex = 0;
	ActiveStrikeDamageScale = 1.0f;
	ActiveStrikeStaggerScale = 1.0f;
	ActiveMeleeTraceSettings = ActiveAttackTraceSettings;
}

const FAIREEnemyAttackPattern*
UAIREEnemyAttackComponent::SelectAttackPattern(const AActor* Target) const
{
	const float TargetDistance = GetSurfaceDistanceToTarget(Target);
	const float HealthRatio = GetOwnerHealthRatio();
	const UWorld* World = GetWorld();
	const double CurrentTime = IsValid(World)
		? World->GetTimeSeconds()
		: 0.0;
	TArray<const FAIREEnemyAttackPattern*> EligiblePatterns;
	TArray<const FAIREEnemyAttackPattern*> NonRecentPatterns;
	TArray<const FAIREEnemyAttackPattern*> NonImmediateRepeatPatterns;
	for (const FAIREEnemyAttackPattern& Pattern : AttackPatterns)
	{
		const double* PatternNextAllowedTime =
			PatternNextAllowedTimes.Find(Pattern.PatternId);
		if (!IsValid(Pattern.Montage)
			|| (bRequiresNonGapCloserFollowUp
				&& Pattern.ForwardMoveDistance > UE_SMALL_NUMBER)
			|| TargetDistance < Pattern.MinRange
			|| TargetDistance > Pattern.MaxRange
			|| HealthRatio < Pattern.MinHealthRatio
			|| HealthRatio > Pattern.MaxHealthRatio
			|| !FMath::IsFinite(Pattern.Weight)
			|| Pattern.Weight <= 0.0f
			|| (PatternNextAllowedTime
				&& CurrentTime < *PatternNextAllowedTime))
		{
			continue;
		}
		EligiblePatterns.Add(&Pattern);
		if (!RecentPatternIds.Contains(Pattern.PatternId))
		{
			NonRecentPatterns.Add(&Pattern);
		}
		if (RecentPatternIds.IsEmpty()
			|| Pattern.PatternId != RecentPatternIds.Last())
		{
			NonImmediateRepeatPatterns.Add(&Pattern);
		}
	}

	const TArray<const FAIREEnemyAttackPattern*>* SelectionPool =
		&EligiblePatterns;
	if (!NonRecentPatterns.IsEmpty())
	{
		SelectionPool = &NonRecentPatterns;
	}
	else if (!NonImmediateRepeatPatterns.IsEmpty())
	{
		SelectionPool = &NonImmediateRepeatPatterns;
	}
	float TotalWeight = 0.0f;
	for (const FAIREEnemyAttackPattern* Pattern : *SelectionPool)
	{
		TotalWeight += Pattern->Weight;
	}
	if (SelectionPool->IsEmpty() || TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float RemainingWeight = FMath::FRandRange(0.0f, TotalWeight);
	for (const FAIREEnemyAttackPattern* Pattern : *SelectionPool)
	{
		RemainingWeight -= Pattern->Weight;
		if (RemainingWeight <= 0.0f)
		{
			return Pattern;
		}
	}
	return SelectionPool->Last();
}

float UAIREEnemyAttackComponent::GetSurfaceDistanceToTarget(
	const AActor* Target) const
{
	if (!OwnerCharacter.IsValid() || !IsValid(Target))
	{
		return TNumericLimits<float>::Max();
	}
	const UCapsuleComponent* OwnerCapsule =
		OwnerCharacter->GetCapsuleComponent();
	const float OwnerRadius = IsValid(OwnerCapsule)
		? OwnerCapsule->GetScaledCapsuleRadius()
		: 0.0f;
	const float TargetRadius = Target->GetSimpleCollisionRadius();
	const float CenterDistance = FVector::Dist2D(
		OwnerCharacter->GetActorLocation(),
		Target->GetActorLocation());
	return FMath::Max(
		0.0f,
		CenterDistance - OwnerRadius - TargetRadius);
}

float UAIREEnemyAttackComponent::GetOwnerHealthRatio() const
{
	const UAIREEnemyVitalityComponent* Vitality = OwnerCharacter.IsValid()
		? OwnerCharacter->FindComponentByClass<UAIREEnemyVitalityComponent>()
		: nullptr;
	if (!IsValid(Vitality))
	{
		return 1.0f;
	}
	const FAIREEnemyVitalitySnapshot Snapshot =
		Vitality->GetVitalitySnapshot();
	return Snapshot.MaxHealth > 0.0f
		? FMath::Clamp(Snapshot.Health / Snapshot.MaxHealth, 0.0f, 1.0f)
		: 1.0f;
}

void UAIREEnemyAttackComponent::StartAttackMovement(
	const float TraceWindowDuration)
{
	if (bAttackMovementStarted
		|| !OwnerCharacter.IsValid()
		|| !FMath::IsFinite(ActiveForwardMoveDistance)
		|| ActiveForwardMoveDistance <= 0.0f
		|| !FMath::IsFinite(TraceWindowDuration)
		|| TraceWindowDuration <= 0.0f)
	{
		return;
	}
	UCharacterMovementComponent* Movement =
		OwnerCharacter->GetCharacterMovement();
	if (!IsValid(Movement)
		|| (IsValid(ActiveAttackMontage)
			&& ActiveAttackMontage->HasRootMotion()))
	{
		return;
	}

	float MontagePlayRate = ActivePlayRate;
	if (BoundAnimInstance.IsValid() && IsValid(ActiveAttackMontage))
	{
		MontagePlayRate = BoundAnimInstance->Montage_GetPlayRate(
			ActiveAttackMontage);
	}
	MontagePlayRate = FMath::Abs(MontagePlayRate);
	if (!FMath::IsFinite(MontagePlayRate)
		|| MontagePlayRate <= UE_SMALL_NUMBER)
	{
		return;
	}

	FVector Forward = AttackForward.GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}
	if (Forward.IsNearlyZero())
	{
		return;
	}

	const FVector StartLocation = OwnerCharacter->GetActorLocation();
	TSharedPtr<FRootMotionSource_MoveToForce> MoveToForce =
		MakeShared<FRootMotionSource_MoveToForce>();
	MoveToForce->InstanceName = FName(TEXT("AIREEnemyAttackForwardMove"));
	MoveToForce->AccumulateMode = ERootMotionAccumulateMode::Override;
	MoveToForce->Settings.SetFlag(
		ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
	MoveToForce->Priority = 1000;
	MoveToForce->StartLocation = StartLocation;
	MoveToForce->TargetLocation = StartLocation
		+ Forward * ActiveForwardMoveDistance;
	MoveToForce->Duration = TraceWindowDuration / MontagePlayRate;
	MoveToForce->bRestrictSpeedToExpected = true;
	MoveToForce->FinishVelocityParams.Mode =
		ERootMotionFinishVelocityMode::SetVelocity;
	MoveToForce->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	ActiveMovementRootMotionSourceId =
		Movement->ApplyRootMotionSource(MoveToForce);
	bAttackMovementStarted = ActiveMovementRootMotionSourceId != 0;
}

void UAIREEnemyAttackComponent::StopAttackMovement()
{
	if (ActiveMovementRootMotionSourceId == 0)
	{
		return;
	}
	if (OwnerCharacter.IsValid())
	{
		if (UCharacterMovementComponent* Movement =
			OwnerCharacter->GetCharacterMovement())
		{
			Movement->RemoveRootMotionSourceByID(
				ActiveMovementRootMotionSourceId);
		}
	}
	ActiveMovementRootMotionSourceId = 0;
}

void UAIREEnemyAttackComponent::ResetAttackTempo(
	const bool bRestoreBasePlayRate)
{
	if (bRestoreBasePlayRate)
	{
		SetAttackTempoPlayRate(1.0f);
	}
	TempoWindowExecutionId.Invalidate();
	ActiveTempoStrikeIndex = INDEX_NONE;
	ActiveTempoStrikeStartTime = 0.0f;
	ActiveTempoStrikePlayRateMultiplier = 1.0f;
	bAttackTempoWindowOpen = false;
	bAttackTempoStrikeRateApplied = false;
}

void UAIREEnemyAttackComponent::SetAttackTempoPlayRate(
	const float PlayRateMultiplier)
{
	if (!BoundAnimInstance.IsValid()
		|| !IsValid(ActiveAttackMontage)
		|| !FMath::IsFinite(ActivePlayRate)
		|| ActivePlayRate <= 0.0f
		|| !FMath::IsFinite(PlayRateMultiplier)
		|| PlayRateMultiplier <= 0.0f)
	{
		return;
	}
	BoundAnimInstance->Montage_SetPlayRate(
		ActiveAttackMontage,
		ActivePlayRate * PlayRateMultiplier);
}

void UAIREEnemyAttackComponent::CloseTraceWindow()
{
	bTraceWindowOpen = false;
	bUseSocketTrace = false;
	TraceWindowExecutionId.Invalidate();
	ActiveStrikeIndex = INDEX_NONE;
	ActiveStrikeDamageScale = 1.0f;
	ActiveStrikeStaggerScale = 1.0f;
	ActiveTraceMesh.Reset();
	PreviousTraceStart = FVector::ZeroVector;
	PreviousTraceEnd = FVector::ZeroVector;
}

void UAIREEnemyAttackComponent::ResetTraceState()
{
	StopAttackMovement();
	ResetAttackTempo(false);
	CloseTraceWindow();
	ActiveAttackTraceSettings = FAIREEnemyMeleeTraceSettings();
	ActiveMeleeTraceSettings = FAIREEnemyMeleeTraceSettings();
	ActiveAttackMontage = nullptr;
	ActivePatternId = NAME_None;
	ActivePlayRate = 1.0f;
	ActivePatternDamageScale = 1.0f;
	ActivePatternStaggerScale = 1.0f;
	ActiveCooldownScale = 1.0f;
	ActiveForwardMoveDistance = 0.0f;
	ActiveTempoRecoveryExtension = 0.0f;
	CommittedStrikeIndices.Reset();
	StrikeExecutionIds.Reset();
	bMontagePlayed = false;
	bTraceWindowEverOpened = false;
	bAttackMovementWindowEverOpened = false;
	bAttackMovementStarted = false;
}

void UAIREEnemyAttackComponent::ClearMontageEndDelegate()
{
	if (BoundAnimInstance.IsValid() && IsValid(ActiveAttackMontage))
	{
		FOnMontageEnded EmptyDelegate;
		BoundAnimInstance->Montage_SetEndDelegate(
			EmptyDelegate,
			ActiveAttackMontage);
	}
	BoundAnimInstance.Reset();
}

void UAIREEnemyAttackComponent::CloseOpportunity()
{
	if (!bOpportunityOpen)
	{
		return;
	}
	bOpportunityOpen = false;
	OnOpportunityClosed.Broadcast(ActiveExecutionId);
}

void UAIREEnemyAttackComponent::FinishAttack()
{
	if (!bAttackActive || bFinishing)
	{
		return;
	}
	bFinishing = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		NextAllowedAttackTime = World->GetTimeSeconds()
			+ CooldownDuration * ActiveCooldownScale;
	}
	ResetAttackTempo(true);
	ClearMontageEndDelegate();
	CloseTraceWindow();
	CloseOpportunity();
	if (AttackTarget.IsValid())
	{
		AttackTarget->OnDestroyed.RemoveDynamic(
			this,
			&UAIREEnemyAttackComponent::HandleTargetDestroyed);
	}
	const FGuid FinishedExecutionId = ActiveExecutionId;
	const bool bCommitted = bDamageApplied;
	bAttackActive = false;
	AttackTarget.Reset();
	ActiveExecutionId.Invalidate();
	bOpportunityOpen = false;
	bHitCommitted = false;
	bDamageApplied = false;
	bDamageCancelled = false;
	bFinishing = false;
	ResetTraceState();
	OnAttackFinished.Broadcast(FinishedExecutionId, bCommitted);
}

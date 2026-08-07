#include "AIREEnemyAttackComponent.h"

#include "AIRECombatDamageSubsystem.h"
#include "AIRECombatDamageTargetInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
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
	ActiveMeleeTraceSettings = MeleeTraceSettings;
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
	if (IsValid(AttackMontage))
	{
		PresentationDuration = OwnerCharacter->PlayAnimMontage(AttackMontage);
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
					AttackMontage);
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
		? FMath::Max(FallbackRecoveryDuration, PresentationDuration)
		: FMath::Max(FallbackRecoveryDuration, FallbackHitDelay);
	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UAIREEnemyAttackComponent::HandleRecoveryExpired,
		RecoveryDuration,
		false);
	OnAttackStarted.Broadcast(Target, ActiveExecutionId);
	return true;
}

bool UAIREEnemyAttackComponent::CommitActiveMeleeHit()
{
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
	const ETraceSampleResult Result = PerformFallbackTraceSample(TargetHit);
	ResolveTraceSample(Result, TargetHit);
	if (Result == ETraceSampleResult::NoHit)
	{
		CloseOpportunity();
	}
	return bDamageApplied;
}

void UAIREEnemyAttackComponent::BeginMeleeTraceWindow(
	const FGuid& ExecutionId)
{
	if (!bAttackActive
		|| bDamageCancelled
		|| bHitCommitted
		|| !ExecutionId.IsValid()
		|| ExecutionId != ActiveExecutionId
		|| !OwnerCharacter.IsValid())
	{
		return;
	}

	CloseTraceWindow();
	bTraceWindowOpen = true;
	bTraceWindowEverOpened = true;
	TraceWindowExecutionId = ExecutionId;
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
}

void UAIREEnemyAttackComponent::UpdateMeleeTraceWindow(
	const FGuid& ExecutionId)
{
	if (!IsTraceCallbackCurrent(ExecutionId))
	{
		return;
	}
	if (!CanResolveActiveHit())
	{
		CloseTraceWindow();
		CloseOpportunity();
		return;
	}

	FHitResult TargetHit;
	const ETraceSampleResult Result = bUseSocketTrace
		? PerformSocketTraceSample(ActiveTraceMesh.Get(), TargetHit)
		: PerformFallbackTraceSample(TargetHit);
	ResolveTraceSample(Result, TargetHit);
}

void UAIREEnemyAttackComponent::EndMeleeTraceWindow(
	const FGuid& ExecutionId)
{
	if (!IsTraceCallbackCurrent(ExecutionId))
	{
		return;
	}
	CloseTraceWindow();
	CloseOpportunity();
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
	if (OwnerCharacter.IsValid() && IsValid(AttackMontage))
	{
		OwnerCharacter->StopAnimMontage(AttackMontage);
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
	return Snapshot;
}

float UAIREEnemyAttackComponent::GetAttackRange() const
{
	return AttackRange;
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

	const float OwnerRadius = OwnerCharacter->GetCapsuleComponent()
		? OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius()
		: 0.0f;
	const float TargetRadius = Target->GetSimpleCollisionRadius();
	const float CenterDistance = FVector::Dist2D(
		OwnerCharacter->GetActorLocation(),
		Target->GetActorLocation());
	return FMath::Max(0.0f, CenterDistance - OwnerRadius - TargetRadius)
		<= AttackRange;
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
	if (Montage != AttackMontage || !bAttackActive)
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
			*GetNameSafe(AttackMontage),
			*ActiveExecutionId.ToString());
	}
#endif
	CloseTraceWindow();
	CloseOpportunity();
	FinishAttack();
}

void UAIREEnemyAttackComponent::HandleFallbackHit()
{
	if (!CanResolveActiveHit())
	{
		CloseOpportunity();
		return;
	}
	FHitResult TargetHit;
	const ETraceSampleResult Result = PerformFallbackTraceSample(TargetHit);
	ResolveTraceSample(Result, TargetHit);
	if (Result == ETraceSampleResult::NoHit)
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
	const FGuid& ExecutionId) const
{
	return bTraceWindowOpen
		&& ExecutionId.IsValid()
		&& ExecutionId == TraceWindowExecutionId
		&& ExecutionId == ActiveExecutionId;
}

bool UAIREEnemyAttackComponent::CanResolveActiveHit() const
{
	return bAttackActive
		&& !bHitCommitted
		&& !bDamageCancelled
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
	Request.Damage = Damage;
	Request.StaggerValue = StaggerValue;
	Request.ExecutionId = ActiveExecutionId;
	Request.bHasHitResult = true;
	Request.HitResult = HitResult;
	bDamageApplied = DamageSubsystem->ApplyDamageRequest(Request)
		== EAIRECombatDamageResult::Applied;
	return bDamageApplied;
}

UAIREEnemyAttackComponent::ETraceSampleResult
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
	TArray<TPair<FVector, FVector>> TraceSegments;
	TraceSegments.Reserve(4);
	TraceSegments.Emplace(PreviousTraceStart, CurrentTraceStart);
	TraceSegments.Emplace(PreviousTraceEnd, CurrentTraceEnd);
	TraceSegments.Emplace(PreviousTraceStart, PreviousTraceEnd);
	TraceSegments.Emplace(CurrentTraceStart, CurrentTraceEnd);
	const ETraceSampleResult Result = SweepTraceSegments(
		TraceSegments,
		OutTargetHit);
	PreviousTraceStart = CurrentTraceStart;
	PreviousTraceEnd = CurrentTraceEnd;
	return Result;
}

UAIREEnemyAttackComponent::ETraceSampleResult
UAIREEnemyAttackComponent::PerformFallbackTraceSample(
	FHitResult& OutTargetHit) const
{
	if (!OwnerCharacter.IsValid())
	{
		return ETraceSampleResult::NoHit;
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
	FHitResult HitResult;
	if (!SweepTraceSegment(Start, End, HitResult))
	{
		return ETraceSampleResult::NoHit;
	}
	if (HitResult.GetActor() == AttackTarget.Get())
	{
		OutTargetHit = HitResult;
		return ETraceSampleResult::TargetHit;
	}
	return ETraceSampleResult::Blocked;
}

UAIREEnemyAttackComponent::ETraceSampleResult
UAIREEnemyAttackComponent::SweepTraceSegments(
	const TArray<TPair<FVector, FVector>>& Segments,
	FHitResult& OutTargetHit) const
{
	bool bTargetHit = false;
	bool bBlocked = false;
	for (const TPair<FVector, FVector>& Segment : Segments)
	{
		FHitResult HitResult;
		if (!SweepTraceSegment(Segment.Key, Segment.Value, HitResult))
		{
			continue;
		}
		if (HitResult.GetActor() == AttackTarget.Get())
		{
			if (!bTargetHit)
			{
				OutTargetHit = HitResult;
				bTargetHit = true;
			}
		}
		else
		{
			bBlocked = true;
		}
	}
	if (bTargetHit)
	{
		return ETraceSampleResult::TargetHit;
	}
	return bBlocked
		? ETraceSampleResult::Blocked
		: ETraceSampleResult::NoHit;
}

bool UAIREEnemyAttackComponent::SweepTraceSegment(
	const FVector& Start,
	const FVector& End,
	FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !OwnerCharacter.IsValid()
		|| !FMath::IsFinite(ActiveMeleeTraceSettings.TraceRadius)
		|| ActiveMeleeTraceSettings.TraceRadius <= 0.0f
		|| ActiveMeleeTraceSettings.TraceChannel.GetValue() >= ECC_MAX)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(AIREEnemyMeleeTrace),
		false,
		OwnerCharacter.Get());
	TArray<AActor*> AttachedActors;
	OwnerCharacter->GetAttachedActors(AttachedActors, true, true);
	QueryParams.AddIgnoredActors(AttachedActors);
	return World->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		FQuat::Identity,
		ActiveMeleeTraceSettings.TraceChannel.GetValue(),
		FCollisionShape::MakeSphere(ActiveMeleeTraceSettings.TraceRadius),
		QueryParams);
}

void UAIREEnemyAttackComponent::ResolveTraceSample(
	const ETraceSampleResult Result,
	const FHitResult& TargetHit)
{
	if (Result == ETraceSampleResult::TargetHit)
	{
		CommitResolvedHit(TargetHit);
	}
	else if (Result == ETraceSampleResult::Blocked)
	{
		CloseTraceWindow();
		CloseOpportunity();
	}
}

void UAIREEnemyAttackComponent::CloseTraceWindow()
{
	bTraceWindowOpen = false;
	bUseSocketTrace = false;
	TraceWindowExecutionId.Invalidate();
	ActiveTraceMesh.Reset();
	PreviousTraceStart = FVector::ZeroVector;
	PreviousTraceEnd = FVector::ZeroVector;
}

void UAIREEnemyAttackComponent::ResetTraceState()
{
	CloseTraceWindow();
	ActiveMeleeTraceSettings = FAIREEnemyMeleeTraceSettings();
	bMontagePlayed = false;
	bTraceWindowEverOpened = false;
}

void UAIREEnemyAttackComponent::ClearMontageEndDelegate()
{
	if (BoundAnimInstance.IsValid() && IsValid(AttackMontage))
	{
		FOnMontageEnded EmptyDelegate;
		BoundAnimInstance->Montage_SetEndDelegate(
			EmptyDelegate,
			AttackMontage);
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
		NextAllowedAttackTime = World->GetTimeSeconds() + CooldownDuration;
	}
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

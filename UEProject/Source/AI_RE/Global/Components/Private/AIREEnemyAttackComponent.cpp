#include "AIREEnemyAttackComponent.h"

#include "AIRECombatDamageSubsystem.h"
#include "AIRECombatDamageTargetInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

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
	const float InFallbackRecoveryDuration)
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
		if (PresentationDuration > 0.0f)
		{
			if (UAnimInstance* AnimInstance =
				OwnerCharacter->GetMesh()->GetAnimInstance())
			{
				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(
					this,
					&UAIREEnemyAttackComponent::HandleMontageEnded);
				AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);
			}
		}
	}

	World->GetTimerManager().SetTimer(
		HitTimerHandle,
		this,
		&UAIREEnemyAttackComponent::HandleFallbackHit,
		FallbackHitDelay,
		false);
	const float RecoveryDuration = FMath::Max3(
		FallbackRecoveryDuration,
		PresentationDuration,
		FallbackHitDelay);
	World->GetTimerManager().SetTimer(
		RecoveryTimerHandle,
		this,
		&UAIREEnemyAttackComponent::FinishAttack,
		RecoveryDuration,
		false);
	OnAttackStarted.Broadcast(Target, ActiveExecutionId);
	return true;
}

bool UAIREEnemyAttackComponent::CommitActiveMeleeHit()
{
	if (!bAttackActive
		|| bHitCommitted
		|| bDamageCancelled
		|| !OwnerCharacter.IsValid())
	{
		return false;
	}

	bHitCommitted = true;
	CloseOpportunity();
	if (TargetingMode != EAIRECombatTargetingMode::SingleTarget
		|| !AttackTarget.IsValid()
		|| !AIRECombatDamageTarget::IsAlive(AttackTarget.Get())
		|| !IsTargetWithinAttackRange(AttackTarget.Get()))
	{
		return false;
	}
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
	bDamageApplied = DamageSubsystem->ApplyDamageRequest(Request)
		== EAIRECombatDamageResult::Applied;
	return bDamageApplied;
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
	if (OwnerCharacter.IsValid() && IsValid(AttackMontage))
	{
		OwnerCharacter->StopAnimMontage(AttackMontage);
	}
	if (!bAttackActive)
	{
		return;
	}
	FinishAttack();
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
	if (Montage == AttackMontage && bAttackActive)
	{
		if (!bInterrupted && bOpportunityOpen)
		{
			CommitActiveMeleeHit();
		}
		else if (bInterrupted)
		{
			bDamageCancelled = true;
		}
		FinishAttack();
	}
}

void UAIREEnemyAttackComponent::HandleFallbackHit()
{
	CommitActiveMeleeHit();
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
	OnAttackFinished.Broadcast(FinishedExecutionId, bCommitted);
}

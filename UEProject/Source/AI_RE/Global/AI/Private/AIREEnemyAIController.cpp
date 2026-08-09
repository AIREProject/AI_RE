#include "AIREEnemyAIController.h"

#include "AIREEnemyAggroComponent.h"
#include "AIREEnemyAttackComponent.h"
#include "AIREEnemyBase.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AIREEnemyConfigDataAsset.h"
#include "AIREEnemyReactionComponent.h"
#include "AIREEnemyVitalityComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"

#if !UE_BUILD_SHIPPING
DEFINE_LOG_CATEGORY_STATIC(LogAIREEnemyAI, Log, All);

namespace
{
const TCHAR* GetAwarenessStateName(
	const EAIREEnemyAwarenessState State)
{
	switch (State)
	{
	case EAIREEnemyAwarenessState::IdleUnaware:
		return TEXT("IdleUnaware");
	case EAIREEnemyAwarenessState::Alerted:
		return TEXT("Alerted");
	case EAIREEnemyAwarenessState::EngagedChase:
		return TEXT("EngagedChase");
	case EAIREEnemyAwarenessState::EngagedAttack:
		return TEXT("EngagedAttack");
	case EAIREEnemyAwarenessState::Searching:
		return TEXT("Searching");
	case EAIREEnemyAwarenessState::Returning:
		return TEXT("Returning");
	case EAIREEnemyAwarenessState::Flinching:
		return TEXT("Flinching");
	case EAIREEnemyAwarenessState::Stunned:
		return TEXT("Stunned");
	case EAIREEnemyAwarenessState::Dead:
		return TEXT("Dead");
	default:
		return TEXT("Unknown");
	}
}
}
#endif

AAIREEnemyAIController::AAIREEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	AggroComponent = CreateDefaultSubobject<UAIREEnemyAggroComponent>(
		TEXT("EnemyAggro"));
	check(AggroComponent);
	SetPerceptionComponent(*AggroComponent);
	StateTreeAIComponent = CreateDefaultSubobject<UStateTreeAIComponent>(
		TEXT("EnemyStateTree"));
	check(StateTreeAIComponent);
	StateTreeAIComponent->SetStartLogicAutomatically(false);
}

void AAIREEnemyAIController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsValid(StateTreeAIComponent)
		|| !StateTreeAIComponent->IsRunning())
	{
		UpdateAwareness();
	}
}

UAIREEnemyAggroComponent* AAIREEnemyAIController::GetAggroComponent() const
{
	return AggroComponent;
}

EAIREEnemyAwarenessState
AAIREEnemyAIController::GetAwarenessState() const
{
	return AwarenessState;
}

FAIREEnemyCombatSnapshot
AAIREEnemyAIController::GetCombatSnapshot() const
{
	FAIREEnemyCombatSnapshot Snapshot;
	Snapshot.AwarenessState = AwarenessState;
	if (IsValid(AggroComponent))
	{
		Snapshot.Aggro = AggroComponent->GetAggroSnapshot();
	}
	if (Enemy.IsValid()
		&& IsValid(Enemy->GetEnemyAttackComponent()))
	{
		Snapshot.Attack =
			Enemy->GetEnemyAttackComponent()->GetAttackSnapshot();
	}
	return Snapshot;
}

void AAIREEnemyAIController::TickStateTree(const float DeltaSeconds)
{
	if (IsValid(StateTreeAIComponent)
		&& StateTreeAIComponent->IsRunning())
	{
		UpdateAwareness();
	}
}

void AAIREEnemyAIController::ExitStateTreeState(
	const EAIREEnemyAwarenessState ExpectedState)
{
	if (AwarenessState != ExpectedState)
	{
		return;
	}

	StopMovement();
	ResetCombatApproach();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (ExpectedState == EAIREEnemyAwarenessState::EngagedAttack
		&& Enemy.IsValid()
		&& IsValid(Enemy->GetEnemyAttackComponent()))
	{
		Enemy->GetEnemyAttackComponent()->CancelCurrentAttack();
	}
}

bool AAIREEnemyAIController::ReportStateTreeAwarenessState(
	const EAIREEnemyAwarenessState NewState)
{
	if (!IsValid(StateTreeAIComponent)
		|| !StateTreeAIComponent->IsRunning()
		|| !Enemy.IsValid()
		|| AwarenessState == EAIREEnemyAwarenessState::Dead
		|| NewState == EAIREEnemyAwarenessState::Dead
		|| NewState == EAIREEnemyAwarenessState::Flinching
		|| NewState == EAIREEnemyAwarenessState::Stunned)
	{
		return false;
	}

	const UAIREEnemyVitalityComponent* Vitality =
		Enemy->GetEnemyVitalityComponent();
	const UAIREEnemyReactionComponent* Reaction =
		Enemy->GetEnemyReactionComponent();
	if (!IsValid(Vitality)
		|| !IsValid(Reaction)
		|| Vitality->IsDead()
		|| Reaction->GetReactionSnapshot().State
			!= EAIREEnemyReactionState::None)
	{
		return false;
	}

	SetAwarenessState(NewState, TEXT("StateTree report"));
	return true;
}

void AAIREEnemyAIController::RequestReturnHome()
{
	if (AwarenessState != EAIREEnemyAwarenessState::Dead)
	{
		bReturnRequested = true;
		BeginReturning(TEXT("Explicit return requested"));
	}
}

void AAIREEnemyAIController::ReportCombatDamage(
	AActor* Source,
	const float Damage)
{
	if (IsValid(AggroComponent)
		&& AwarenessState != EAIREEnemyAwarenessState::Dead)
	{
		AggroComponent->ReportDamage(Source, Damage);
	}
}

void AAIREEnemyAIController::HandleEnemyDeath()
{
	if (AwarenessState == EAIREEnemyAwarenessState::Dead)
	{
		return;
	}

	StopMovement();
	ResetEngagementDecision(true);
	if (Enemy.IsValid() && IsValid(Enemy->GetEnemyAttackComponent()))
	{
		Enemy->GetEnemyAttackComponent()->CancelCurrentAttack();
	}
	if (IsValid(AggroComponent))
	{
		AggroComponent->StopAggroTracking();
	}
	ClearFocus(EAIFocusPriority::Gameplay);
	bReturnRequested = false;
	SetAwarenessState(EAIREEnemyAwarenessState::Dead, TEXT("Enemy death"));
	SetActorTickEnabled(false);
}

void AAIREEnemyAIController::HandleEnemyReactionChanged(
	const EAIREEnemyReactionState ReactionState)
{
	if (AwarenessState == EAIREEnemyAwarenessState::Dead)
	{
		return;
	}
	if (ReactionState == EAIREEnemyReactionState::None)
	{
		return;
	}
	StopMovement();
	ResetCombatApproach();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (Enemy.IsValid() && IsValid(Enemy->GetEnemyAttackComponent()))
	{
		Enemy->GetEnemyAttackComponent()->CancelCurrentAttack();
	}
	SetAwarenessState(
		ReactionState == EAIREEnemyReactionState::Stunned
			? EAIREEnemyAwarenessState::Stunned
			: EAIREEnemyAwarenessState::Flinching,
		TEXT("Reaction started"));
}

void AAIREEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Enemy = Cast<AAIREEnemyBase>(InPawn);
	if (!Enemy.IsValid())
	{
		SetActorTickEnabled(false);
		return;
	}
	HomeLocation = Enemy->GetActorLocation();
	const UAIREEnemyConfigDataAsset* Config = Enemy->GetEnemyConfig();
	check(Config);
	HomeLeashRadius = Config->HomeLeashRadius;
	BaseMovementSpeed = Config->MovementSpeed;
	CombatSprintSpeed = Config->CombatSprintSpeed;
	CombatSprintStartDistance = Config->CombatSprintStartDistance;
	TacticalApproachDistance = Config->TacticalApproachDistance;
	TacticalLateralOffset = Config->TacticalLateralOffset;
	TacticalMoveDuration = Config->TacticalMoveDuration;
	bUseCombatApproachActions = Config->bUseCombatApproachActions;
	NextLateralSide = 1;
	ResetEngagementDecision(true);
	bReturnRequested = false;
	AwarenessState = EAIREEnemyAwarenessState::IdleUnaware;
	StateDeadline = 0.0;
	AggroComponent->StartAggroTracking(Enemy.Get());
	StateTreeAIComponent->StartLogic();
	SetActorTickEnabled(true);
}

void AAIREEnemyAIController::OnUnPossess()
{
	StopMovement();
	ResetEngagementDecision(true);
	if (IsValid(AggroComponent))
	{
		AggroComponent->StopAggroTracking();
	}
	if (IsValid(StateTreeAIComponent))
	{
		StateTreeAIComponent->StopLogic(
			TEXT("Enemy controller released its pawn."));
	}
	Enemy.Reset();
	bReturnRequested = false;
	SetActorTickEnabled(false);
	Super::OnUnPossess();
}

void AAIREEnemyAIController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopMovement();
	ResetEngagementDecision(true);
	ClearFocus(EAIFocusPriority::Gameplay);
	if (IsValid(AggroComponent))
	{
		AggroComponent->StopAggroTracking();
	}
	if (IsValid(StateTreeAIComponent))
	{
		StateTreeAIComponent->StopLogic(TEXT("Enemy controller ended play."));
	}
	Enemy.Reset();
	bReturnRequested = false;
	Super::EndPlay(EndPlayReason);
}

void AAIREEnemyAIController::UpdateAwareness()
{
	if (!Enemy.IsValid())
	{
		return;
	}
	UAIREEnemyVitalityComponent* Vitality =
		Enemy->GetEnemyVitalityComponent();
	UAIREEnemyReactionComponent* Reaction =
		Enemy->GetEnemyReactionComponent();
	UAIREEnemyAttackComponent* Attack =
		Enemy->GetEnemyAttackComponent();
	if (!IsValid(Vitality)
		|| !IsValid(Reaction)
		|| !IsValid(Attack)
		|| Vitality->IsDead())
	{
		HandleEnemyDeath();
		return;
	}

	const EAIREEnemyReactionState ReactionState =
		Reaction->GetReactionSnapshot().State;
	if (ReactionState != EAIREEnemyReactionState::None)
	{
		HandleEnemyReactionChanged(ReactionState);
		return;
	}
	if (AwarenessState == EAIREEnemyAwarenessState::Flinching
		|| AwarenessState == EAIREEnemyAwarenessState::Stunned)
	{
		if (bReturnRequested)
		{
			BeginReturning(TEXT("Deferred return after reaction"));
			return;
		}
		SetAwarenessState(
			EAIREEnemyAwarenessState::EngagedChase,
			TEXT("Reaction recovered"));
	}
	if (AwarenessState == EAIREEnemyAwarenessState::Returning)
	{
		if (HasReachedHome())
		{
			CompleteReturnHome();
		}
		else if (GetMoveStatus() == EPathFollowingStatus::Idle
			&& MoveToLocation(
				HomeLocation,
				HomeAcceptanceRadius,
				false) != EPathFollowingRequestResult::RequestSuccessful)
		{
			CompleteReturnHome();
		}
		return;
	}

	AggroComponent->RefreshSelection();
	const FAIREEnemyAttackSnapshot AttackSnapshot =
		Attack->GetAttackSnapshot();
	if (AttackSnapshot.bActive)
	{
		const AActor* AttackTarget = AttackSnapshot.Target.Get();
		if (IsOutsideHomeLeash(AttackTarget))
		{
			Attack->CancelCurrentAttack();
			BeginReturning(TEXT("Active attack exceeded home leash"));
			return;
		}
		if (AIRECombatDamageTarget::IsAlive(AttackSnapshot.Target.Get()))
		{
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedAttack,
				TEXT("Active attack remains valid"));
			return;
		}
		Attack->CancelCurrentAttack();
		AggroComponent->RefreshSelection();
	}

	AActor* Target = AggroComponent->GetSelectedTarget();
	if (AwarenessState == EAIREEnemyAwarenessState::Searching)
	{
		const bool bCanReengage = IsValid(Target)
			&& (AggroComponent->SelectedTargetHasRecentSightEvidence()
				|| AggroComponent->SelectedTargetHasRecentDamageEvidence());
		if (!bCanReengage)
		{
			if (GetWorld()->GetTimeSeconds() >= StateDeadline)
			{
				BeginReturning(TEXT("Search duration elapsed"));
			}
			else if (MoveToLocation(SearchLocation, HomeAcceptanceRadius)
				== EPathFollowingRequestResult::Failed)
			{
				BeginReturning(TEXT("Search movement failed"));
			}
			return;
		}
	}
	if (IsValid(Target))
	{
		SetFocus(Target, EAIFocusPriority::Gameplay);
		const bool bSightEngagement =
			AggroComponent->SelectedTargetHasRecentSightEvidence();
		const bool bDamageOnlyEngagement =
			AggroComponent->SelectedTargetHasRecentDamageEvidence();
		if (!bSightEngagement && !bDamageOnlyEngagement)
		{
			BeginSearching();
			return;
		}
		if (AwarenessState == EAIREEnemyAwarenessState::IdleUnaware
			|| AwarenessState == EAIREEnemyAwarenessState::Returning)
		{
			if (bDamageOnlyEngagement && !bSightEngagement)
			{
				SetAwarenessState(
					EAIREEnemyAwarenessState::EngagedChase,
					TEXT("Damage evidence engagement"));
			}
			else
			{
				StateDeadline = GetWorld()->GetTimeSeconds() + AlertDuration;
				SetAwarenessState(
					EAIREEnemyAwarenessState::Alerted,
					TEXT("Sight target acquired"));
				StopMovement();
				return;
			}
		}
		if (AwarenessState == EAIREEnemyAwarenessState::Alerted)
		{
			if (GetWorld()->GetTimeSeconds() < StateDeadline)
			{
				return;
			}
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedChase,
				TEXT("Alert duration elapsed"));
		}
		UpdateEngagement(Target);
		return;
	}

	if (AwarenessState != EAIREEnemyAwarenessState::Returning
		&& AwarenessState != EAIREEnemyAwarenessState::IdleUnaware)
	{
		BeginReturning(TEXT("Aggro target lost"));
		return;
	}
}

void AAIREEnemyAIController::UpdateEngagement(AActor* Target)
{
	UAIREEnemyAttackComponent* Attack = Enemy.IsValid()
		? Enemy->GetEnemyAttackComponent()
		: nullptr;
	if (!IsValid(Attack) || !IsValid(Target))
	{
		return;
	}
	if (IsOutsideHomeLeash(Target))
	{
		BeginReturning(TEXT("Engagement exceeded home leash"));
		return;
	}
	if (bUseCombatApproachActions)
	{
		UpdateCombatApproach(Target, Attack);
		return;
	}
	if (Attack->IsTargetWithinAttackRange(Target))
	{
		if (Attack->TryStartMeleeAttack(Target))
		{
			if (GetMoveStatus() != EPathFollowingStatus::Idle)
			{
				StopMovement();
			}
			ClearFocus(EAIFocusPriority::Gameplay);
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedAttack,
				TEXT("Melee attack started"));
			return;
		}

		const float PreferredAttackRange =
			Attack->GetPreferredAttackRange();
		if (Attack->GetTargetSurfaceDistance(Target)
			> PreferredAttackRange)
		{
			const UPathFollowingComponent* PathFollowing =
				GetPathFollowingComponent();
			const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
			const bool bAlreadyClosing = IsValid(PathFollowing)
				&& PathFollowing->GetMoveGoal() == Target
				&& (MoveStatus == EPathFollowingStatus::Waiting
					|| MoveStatus == EPathFollowingStatus::Moving);
			if (!bAlreadyClosing
				&& MoveToActor(Target, PreferredAttackRange, true)
					== EPathFollowingRequestResult::Failed)
			{
				BeginSearching();
				return;
			}
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedChase,
				TEXT("Closing preferred melee range"));
			return;
		}

		SetAwarenessState(
			EAIREEnemyAwarenessState::EngagedChase,
			TEXT("Holding melee range while attack is unavailable"));
		return;
	}
	const UPathFollowingComponent* PathFollowing =
		GetPathFollowingComponent();
	const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
	const bool bAlreadyChasingTarget = IsValid(PathFollowing)
		&& PathFollowing->GetMoveGoal() == Target
		&& (MoveStatus == EPathFollowingStatus::Waiting
			|| MoveStatus == EPathFollowingStatus::Moving);
	if (!bAlreadyChasingTarget
		&& MoveToActor(Target, Attack->GetAttackRange(), true)
			== EPathFollowingRequestResult::Failed)
	{
		BeginSearching();
		return;
	}
	SetAwarenessState(
		EAIREEnemyAwarenessState::EngagedChase,
		TEXT("Move to attack range requested"));
}

bool AAIREEnemyAIController::UpdateCombatApproach(
	AActor* Target,
	UAIREEnemyAttackComponent* Attack)
{
	if (!Enemy.IsValid() || !IsValid(Target) || !IsValid(Attack))
	{
		return false;
	}

	if (EngagementDecisionTarget.Get() != Target)
	{
		if (EngagementDecisionTarget.IsValid())
		{
			StopMovement();
		}
		ResetCombatApproach();
		EngagementDecisionTarget = Target;
		bCooldownRepositionConsumed = false;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
	if (CombatApproachMove == EAIREEnemyCombatApproachMove::Lateral)
	{
		const bool bLateralMoveActive = CombatApproachTarget.Get() == Target
			&& Now < TacticalMoveDeadline
			&& (MoveStatus == EPathFollowingStatus::Waiting
				|| MoveStatus == EPathFollowingStatus::Moving);
		if (bLateralMoveActive)
		{
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedChase,
				TEXT("Tactical lateral approach active"));
			return true;
		}

		if (MoveStatus != EPathFollowingStatus::Idle)
		{
			StopMovement();
		}
		ResetCombatApproach();
		CombatApproachTarget = Target;
	}

	const float SurfaceDistance =
		Attack->GetTargetSurfaceDistance(Target);
	if (SurfaceDistance > CombatSprintStartDistance)
	{
		SetEngagementMovementSpeed(CombatSprintSpeed);
		const UPathFollowingComponent* PathFollowing =
			GetPathFollowingComponent();
		const bool bAlreadySprintingToTarget =
			CombatApproachMove == EAIREEnemyCombatApproachMove::Sprint
			&& CombatApproachTarget.Get() == Target
			&& IsValid(PathFollowing)
			&& PathFollowing->GetMoveGoal() == Target
			&& (MoveStatus == EPathFollowingStatus::Waiting
				|| MoveStatus == EPathFollowingStatus::Moving);
		if (!bAlreadySprintingToTarget)
		{
			if (MoveToActor(
					Target,
					CombatSprintStartDistance,
					true)
				== EPathFollowingRequestResult::Failed)
			{
				ResetCombatApproach();
				BeginSearching();
				return true;
			}
			CombatApproachMove =
				EAIREEnemyCombatApproachMove::Sprint;
			CombatApproachTarget = Target;
		}
		SetAwarenessState(
			EAIREEnemyAwarenessState::EngagedChase,
			TEXT("Combat sprint approach active"));
		return true;
	}

	if (CombatApproachMove == EAIREEnemyCombatApproachMove::Sprint)
	{
		StopMovement();
		ResetCombatApproach();
	}
	SetEngagementMovementSpeed(BaseMovementSpeed);

	if (SurfaceDistance > Attack->GetAttackRange())
	{
		if (!StartOrContinueDirectApproach(
				Target,
				Attack->GetAttackRange()))
		{
			ResetCombatApproach();
			BeginSearching();
			return true;
		}
		SetAwarenessState(
			EAIREEnemyAwarenessState::EngagedChase,
			TEXT("Closing outer attack range"));
		return true;
	}

	const float PreferredAttackRange = Attack->GetPreferredAttackRange();
	const bool bWithinPreferredRange =
		SurfaceDistance <= PreferredAttackRange;
	const bool bAttackOnCooldown =
		Attack->GetRemainingAttackCooldown() > UE_SMALL_NUMBER;
	const bool bRequiresMeleeFollowUp =
		Attack->RequiresNonGapCloserFollowUp();
	const bool bMustCloseForMeleeFollowUp =
		bRequiresMeleeFollowUp && !bWithinPreferredRange;
	if (!bAttackOnCooldown)
	{
		bCooldownRepositionConsumed = false;
		if (Attack->TryStartMeleeAttack(Target))
		{
			if (GetMoveStatus() != EPathFollowingStatus::Idle)
			{
				StopMovement();
			}
			ResetCombatApproach();
			ClearFocus(EAIFocusPriority::Gameplay);
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedAttack,
				TEXT("Combat approach attack started"));
			return true;
		}

		if (!bWithinPreferredRange)
		{
			if (!StartOrContinueDirectApproach(
					Target,
					PreferredAttackRange))
			{
				ResetCombatApproach();
				BeginSearching();
				return true;
			}
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedChase,
				bMustCloseForMeleeFollowUp
					? TEXT("Closing for required melee follow-up")
					: TEXT("Closing pattern range after attack rejection"));
			return true;
		}

		SetAwarenessState(
			EAIREEnemyAwarenessState::EngagedChase,
			TEXT("Holding preferred range after attack rejection"));
		return true;
	}

	if (bRequiresMeleeFollowUp)
	{
		if (bMustCloseForMeleeFollowUp
			&& !StartOrContinueDirectApproach(
				Target,
				PreferredAttackRange))
		{
			ResetCombatApproach();
			BeginSearching();
			return true;
		}
		SetAwarenessState(
			EAIREEnemyAwarenessState::EngagedChase,
			bMustCloseForMeleeFollowUp
				? TEXT("Closing melee follow-up during cooldown")
				: TEXT("Holding melee follow-up range during cooldown"));
		return true;
	}

	if (bCooldownRepositionConsumed)
	{
		SetAwarenessState(
			EAIREEnemyAwarenessState::EngagedChase,
			TEXT("Holding range after cooldown reposition"));
		return true;
	}

	const float DesiredLateralSurfaceDistance =
		bWithinPreferredRange
		? PreferredAttackRange
		: TacticalApproachDistance;
	if (StartLateralApproach(Target, DesiredLateralSurfaceDistance))
	{
		bCooldownRepositionConsumed = true;
		SetAwarenessState(
			EAIREEnemyAwarenessState::EngagedChase,
			TEXT("Tactical lateral approach requested"));
		return true;
	}
	bCooldownRepositionConsumed = true;
	CombatApproachTarget = Target;
	SetAwarenessState(
		EAIREEnemyAwarenessState::EngagedChase,
		TEXT("Holding range after tactical move failed"));
	return true;
}

bool AAIREEnemyAIController::StartOrContinueDirectApproach(
	AActor* Target,
	const float AcceptanceRadius)
{
	if (!IsValid(Target)
		|| !FMath::IsFinite(AcceptanceRadius)
		|| AcceptanceRadius < 0.0f)
	{
		return false;
	}

	const UPathFollowingComponent* PathFollowing =
		GetPathFollowingComponent();
	const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
	const bool bAlreadyClosingToTarget =
		CombatApproachMove == EAIREEnemyCombatApproachMove::Direct
		&& CombatApproachTarget.Get() == Target
		&& IsValid(PathFollowing)
		&& PathFollowing->GetMoveGoal() == Target
		&& (MoveStatus == EPathFollowingStatus::Waiting
			|| MoveStatus == EPathFollowingStatus::Moving);
	if (bAlreadyClosingToTarget)
	{
		return true;
	}

	if (MoveToActor(Target, AcceptanceRadius, true)
		== EPathFollowingRequestResult::Failed)
	{
		return false;
	}
	CombatApproachMove = EAIREEnemyCombatApproachMove::Direct;
	CombatApproachTarget = Target;
	return true;
}

bool AAIREEnemyAIController::StartLateralApproach(
	AActor* Target,
	const float DesiredSurfaceDistance)
{
	if (!Enemy.IsValid()
		|| !IsValid(Target)
		|| !FMath::IsFinite(DesiredSurfaceDistance)
		|| DesiredSurfaceDistance <= 0.0f)
	{
		return false;
	}

	FVector Radial =
		Enemy->GetActorLocation() - Target->GetActorLocation();
	Radial.Z = 0.0f;
	if (!Radial.Normalize())
	{
		Radial = -Target->GetActorForwardVector();
		Radial.Z = 0.0f;
		if (!Radial.Normalize())
		{
			return false;
		}
	}

	const FVector Tangent = FVector::CrossProduct(
		FVector::UpVector,
		Radial) * static_cast<float>(NextLateralSide);
	const float OrbitRadius = DesiredSurfaceDistance
		+ Enemy->GetSimpleCollisionRadius()
		+ Target->GetSimpleCollisionRadius();
	const float LateralOffset = FMath::Min(
		TacticalLateralOffset,
		FMath::Max(80.0f, DesiredSurfaceDistance * 0.75f));
	TacticalMoveDestination = Target->GetActorLocation()
		+ Radial * OrbitRadius
		+ Tangent * LateralOffset;
	TacticalMoveDestination.Z = Enemy->GetActorLocation().Z;

	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
		TacticalMoveDestination,
		HomeAcceptanceRadius,
		false,
		true,
		true,
		true);
	if (MoveResult != EPathFollowingRequestResult::RequestSuccessful)
	{
		return false;
	}

	CombatApproachMove = EAIREEnemyCombatApproachMove::Lateral;
	CombatApproachTarget = Target;
	TacticalMoveDeadline =
		GetWorld()->GetTimeSeconds() + TacticalMoveDuration;
	NextLateralSide *= -1;
#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogAIREEnemyAI,
		Verbose,
		TEXT("Tactical approach Boss=%s Target=%s Destination=%s Duration=%.2f"),
		*GetNameSafe(Enemy.Get()),
		*GetNameSafe(Target),
		*TacticalMoveDestination.ToCompactString(),
		TacticalMoveDuration);
#endif
	return true;
}

void AAIREEnemyAIController::ResetCombatApproach()
{
	CombatApproachMove = EAIREEnemyCombatApproachMove::None;
	CombatApproachTarget.Reset();
	TacticalMoveDestination = FVector::ZeroVector;
	TacticalMoveDeadline = 0.0;
	SetEngagementMovementSpeed(BaseMovementSpeed);
}

void AAIREEnemyAIController::ResetEngagementDecision(
	const bool bResetAttackSequence)
{
	ResetCombatApproach();
	EngagementDecisionTarget.Reset();
	bCooldownRepositionConsumed = false;
	if (bResetAttackSequence && Enemy.IsValid())
	{
		if (UAIREEnemyAttackComponent* Attack =
			Enemy->GetEnemyAttackComponent())
		{
			Attack->ResetAttackSequence();
		}
	}
}

void AAIREEnemyAIController::SetEngagementMovementSpeed(
	const float Speed)
{
	if (Enemy.IsValid())
	{
		if (UCharacterMovementComponent* Movement =
			Enemy->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = FMath::Max(0.0f, Speed);
		}
	}
}

void AAIREEnemyAIController::BeginSearching()
{
	if (AwarenessState == EAIREEnemyAwarenessState::Searching)
	{
		return;
	}
	StopMovement();
	ResetCombatApproach();
	ClearFocus(EAIFocusPriority::Gameplay);
	SearchLocation = AggroComponent->GetSelectedTargetLastKnownLocation();
	StateDeadline = GetWorld()->GetTimeSeconds() + SearchDuration;
	SetAwarenessState(EAIREEnemyAwarenessState::Searching, TEXT("Target lost"));
}

void AAIREEnemyAIController::BeginReturning(const TCHAR* const Reason)
{
	bReturnRequested = true;
	if (Enemy.IsValid()
		&& Enemy->GetEnemyReactionComponent()->GetReactionSnapshot().State
			!= EAIREEnemyReactionState::None)
	{
		return;
	}
	StopMovement();
	ResetEngagementDecision(true);
	ClearFocus(EAIFocusPriority::Gameplay);
	if (Enemy.IsValid())
	{
		Enemy->GetEnemyAttackComponent()->CancelCurrentAttack();
	}
	AggroComponent->StopAggroTracking();
	SetAwarenessState(EAIREEnemyAwarenessState::Returning, Reason);
	if (MoveToLocation(HomeLocation, HomeAcceptanceRadius, false)
		== EPathFollowingRequestResult::Failed)
	{
		CompleteReturnHome();
	}
}

void AAIREEnemyAIController::CompleteReturnHome()
{
	StopMovement();
	ResetEngagementDecision(true);
	ClearFocus(EAIFocusPriority::Gameplay);
	if (Enemy.IsValid())
	{
		Enemy->GetEnemyReactionComponent()->ResetForReturnHome();
		AggroComponent->StartAggroTracking(Enemy.Get());
	}
	bReturnRequested = false;
	SetAwarenessState(
		EAIREEnemyAwarenessState::IdleUnaware,
		TEXT("Return home complete"));
}

void AAIREEnemyAIController::SetAwarenessState(
	const EAIREEnemyAwarenessState NewState,
	const TCHAR* const Reason)
{
	if (AwarenessState == NewState)
	{
		return;
	}
	const EAIREEnemyAwarenessState PreviousState = AwarenessState;
	AwarenessState = NewState;
#if !UE_BUILD_SHIPPING
	const FAIREEnemyAggroSnapshot AggroSnapshot = IsValid(AggroComponent)
		? AggroComponent->GetAggroSnapshot()
		: FAIREEnemyAggroSnapshot();
	UE_LOG(
		LogAIREEnemyAI,
		Log,
		TEXT("Awareness transition Boss=%s Target=%s Revision=%lld Visible=%d RecentSight=%d RecentDamage=%d %s -> %s Reason=%s"),
		*GetNameSafe(Enemy.Get()),
		*GetNameSafe(AggroSnapshot.SelectedTarget.Get()),
		AggroSnapshot.TargetRevision,
		IsValid(AggroComponent) && AggroComponent->IsSelectedTargetVisible(),
		IsValid(AggroComponent) && AggroComponent->SelectedTargetHasRecentSightEvidence(),
		IsValid(AggroComponent) && AggroComponent->SelectedTargetHasRecentDamageEvidence(),
		GetAwarenessStateName(PreviousState),
		GetAwarenessStateName(AwarenessState),
		Reason);
#else
	(void)Reason;
#endif
	OnAwarenessStateChanged.Broadcast(PreviousState, AwarenessState);
}

bool AAIREEnemyAIController::HasReachedHome() const
{
	return Enemy.IsValid()
		&& FVector::DistSquared2D(
			Enemy->GetActorLocation(),
			HomeLocation)
		<= FMath::Square(HomeAcceptanceRadius);
}

bool AAIREEnemyAIController::IsOutsideHomeLeash(
	const AActor* const Target) const
{
	if (HomeLeashRadius <= 0.0f || !Enemy.IsValid())
	{
		return false;
	}

	const float HomeLeashRadiusSquared = FMath::Square(HomeLeashRadius);
	return FVector::DistSquared2D(
		Enemy->GetActorLocation(),
		HomeLocation) > HomeLeashRadiusSquared
		|| (IsValid(Target)
			&& FVector::DistSquared2D(
				Target->GetActorLocation(),
				HomeLocation) > HomeLeashRadiusSquared);
}

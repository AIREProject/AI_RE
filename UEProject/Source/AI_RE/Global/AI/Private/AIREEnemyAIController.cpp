#include "AIREEnemyAIController.h"

#include "AIREEnemyAggroComponent.h"
#include "AIREEnemyAttackComponent.h"
#include "AIREEnemyBase.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AIREEnemyReactionComponent.h"
#include "AIREEnemyVitalityComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
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
		BeginReturning();
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
			BeginReturning();
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
		else if (MoveToLocation(HomeLocation, HomeAcceptanceRadius)
			== EPathFollowingRequestResult::Failed)
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
		const bool bAttackOutsideHomeLeash =
			FVector::DistSquared2D(
				Enemy->GetActorLocation(),
				HomeLocation) > FMath::Square(HomeLeashRadius)
			|| (IsValid(AttackTarget)
				&& FVector::DistSquared2D(
					AttackTarget->GetActorLocation(),
					HomeLocation) > FMath::Square(HomeLeashRadius));
		if (bAttackOutsideHomeLeash)
		{
			Attack->CancelCurrentAttack();
			BeginReturning();
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
			&& (AggroComponent->IsSelectedTargetVisible()
				|| AggroComponent->SelectedTargetHasRecentDamageEvidence());
		if (!bCanReengage)
		{
			if (GetWorld()->GetTimeSeconds() >= StateDeadline)
			{
				BeginReturning();
			}
			else if (MoveToLocation(SearchLocation, HomeAcceptanceRadius)
				== EPathFollowingRequestResult::Failed)
			{
				BeginReturning();
			}
			return;
		}
	}
	if (IsValid(Target))
	{
		const bool bVisible = AggroComponent->IsSelectedTargetVisible();
		const bool bDamageOnlyEngagement =
			AggroComponent->SelectedTargetHasRecentDamageEvidence();
		if (!bVisible && !bDamageOnlyEngagement)
		{
			BeginSearching();
			return;
		}
		if (AwarenessState == EAIREEnemyAwarenessState::IdleUnaware
			|| AwarenessState == EAIREEnemyAwarenessState::Returning)
		{
			if (bDamageOnlyEngagement)
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
		BeginReturning();
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
	const bool bOutsideHomeLeash = FVector::DistSquared2D(
		Enemy->GetActorLocation(),
		HomeLocation) > FMath::Square(HomeLeashRadius)
		|| FVector::DistSquared2D(
			Target->GetActorLocation(),
			HomeLocation) > FMath::Square(HomeLeashRadius);
	if (bOutsideHomeLeash)
	{
		BeginReturning();
		return;
	}
	if (Attack->IsTargetWithinAttackRange(Target))
	{
		StopMovement();
		if (Attack->TryStartMeleeAttack(Target))
		{
			ClearFocus(EAIFocusPriority::Gameplay);
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedAttack,
				TEXT("Melee attack started"));
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
			SetAwarenessState(
				EAIREEnemyAwarenessState::EngagedChase,
				TEXT("Melee attack start rejected"));
		}
		return;
	}
	ClearFocus(EAIFocusPriority::Gameplay);
	if (MoveToActor(Target, Attack->GetAttackRange(), false)
		== EPathFollowingRequestResult::Failed)
	{
		BeginSearching();
		return;
	}
	SetAwarenessState(
		EAIREEnemyAwarenessState::EngagedChase,
		TEXT("Move to attack range requested"));
}

void AAIREEnemyAIController::BeginSearching()
{
	if (AwarenessState == EAIREEnemyAwarenessState::Searching)
	{
		return;
	}
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	SearchLocation = AggroComponent->GetSelectedTargetLastKnownLocation();
	StateDeadline = GetWorld()->GetTimeSeconds() + SearchDuration;
	SetAwarenessState(EAIREEnemyAwarenessState::Searching, TEXT("Target lost"));
}

void AAIREEnemyAIController::BeginReturning()
{
	bReturnRequested = true;
	if (Enemy.IsValid()
		&& Enemy->GetEnemyReactionComponent()->GetReactionSnapshot().State
			!= EAIREEnemyReactionState::None)
	{
		return;
	}
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (Enemy.IsValid())
	{
		Enemy->GetEnemyAttackComponent()->CancelCurrentAttack();
	}
	AggroComponent->StopAggroTracking();
	SetAwarenessState(EAIREEnemyAwarenessState::Returning, TEXT("Return requested"));
	if (MoveToLocation(HomeLocation, HomeAcceptanceRadius)
		== EPathFollowingRequestResult::Failed)
	{
		CompleteReturnHome();
	}
}

void AAIREEnemyAIController::CompleteReturnHome()
{
	StopMovement();
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
		TEXT("Awareness transition Boss=%s Target=%s Revision=%lld Visible=%d RecentDamage=%d %s -> %s Reason=%s"),
		*GetNameSafe(Enemy.Get()),
		*GetNameSafe(AggroSnapshot.SelectedTarget.Get()),
		AggroSnapshot.TargetRevision,
		IsValid(AggroComponent) && AggroComponent->IsSelectedTargetVisible(),
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

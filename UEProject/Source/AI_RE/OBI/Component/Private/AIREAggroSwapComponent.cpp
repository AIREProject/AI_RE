#include "AIREAggroSwapComponent.h"

#include "AIRECombatDamageTargetInterface.h"
#include "AIRECombatEvadeComponent.h"
#include "AIREEnemyAggroComponent.h"
#include "AIREEnemyAIController.h"
#include "AIREEnemyAttackComponent.h"
#include "AIREEnemyBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIREAggroSwap, Log, All);

UAIREAggroSwapComponent::UAIREAggroSwapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

EAIREAggroSwapResult UAIREAggroSwapComponent::TryAggroSwap()
{
	TWeakObjectPtr<AAIREEnemyBase> Enemy;
	FAIREEnemyAttackSnapshot Attack;
	const EAIREAggroSwapResult OpportunityResult =
		FindActiveOpportunity(Enemy, Attack);
	if (OpportunityResult != EAIREAggroSwapResult::Applied)
	{
		return OpportunityResult;
	}
	if (IsOnCooldown())
	{
		return EAIREAggroSwapResult::OnCooldown;
	}

	APlayerController* PlayerController = OwnerController.Get();
	APawn* PlayerPawn = IsValid(PlayerController)
		? PlayerController->GetPawn()
		: nullptr;
	if (!AIRECombatDamageTarget::IsAlive(PlayerPawn))
	{
		return EAIREAggroSwapResult::InvalidPartyState;
	}

	AActor* OtherPartyActor = nullptr;
	int32 OtherPartyActorCount = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate)
			|| Candidate == PlayerPawn
			|| !Candidate->GetClass()->ImplementsInterface(
				UAIRECombatDamageTargetInterface::StaticClass()))
		{
			continue;
		}
		const IAIRECombatDamageTargetInterface* Combatant =
			Cast<IAIRECombatDamageTargetInterface>(Candidate);
		if (Combatant
			&& Combatant->GetCombatAffiliation()
				== EAIRECombatAffiliation::PlayerParty
			&& Combatant->IsCombatTargetAlive()
			&& IsValid(Candidate->FindComponentByClass<
				UAIRECombatEvadeComponent>()))
		{
			OtherPartyActor = Candidate;
			++OtherPartyActorCount;
		}
	}
	if (OtherPartyActorCount != 1 || !IsValid(OtherPartyActor))
	{
		return EAIREAggroSwapResult::InvalidPartyState;
	}

	AAIREEnemyAIController* BossController =
		Cast<AAIREEnemyAIController>(Enemy->GetController());
	UAIREEnemyAggroComponent* Aggro = IsValid(BossController)
		? BossController->GetAggroComponent()
		: nullptr;
	AActor* CurrentTarget = Attack.Target.Get();
	AActor* NewTarget = nullptr;
	UAIRECombatEvadeComponent* Evade = nullptr;
	if (CurrentTarget == PlayerPawn)
	{
		NewTarget = OtherPartyActor;
		Evade = PlayerPawn->FindComponentByClass<UAIRECombatEvadeComponent>();
	}
	else if (CurrentTarget == OtherPartyActor)
	{
		NewTarget = PlayerPawn;
		Evade = OtherPartyActor->FindComponentByClass<
			UAIRECombatEvadeComponent>();
	}
	if (!IsValid(Aggro)
		|| !FMath::IsFinite(CooldownDuration)
		|| CooldownDuration < 0.0f
		|| !AIRECombatDamageTarget::IsAlive(CurrentTarget)
		|| !AIRECombatDamageTarget::IsAlive(NewTarget)
		|| !IsValid(Evade)
		|| !Evade->CanStartLateralDash(Enemy.Get()))
	{
		return EAIREAggroSwapResult::InvalidPartyState;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return EAIREAggroSwapResult::InvalidPartyState;
	}
	NextAllowedSwapTime = World->GetTimeSeconds() + CooldownDuration;

	UAIREEnemyAttackComponent* BossAttack = Enemy->GetEnemyAttackComponent();
	if (!IsValid(BossAttack)
		|| !BossAttack->TryCancelDamageForAggroSwap(Attack.ExecutionId))
	{
		return EAIREAggroSwapResult::CommitRejected;
	}
	BossController->ClearFocus(EAIFocusPriority::Gameplay);
	// The evade component owns movement and active GAS ability cancellation.
	if (!Evade->TryStartLateralDash(Enemy.Get()))
	{
		return EAIREAggroSwapResult::EvadeRejected;
	}
	if (!Aggro->PromoteTargetAboveCurrentMaximum(NewTarget))
	{
		return EAIREAggroSwapResult::AggroRejected;
	}

	UE_LOG(
		LogAIREAggroSwap,
		Log,
		TEXT("Boss aggro swap applied. Boss=%s PreviousTarget=%s NewTarget=%s ExecutionId=%s"),
		*GetNameSafe(Enemy.Get()),
		*GetNameSafe(CurrentTarget),
		*GetNameSafe(NewTarget),
		*Attack.ExecutionId.ToString());
	return EAIREAggroSwapResult::Applied;
}

float UAIREAggroSwapComponent::GetRemainingCooldown() const
{
	const UWorld* World = GetWorld();
	return IsValid(World)
		? static_cast<float>(FMath::Max(
			0.0,
			NextAllowedSwapTime - World->GetTimeSeconds()))
		: 0.0f;
}

bool UAIREAggroSwapComponent::IsOnCooldown() const
{
	return GetRemainingCooldown() > 0.0f;
}

void UAIREAggroSwapComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerController = Cast<APlayerController>(GetOwner());
}

void UAIREAggroSwapComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	OwnerController.Reset();
	Super::EndPlay(EndPlayReason);
}

EAIREAggroSwapResult UAIREAggroSwapComponent::FindActiveOpportunity(
	TWeakObjectPtr<AAIREEnemyBase>& OutEnemy,
	FAIREEnemyAttackSnapshot& OutAttack) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return EAIREAggroSwapResult::NoActiveOpportunity;
	}

	int32 OpportunityCount = 0;
	for (TActorIterator<AAIREEnemyBase> It(World); It; ++It)
	{
		AAIREEnemyBase* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->IsCombatTargetAlive())
		{
			continue;
		}
		UAIREEnemyAttackComponent* AttackComponent =
			Candidate->GetEnemyAttackComponent();
		if (!IsValid(AttackComponent))
		{
			continue;
		}
		const FAIREEnemyAttackSnapshot Snapshot =
			AttackComponent->GetAttackSnapshot();
		if (!Snapshot.bActive
			|| !Snapshot.bOpportunityOpen
			|| Snapshot.bHitCommitted
			|| Snapshot.bDamageCancelled
			|| Snapshot.TargetingMode
				!= EAIRECombatTargetingMode::SingleTarget
			|| !Snapshot.ExecutionId.IsValid())
		{
			continue;
		}
		++OpportunityCount;
		OutEnemy = Candidate;
		OutAttack = Snapshot;
	}
	if (OpportunityCount == 0)
	{
		return EAIREAggroSwapResult::NoActiveOpportunity;
	}
	return OpportunityCount == 1
		? EAIREAggroSwapResult::Applied
		: EAIREAggroSwapResult::AmbiguousOpportunity;
}

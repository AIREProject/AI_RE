#include "AIREEnemyBase.h"

#include "AbilitySystemComponent.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AIREEnemyAIController.h"
#include "AIREEnemyAttackComponent.h"
#include "AIREEnemyConfigDataAsset.h"
#include "AIREEnemyReactionAttributeSet.h"
#include "AIREEnemyReactionComponent.h"
#include "AIREEnemyVitalityComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"

AAIREEnemyBase::AAIREEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;
	AIControllerClass = AAIREEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	EnemyVitalityComponent =
		CreateDefaultSubobject<UAIREEnemyVitalityComponent>(TEXT("EnemyVitality"));
	EnemyReactionComponent =
		CreateDefaultSubobject<UAIREEnemyReactionComponent>(TEXT("EnemyReaction"));
	EnemyAttackComponent =
		CreateDefaultSubobject<UAIREEnemyAttackComponent>(TEXT("EnemyAttack"));
	EnemyReactionAttributeSet =
		CreateDefaultSubobject<UAIREEnemyReactionAttributeSet>(
			TEXT("EnemyReactionAttributes"));
	StimuliSource =
		CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(
			TEXT("EnemyStimuliSource"));
}

EAIRECombatAffiliation AAIREEnemyBase::GetCombatAffiliation() const
{
	return EAIRECombatAffiliation::Enemy;
}

FGameplayAttribute AAIREEnemyBase::GetCombatFlinchAttribute() const
{
	return IsValid(EnemyReactionComponent)
		&& EnemyReactionComponent->IsAcceptingStagger()
		? UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute()
		: FGameplayAttribute();
}

FGameplayAttribute AAIREEnemyBase::GetCombatStunAttribute() const
{
	return IsValid(EnemyReactionComponent)
		&& EnemyReactionComponent->IsAcceptingStagger()
		? UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute()
		: FGameplayAttribute();
}

bool AAIREEnemyBase::IsCombatTargetAlive() const
{
	return IsValid(EnemyVitalityComponent)
		&& !EnemyVitalityComponent->IsDead();
}

void AAIREEnemyBase::NotifyCombatDamageApplied(
	const FAIRECombatDamageRequest& Request)
{
	if (AAIREEnemyAIController* EnemyController =
		Cast<AAIREEnemyAIController>(GetController()))
	{
		EnemyController->ReportCombatDamage(Request.Source.Get(), Request.Damage);
	}
}

bool AAIREEnemyBase::IsHostileThreatFor_Implementation(
	const AActor* Observer) const
{
	if (!IsCombatTargetAlive()
		|| !IsValid(Observer)
		|| Observer == this
		|| !Observer->GetClass()->ImplementsInterface(
			UAIRECombatDamageTargetInterface::StaticClass()))
	{
		return false;
	}
	const IAIRECombatDamageTargetInterface* ObserverCombatant =
		Cast<IAIRECombatDamageTargetInterface>(Observer);
	return ObserverCombatant
		&& ObserverCombatant->GetCombatAffiliation()
			== EAIRECombatAffiliation::PlayerParty;
}

bool AAIREEnemyBase::IsAliveThreatTarget_Implementation() const
{
	return IsCombatTargetAlive();
}

UAIREEnemyVitalityComponent*
AAIREEnemyBase::GetEnemyVitalityComponent() const
{
	return EnemyVitalityComponent;
}

UAIREEnemyReactionComponent*
AAIREEnemyBase::GetEnemyReactionComponent() const
{
	return EnemyReactionComponent;
}

UAIREEnemyAttackComponent* AAIREEnemyBase::GetEnemyAttackComponent() const
{
	return EnemyAttackComponent;
}

const UAIREEnemyConfigDataAsset* AAIREEnemyBase::GetEnemyConfig() const
{
	if (IsValid(EnemyConfig))
	{
		FText ValidationError;
		if (EnemyConfig->IsConfigurationValid(ValidationError))
		{
			return EnemyConfig;
		}
	}
	return GetDefault<UAIREEnemyConfigDataAsset>();
}

void AAIREEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (IsValid(PrimitiveComponent))
		{
			PrimitiveComponent->SetCollisionResponseToChannel(
				ECC_Camera,
				ECR_Ignore);
		}
	}
	ApplyEnemyConfig();
	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	const bool bVitalityInitialized =
		EnemyVitalityComponent->InitializeVitality(AbilitySystemComponent);
	const bool bReactionInitialized =
		EnemyReactionComponent->InitializeReaction(
			AbilitySystemComponent,
			EnemyReactionAttributeSet);
	const bool bAttackInitialized = EnemyAttackComponent->InitializeAttack();
	ensureMsgf(
		bVitalityInitialized && bReactionInitialized && bAttackInitialized,
		TEXT("Enemy combat composition failed to initialize for %s."),
		*GetNameSafe(this));
	EnemyVitalityComponent->OnDeath.AddUniqueDynamic(
		this,
		&AAIREEnemyBase::HandleEnemyDeath);
	EnemyReactionComponent->OnReactionStateChanged.AddUniqueDynamic(
		this,
		&AAIREEnemyBase::HandleReactionStateChanged);
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	if (EnemyVitalityComponent->IsDead())
	{
		HandleEnemyDeath(this);
	}
}

void AAIREEnemyBase::ApplyEnemyConfig()
{
	const UAIREEnemyConfigDataAsset* Config = GetEnemyConfig();
	check(Config);
	GetCharacterMovement()->MaxWalkSpeed = Config->MovementSpeed;
	DeathRemovalDelay = Config->DeathRemovalDelay;
	EnemyVitalityComponent->ConfigureDefaults(
		Config->MaxHealth,
		Config->InitialHealth);
	EnemyReactionComponent->ConfigureThresholds(
		Config->FlinchThreshold,
		Config->FlinchDuration,
		Config->StunThreshold,
		Config->StunDuration);
	EnemyAttackComponent->ConfigureDefaults(
		Config->AttackRange,
		Config->AttackDamage,
		Config->AttackStaggerValue,
		Config->AttackCooldownDuration,
		Config->AttackFallbackHitDelay,
		Config->AttackFallbackRecoveryDuration,
		Config->MeleeTrace);
	EnemyAttackComponent->ConfigureAttackPatterns(Config->AttackPatterns);
}

void AAIREEnemyBase::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathRemovalTimerHandle);
	}
	StimuliSource->UnregisterFromPerceptionSystem();
	EnemyVitalityComponent->OnDeath.RemoveDynamic(
		this,
		&AAIREEnemyBase::HandleEnemyDeath);
	EnemyReactionComponent->OnReactionStateChanged.RemoveDynamic(
		this,
		&AAIREEnemyBase::HandleReactionStateChanged);
	EnemyAttackComponent->ShutdownAttack();
	EnemyReactionComponent->ShutdownReaction();
	EnemyVitalityComponent->ShutdownVitality();
	AbilitySystemComponent->ClearActorInfo();
	Super::EndPlay(EndPlayReason);
}

void AAIREEnemyBase::UnPossessed()
{
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->ClearActorInfo();
	}
	Super::UnPossessed();
}

void AAIREEnemyBase::HandleEnemyDeath(AActor* EnemyActor)
{
	(void)EnemyActor;
	StimuliSource->UnregisterFromPerceptionSystem();
	EnemyReactionComponent->HandleOwnerDeath();
	EnemyAttackComponent->CancelCurrentAttack();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (AAIREEnemyAIController* EnemyController =
		Cast<AAIREEnemyAIController>(GetController()))
	{
		EnemyController->HandleEnemyDeath();
	}
	if (DeathRemovalDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			DeathRemovalTimerHandle,
			this,
			&AAIREEnemyBase::RemoveDeadEnemy,
			DeathRemovalDelay,
			false);
	}
}

void AAIREEnemyBase::HandleReactionStateChanged(
	const EAIREEnemyReactionState PreviousState,
	const EAIREEnemyReactionState CurrentState)
{
	(void)PreviousState;
	if (AAIREEnemyAIController* EnemyController =
		Cast<AAIREEnemyAIController>(GetController()))
	{
		EnemyController->HandleEnemyReactionChanged(CurrentState);
	}
}

void AAIREEnemyBase::RemoveDeadEnemy()
{
	Destroy();
}

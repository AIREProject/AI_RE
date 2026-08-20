#include "Threat/AIRECompanionThreatComponent.h"

#include "Core/AIRECompanionCharacter.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Policy/AIRECompanionLocalBehaviorPolicyComponent.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionThreat, Log, All);

UAIRECompanionThreatComponent::UAIRECompanionThreatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("ThreatSightConfig"));
	check(SightConfig);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->PeripheralVisionAngleDegrees = 180.0f;
	ConfigureSense(*SightConfig);
	SetDominantSense(SightConfig->GetSenseImplementation());
}

void UAIRECompanionThreatComponent::StartThreatDetection(AAIRECompanionCharacter* InCompanionCharacter)
{
	StopThreatDetection();
	if (!IsValid(InCompanionCharacter))
	{
		return;
	}

	const UAIRECompanionConfigDataAsset* CompanionConfig = InCompanionCharacter->GetCompanionConfig();
	UAIRECompanionLocalBehaviorPolicyComponent* PolicyComponent =
		InCompanionCharacter->GetLocalBehaviorPolicyComponent();
	if (!IsValid(CompanionConfig) || !IsValid(PolicyComponent))
	{
		UE_LOG(
			LogAIRECompanionThreat,
			Warning,
			TEXT("Cannot start threat detection for %s without valid configuration and policy."),
			*GetNameSafe(InCompanionCharacter));
		return;
	}

	CompanionCharacter = InCompanionCharacter;
	InCompanionCharacter->SetCombatEquipmentActive(false);
	LocalBehaviorPolicyComponent = PolicyComponent;
	PolicyComponent->OnLocalBehaviorPolicyChanged.AddUniqueDynamic(
		this,
		&UAIRECompanionThreatComponent::HandleLocalBehaviorPolicyChanged);
	ConfigureSight(*CompanionConfig);
	bIsThreatDetectionActive = true;
	SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	SetComponentTickEnabled(true);
	RequestStimuliListenerUpdate();

	UE_LOG(
		LogAIRECompanionThreat,
		Log,
		TEXT("Threat detection started. Companion=%s DetectionDistance=%.2f LoseSightDistance=%.2f SightMemory=%.2f MaxPlayerChaseDistance=%.2f"),
		*GetNameSafe(InCompanionCharacter),
		CompanionConfig->ThreatDetectionDistance,
		CompanionConfig->ThreatDetectionDistance
			+ CompanionConfig->ThreatLoseSightDistance,
		CompanionConfig->ThreatSightMemoryDuration,
		CompanionConfig->MaxChaseDistanceFromPlayer);
}

void UAIRECompanionThreatComponent::StopThreatDetection()
{
	if (CompanionCharacter.IsValid())
	{
		CompanionCharacter->SetCombatEquipmentActive(false);
	}

	if (LocalBehaviorPolicyComponent.IsValid())
	{
		LocalBehaviorPolicyComponent->OnLocalBehaviorPolicyChanged.RemoveDynamic(
			this,
			&UAIRECompanionThreatComponent::HandleLocalBehaviorPolicyChanged);
	}
	LocalBehaviorPolicyComponent.Reset();
	bIsThreatDetectionActive = false;
	SetComponentTickEnabled(false);
	SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	ClearPerceivedHostiles(EAIREThreatCleanupReason::DetectionStopped);
	ForgetAll();
	CompanionCharacter.Reset();
}

AActor* UAIRECompanionThreatComponent::GetSelectedThreatTarget() const
{
	return SelectedThreatTarget.Get();
}

bool UAIRECompanionThreatComponent::IsCombatRequested() const
{
	const FAIRECompanionLocalBehaviorPolicy Policy =
		LocalBehaviorPolicyComponent.IsValid()
			? LocalBehaviorPolicyComponent->GetLocalBehaviorPolicy()
			: FAIRECompanionLocalBehaviorPolicy();
	return bIsThreatDetectionActive
		&& CompanionCharacter.IsValid()
		&& LocalBehaviorPolicyComponent.IsValid()
		&& Policy.EngagementPolicy
			!= EAIRECompanionEngagementPolicy::HoldFire
		&& !CompanionCharacter->IsAbilitySystemDisabled()
		&& SelectedThreatTarget.IsValid();
}

int32 UAIRECompanionThreatComponent::GetPerceivedHostileCount() const
{
	constexpr int32 MaxReportedHostileCount = 32;
	const UWorld* World = GetWorld();
	if (!bIsThreatDetectionActive
		|| !CompanionCharacter.IsValid()
		|| !IsValid(World))
	{
		return 0;
	}
	const double Now = World->GetTimeSeconds();
	int32 HostileCount = 0;
	for (const TWeakObjectPtr<AActor>& Candidate : PerceivedHostiles)
	{
		AActor* CandidateActor = Candidate.Get();
		const double* SightLossDeadline =
			PendingSightLossDeadlines.Find(Candidate);
		const bool bSightMemoryExpired = SightLossDeadline != nullptr
			&& Now >= *SightLossDeadline;
		if (IsValid(CandidateActor)
			&& !CandidateActor->IsActorBeingDestroyed()
			&& !bSightMemoryExpired
			&& IsActorHostile(CandidateActor))
		{
			++HostileCount;
			if (HostileCount >= MaxReportedHostileCount)
			{
				break;
			}
		}
	}

	return HostileCount;
}

void UAIRECompanionThreatComponent::BeginPlay()
{
	Super::BeginPlay();
	OnTargetPerceptionUpdated.AddUniqueDynamic(this, &UAIRECompanionThreatComponent::HandleTargetPerceptionUpdated);
	if (!bIsThreatDetectionActive)
	{
		SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	}
}

void UAIRECompanionThreatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopThreatDetection();
	OnTargetPerceptionUpdated.RemoveDynamic(this, &UAIRECompanionThreatComponent::HandleTargetPerceptionUpdated);
	Super::EndPlay(EndPlayReason);
}

void UAIRECompanionThreatComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshThreatSelection();
}

void UAIRECompanionThreatComponent::HandleTargetPerceptionUpdated(AActor* Actor, const FAIStimulus Stimulus)
{
	if (!bIsThreatDetectionActive || !IsValid(Actor))
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		if (IsActorHostile(Actor))
		{
			PendingSightLossDeadlines.Remove(Actor);
			AddPerceivedHostile(Actor);
		}
		return;
	}

	const UAIRECompanionConfigDataAsset* CompanionConfig =
		CompanionCharacter.IsValid()
			? CompanionCharacter->GetCompanionConfig()
			: nullptr;
	const UWorld* World = GetWorld();
	if (!PerceivedHostiles.Contains(Actor)
		|| !IsValid(CompanionConfig)
		|| CompanionConfig->ThreatSightMemoryDuration <= 0.0f
		|| !IsValid(World))
	{
		RemovePerceivedHostile(Actor, EAIREThreatCleanupReason::PerceptionLost);
		return;
	}

	PendingSightLossDeadlines.FindOrAdd(Actor) =
		World->GetTimeSeconds()
		+ CompanionConfig->ThreatSightMemoryDuration;
}

void UAIRECompanionThreatComponent::HandlePerceivedActorDestroyed(AActor* DestroyedActor)
{
	RemovePerceivedHostile(DestroyedActor, EAIREThreatCleanupReason::TargetInvalid);
}

void UAIRECompanionThreatComponent::HandleLocalBehaviorPolicyChanged(
	const FAIRECompanionLocalBehaviorPolicy PreviousPolicy,
	const FAIRECompanionLocalBehaviorPolicy CurrentPolicy)
{
	(void)PreviousPolicy;
	(void)CurrentPolicy;
	RefreshThreatSelection();
}

void UAIRECompanionThreatComponent::ConfigureSight(const UAIRECompanionConfigDataAsset& CompanionConfig)
{
	SightConfig->SightRadius = CompanionConfig.ThreatDetectionDistance;
	SightConfig->LoseSightRadius = CompanionConfig.ThreatDetectionDistance
		+ CompanionConfig.ThreatLoseSightDistance;
	ConfigureSense(*SightConfig);
}

void UAIRECompanionThreatComponent::RefreshThreatSelection()
{
	if (!bIsThreatDetectionActive || !CompanionCharacter.IsValid())
	{
		ClearSelectedTarget(EAIREThreatCleanupReason::DetectionStopped);
		return;
	}

	if (CompanionCharacter->IsAbilitySystemDisabled())
	{
		ClearSelectedTarget(EAIREThreatCleanupReason::CompanionDisabled);
		return;
	}

	const UAIRECompanionConfigDataAsset* CompanionConfig = CompanionCharacter->GetCompanionConfig();
	if (!IsValid(CompanionConfig) || !LocalBehaviorPolicyComponent.IsValid())
	{
		ClearSelectedTarget(EAIREThreatCleanupReason::PolicyUnavailable);
		return;
	}

	const UWorld* World = CompanionCharacter->GetWorld();
	const double Now = IsValid(World) ? World->GetTimeSeconds() : 0.0;
	for (int32 Index = PerceivedHostiles.Num() - 1; Index >= 0; --Index)
	{
		const TWeakObjectPtr<AActor> Candidate = PerceivedHostiles[Index];
		AActor* Actor = Candidate.Get();
		if (!IsValid(Actor))
		{
			const bool bWasSelectedTarget = SelectedThreatTarget == Candidate;
			PendingSightLossDeadlines.Remove(Candidate);
			PerceivedHostiles.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			if (bWasSelectedTarget)
			{
				ClearSelectedTarget(EAIREThreatCleanupReason::TargetInvalid);
			}
			continue;
		}

		const double* SightLossDeadline =
			PendingSightLossDeadlines.Find(Candidate);
		const bool bSightMemoryExpired = SightLossDeadline != nullptr
			&& (!IsValid(World) || Now >= *SightLossDeadline);
		if (IsActorHostile(Actor) && !bSightMemoryExpired)
		{
			continue;
		}

		RemovePerceivedHostile(
			Actor,
			bSightMemoryExpired
				? EAIREThreatCleanupReason::PerceptionLost
				: EAIREThreatCleanupReason::NotHostile);
	}

	const FAIRECompanionLocalBehaviorPolicy Policy =
		LocalBehaviorPolicyComponent->GetLocalBehaviorPolicy();
	if (!Policy.IsValid())
	{
		ClearSelectedTarget(EAIREThreatCleanupReason::PolicyUnavailable);
		return;
	}
	if (Policy.EngagementPolicy == EAIRECompanionEngagementPolicy::HoldFire)
	{
		ClearSelectedTarget(EAIREThreatCleanupReason::HoldFire);
		return;
	}

	APawn* PlayerPawn =
		IsValid(World) ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!IsValid(PlayerPawn))
	{
		ClearSelectedTarget(EAIREThreatCleanupReason::NoPlayer);
		return;
	}

	if (SelectedThreatTarget.IsValid())
	{
		EAIREThreatCleanupReason FailureReason = EAIREThreatCleanupReason::None;
		if (IsActorEligible(
			SelectedThreatTarget.Get(),
			PlayerPawn,
			*CompanionConfig,
			Policy.EngagementPolicy,
			FailureReason))
		{
			return;
		}

		ClearSelectedTarget(FailureReason);
	}

	SelectClosestEligibleTarget(
		*PlayerPawn,
		*CompanionConfig,
		Policy.EngagementPolicy);
}

bool UAIRECompanionThreatComponent::IsActorHostile(AActor* Actor) const
{
	if (!IsValid(Actor) || Actor == CompanionCharacter.Get()
		|| !Actor->GetClass()->ImplementsInterface(UAIREThreatTargetInterface::StaticClass()))
	{
		return false;
	}

	return IAIREThreatTargetInterface::Execute_IsAliveThreatTarget(Actor)
		&& IAIREThreatTargetInterface::Execute_IsHostileThreatFor(
			Actor,
			CompanionCharacter.Get());
}

bool UAIRECompanionThreatComponent::IsActorEligible(
	AActor* Actor,
	const APawn* PlayerPawn,
	const UAIRECompanionConfigDataAsset& CompanionConfig,
	const EAIRECompanionEngagementPolicy EngagementPolicy,
	EAIREThreatCleanupReason& OutFailureReason) const
{
	OutFailureReason = EAIREThreatCleanupReason::None;
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
	{
		OutFailureReason = EAIREThreatCleanupReason::TargetInvalid;
		return false;
	}
	if (!IsActorHostile(Actor))
	{
		OutFailureReason = EAIREThreatCleanupReason::NotHostile;
		return false;
	}
	if (!IsValid(PlayerPawn))
	{
		OutFailureReason = EAIREThreatCleanupReason::NoPlayer;
		return false;
	}
	if (EngagementPolicy == EAIRECompanionEngagementPolicy::HoldFire)
	{
		OutFailureReason = EAIREThreatCleanupReason::HoldFire;
		return false;
	}

	const AAIRECompanionCharacter* Companion = CompanionCharacter.Get();
	if (!IsValid(Companion))
	{
		OutFailureReason = EAIREThreatCleanupReason::DetectionStopped;
		return false;
	}

	const float DistanceToCompanionSquared = FVector::DistSquared(
		Actor->GetActorLocation(),
		Companion->GetActorLocation());
	const float EligibleDetectionDistance =
		CompanionConfig.ThreatDetectionDistance
		+ (Actor == SelectedThreatTarget.Get()
			? CompanionConfig.ThreatLoseSightDistance
			: 0.0f);
	if (DistanceToCompanionSquared > FMath::Square(EligibleDetectionDistance))
	{
		OutFailureReason = EAIREThreatCleanupReason::OutsideDetectionDistance;
		return false;
	}

	const float DistanceToPlayerSquared = FVector::DistSquared(
		Actor->GetActorLocation(),
		PlayerPawn->GetActorLocation());
	if (EngagementPolicy == EAIRECompanionEngagementPolicy::DefendPlayer
		&& DistanceToPlayerSquared
			> FMath::Square(CompanionConfig.DefendPlayerRadius))
	{
		OutFailureReason = EAIREThreatCleanupReason::OutsideDefendPlayerRadius;
		return false;
	}
	if (EngagementPolicy == EAIRECompanionEngagementPolicy::Aggressive
		&& DistanceToPlayerSquared
			> FMath::Square(CompanionConfig.MaxChaseDistanceFromPlayer))
	{
		OutFailureReason = EAIREThreatCleanupReason::OutsidePlayerChaseDistance;
		return false;
	}

	return true;
}

void UAIRECompanionThreatComponent::AddPerceivedHostile(AActor* Actor)
{
	if (!IsValid(Actor) || PerceivedHostiles.Contains(Actor))
	{
		return;
	}

	PerceivedHostiles.Add(Actor);
	Actor->OnDestroyed.AddUniqueDynamic(this, &UAIRECompanionThreatComponent::HandlePerceivedActorDestroyed);
}

void UAIRECompanionThreatComponent::RemovePerceivedHostile(
	AActor* Actor,
	const EAIREThreatCleanupReason Reason)
{
	if (IsValid(Actor))
	{
		Actor->OnDestroyed.RemoveDynamic(this, &UAIRECompanionThreatComponent::HandlePerceivedActorDestroyed);
	}
	PendingSightLossDeadlines.Remove(Actor);
	PerceivedHostiles.Remove(Actor);

	if (SelectedThreatTarget.Get() == Actor || (!IsValid(Actor) && !SelectedThreatTarget.IsValid()))
	{
		ClearSelectedTarget(Reason);
	}
}

void UAIRECompanionThreatComponent::SelectClosestEligibleTarget(
	const APawn& PlayerPawn,
	const UAIRECompanionConfigDataAsset& CompanionConfig,
	const EAIRECompanionEngagementPolicy EngagementPolicy)
{
	const AAIRECompanionCharacter* Companion = CompanionCharacter.Get();
	if (!IsValid(Companion))
	{
		return;
	}

	AActor* ClosestTarget = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AActor>& Candidate : PerceivedHostiles)
	{
		AActor* CandidateActor = Candidate.Get();
		EAIREThreatCleanupReason FailureReason = EAIREThreatCleanupReason::None;
		if (!IsActorEligible(
			CandidateActor,
			&PlayerPawn,
			CompanionConfig,
			EngagementPolicy,
			FailureReason))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			CandidateActor->GetActorLocation(),
			Companion->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestTarget = CandidateActor;
			ClosestDistanceSquared = DistanceSquared;
		}
	}

	if (!IsValid(ClosestTarget))
	{
		return;
	}

	SelectedThreatTarget = ClosestTarget;
	CompanionCharacter->SetCombatEquipmentActive(true);
	UE_LOG(
		LogAIRECompanionThreat,
		Log,
		TEXT("Threat target selected. Companion=%s Target=%s CombatRequested=true"),
		*GetNameSafe(Companion),
		*GetNameSafe(ClosestTarget));
}

void UAIRECompanionThreatComponent::ClearSelectedTarget(const EAIREThreatCleanupReason Reason)
{
	const bool bHadSelectedTarget = SelectedThreatTarget.IsValid() || SelectedThreatTarget.IsStale();
	AActor* PreviousTarget = SelectedThreatTarget.Get();
	if (!bHadSelectedTarget)
	{
		SelectedThreatTarget.Reset();
		return;
	}

	SelectedThreatTarget.Reset();
	if (CompanionCharacter.IsValid())
	{
		CompanionCharacter->SetCombatEquipmentActive(false);
	}
	UE_LOG(
		LogAIRECompanionThreat,
		Log,
		TEXT("Threat target cleared. Companion=%s Target=%s CombatRequested=false Reason=%s"),
		*GetNameSafe(CompanionCharacter.Get()),
		*GetNameSafe(PreviousTarget),
		GetCleanupReasonName(Reason));
}

void UAIRECompanionThreatComponent::ClearPerceivedHostiles(const EAIREThreatCleanupReason Reason)
{
	ClearSelectedTarget(Reason);
	for (const TWeakObjectPtr<AActor>& Candidate : PerceivedHostiles)
	{
		if (AActor* CandidateActor = Candidate.Get(); IsValid(CandidateActor))
		{
			CandidateActor->OnDestroyed.RemoveDynamic(
				this,
				&UAIRECompanionThreatComponent::HandlePerceivedActorDestroyed);
		}
	}
	PerceivedHostiles.Reset();
	PendingSightLossDeadlines.Reset();
}

const TCHAR* UAIRECompanionThreatComponent::GetCleanupReasonName(const EAIREThreatCleanupReason Reason)
{
	switch (Reason)
	{
	case EAIREThreatCleanupReason::PerceptionLost:
		return TEXT("PerceptionLost");
	case EAIREThreatCleanupReason::NotHostile:
		return TEXT("NotHostile");
	case EAIREThreatCleanupReason::TargetInvalid:
		return TEXT("TargetInvalid");
	case EAIREThreatCleanupReason::NoPlayer:
		return TEXT("NoPlayer");
	case EAIREThreatCleanupReason::OutsideDetectionDistance:
		return TEXT("OutsideDetectionDistance");
	case EAIREThreatCleanupReason::OutsideDefendPlayerRadius:
		return TEXT("OutsideDefendPlayerRadius");
	case EAIREThreatCleanupReason::OutsidePlayerChaseDistance:
		return TEXT("OutsidePlayerChaseDistance");
	case EAIREThreatCleanupReason::HoldFire:
		return TEXT("HoldFire");
	case EAIREThreatCleanupReason::PolicyUnavailable:
		return TEXT("PolicyUnavailable");
	case EAIREThreatCleanupReason::CompanionDisabled:
		return TEXT("CompanionDisabled");
	case EAIREThreatCleanupReason::DetectionStopped:
		return TEXT("DetectionStopped");
	case EAIREThreatCleanupReason::None:
	default:
		return TEXT("None");
	}
}

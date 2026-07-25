#include "AIRECompanionThreatComponent.h"

#include "AIRECompanionCharacter.h"
#include "AIRECompanionConfigDataAsset.h"
#include "AIREThreatTargetInterface.h"
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
	if (!IsValid(CompanionConfig))
	{
		UE_LOG(
			LogAIRECompanionThreat,
			Warning,
			TEXT("Cannot start threat detection for %s without valid configuration."),
			*GetNameSafe(InCompanionCharacter));
		return;
	}

	CompanionCharacter = InCompanionCharacter;
	ConfigureSight(*CompanionConfig);
	bIsThreatDetectionActive = true;
	SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	SetComponentTickEnabled(true);
	RequestStimuliListenerUpdate();

	UE_LOG(
		LogAIRECompanionThreat,
		Log,
		TEXT("Threat detection started. Companion=%s DetectionDistance=%.2f MaxPlayerChaseDistance=%.2f"),
		*GetNameSafe(InCompanionCharacter),
		CompanionConfig->ThreatDetectionDistance,
		CompanionConfig->MaxChaseDistanceFromPlayer);
}

void UAIRECompanionThreatComponent::StopThreatDetection()
{
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
	return bIsThreatDetectionActive
		&& CompanionCharacter.IsValid()
		&& !CompanionCharacter->IsAbilitySystemDisabled()
		&& SelectedThreatTarget.IsValid();
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
			AddPerceivedHostile(Actor);
		}
		return;
	}

	RemovePerceivedHostile(Actor, EAIREThreatCleanupReason::PerceptionLost);
}

void UAIRECompanionThreatComponent::HandlePerceivedActorDestroyed(AActor* DestroyedActor)
{
	RemovePerceivedHostile(DestroyedActor, EAIREThreatCleanupReason::TargetInvalid);
}

void UAIRECompanionThreatComponent::ConfigureSight(const UAIRECompanionConfigDataAsset& CompanionConfig)
{
	SightConfig->SightRadius = CompanionConfig.ThreatDetectionDistance;
	SightConfig->LoseSightRadius = CompanionConfig.ThreatDetectionDistance;
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
	const UWorld* World = CompanionCharacter->GetWorld();
	APawn* PlayerPawn = IsValid(World) ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!IsValid(CompanionConfig) || !IsValid(PlayerPawn))
	{
		ClearSelectedTarget(EAIREThreatCleanupReason::NoPlayer);
		return;
	}

	for (int32 Index = PerceivedHostiles.Num() - 1; Index >= 0; --Index)
	{
		AActor* Actor = PerceivedHostiles[Index].Get();
		if (IsValid(Actor) && IsActorHostile(Actor))
		{
			continue;
		}

		RemovePerceivedHostile(
			Actor,
			IsValid(Actor) ? EAIREThreatCleanupReason::NotHostile : EAIREThreatCleanupReason::TargetInvalid);
	}

	if (SelectedThreatTarget.IsValid())
	{
		EAIREThreatCleanupReason FailureReason = EAIREThreatCleanupReason::None;
		if (IsActorEligible(SelectedThreatTarget.Get(), PlayerPawn, *CompanionConfig, FailureReason))
		{
			return;
		}

		ClearSelectedTarget(FailureReason);
	}

	SelectClosestEligibleTarget(*PlayerPawn, *CompanionConfig);
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

	const AAIRECompanionCharacter* Companion = CompanionCharacter.Get();
	if (!IsValid(Companion))
	{
		OutFailureReason = EAIREThreatCleanupReason::DetectionStopped;
		return false;
	}

	const float DistanceToCompanionSquared = FVector::DistSquared(
		Actor->GetActorLocation(),
		Companion->GetActorLocation());
	if (DistanceToCompanionSquared > FMath::Square(CompanionConfig.ThreatDetectionDistance))
	{
		OutFailureReason = EAIREThreatCleanupReason::OutsideDetectionDistance;
		return false;
	}

	const float DistanceToPlayerSquared = FVector::DistSquared(
		Actor->GetActorLocation(),
		PlayerPawn->GetActorLocation());
	if (DistanceToPlayerSquared > FMath::Square(CompanionConfig.MaxChaseDistanceFromPlayer))
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
	PerceivedHostiles.Remove(Actor);

	if (SelectedThreatTarget.Get() == Actor || (!IsValid(Actor) && !SelectedThreatTarget.IsValid()))
	{
		ClearSelectedTarget(Reason);
	}
}

void UAIRECompanionThreatComponent::SelectClosestEligibleTarget(
	const APawn& PlayerPawn,
	const UAIRECompanionConfigDataAsset& CompanionConfig)
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
		if (!IsActorEligible(CandidateActor, &PlayerPawn, CompanionConfig, FailureReason))
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
	case EAIREThreatCleanupReason::OutsidePlayerChaseDistance:
		return TEXT("OutsidePlayerChaseDistance");
	case EAIREThreatCleanupReason::CompanionDisabled:
		return TEXT("CompanionDisabled");
	case EAIREThreatCleanupReason::DetectionStopped:
		return TEXT("DetectionStopped");
	case EAIREThreatCleanupReason::None:
	default:
		return TEXT("None");
	}
}

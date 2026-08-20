#include "AIREEnemyAggroComponent.h"

#include "AIRECombatDamageTargetInterface.h"
#include "AIREEnemyBase.h"
#include "Engine/World.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

#if !UE_BUILD_SHIPPING
DEFINE_LOG_CATEGORY_STATIC(LogAIREEnemyAggro, Log, All);
#endif

UAIREEnemyAggroComponent::UAIREEnemyAggroComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	check(SightConfig);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SetDominantSense(UAISense_Sight::StaticClass());
}

bool UAIREEnemyAggroComponent::StartAggroTracking(
	AAIREEnemyBase* InEnemy)
{
	StopAggroTracking();
	if (!IsValid(InEnemy))
	{
		return false;
	}
	Enemy = InEnemy;
	ConfigureSight();
	bTracking = true;
	SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	RequestStimuliListenerUpdate();
	return true;
}

void UAIREEnemyAggroComponent::StopAggroTracking()
{
	bTracking = false;
	SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	ResetAggro();
	ForgetAll();
	Enemy.Reset();
}

void UAIREEnemyAggroComponent::RefreshSelection()
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		AActor* Actor = Entries[Index].Actor.Get();
		if (IsEligiblePartyParticipant(Actor))
		{
			continue;
		}
		if (IsValid(Actor))
		{
			Actor->OnDestroyed.RemoveDynamic(
				this,
				&UAIREEnemyAggroComponent::HandleCandidateDestroyed);
		}
		if (SelectedTarget.Get() == Actor || !SelectedTarget.IsValid())
		{
			SelectTarget(nullptr);
		}
		Entries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}
	FAggroEntry* HighestEntry = nullptr;
	for (FAggroEntry& Entry : Entries)
	{
		if (!HighestEntry || Entry.Threat > HighestEntry->Threat)
		{
			HighestEntry = &Entry;
		}
	}
	if (!HighestEntry)
	{
		SelectTarget(nullptr);
		return;
	}

	FAggroEntry* CurrentEntry = FindEntry(SelectedTarget.Get());
	if (!CurrentEntry)
	{
		SelectTarget(HighestEntry->Actor.Get());
		return;
	}
	if (HighestEntry != CurrentEntry
		&& !HasRecentSightEvidence(*CurrentEntry)
		&& HighestEntry->Threat >= CurrentEntry->Threat + RetargetMargin)
	{
		SelectTarget(HighestEntry->Actor.Get());
	}
}

void UAIREEnemyAggroComponent::ReportDamage(
	AActor* Attacker,
	const float Damage)
{
	if (!bTracking
		|| !IsEligiblePartyParticipant(Attacker)
		|| !FMath::IsFinite(Damage)
		|| Damage < 0.0f)
	{
		return;
	}
	FAggroEntry& Entry = FindOrAddEntry(Attacker);
	Entry.Threat += Damage * DamageThreatMultiplier;
	Entry.bHasDamageEvidence = true;
	Entry.LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	Entry.LastKnownLocation = Attacker->GetActorLocation();
	RefreshSelection();
}

bool UAIREEnemyAggroComponent::PromoteTargetAboveCurrentMaximum(
	AActor* Target)
{
	if (!bTracking || !IsEligiblePartyParticipant(Target))
	{
		return false;
	}
	float MaximumThreat = 0.0f;
	for (const FAggroEntry& Entry : Entries)
	{
		MaximumThreat = FMath::Max(MaximumThreat, Entry.Threat);
	}
	FAggroEntry& Entry = FindOrAddEntry(Target);
	Entry.Threat = MaximumThreat + SwapLeadMargin;
	Entry.LastKnownLocation = Target->GetActorLocation();
	SelectTarget(Target);
	return SelectedTarget.Get() == Target;
}

void UAIREEnemyAggroComponent::ResetAggro()
{
	SelectTarget(nullptr);
	for (const FAggroEntry& Entry : Entries)
	{
		if (AActor* Actor = Entry.Actor.Get(); IsValid(Actor))
		{
			Actor->OnDestroyed.RemoveDynamic(
				this,
				&UAIREEnemyAggroComponent::HandleCandidateDestroyed);
		}
	}
	Entries.Reset();
}

AActor* UAIREEnemyAggroComponent::GetSelectedTarget() const
{
	return SelectedTarget.Get();
}

FAIREEnemyAggroSnapshot UAIREEnemyAggroComponent::GetAggroSnapshot() const
{
	FAIREEnemyAggroSnapshot Snapshot;
	Snapshot.SelectedTarget = SelectedTarget.Get();
	Snapshot.TargetRevision = TargetRevision;
	for (const FAggroEntry& Entry : Entries)
	{
		FAIREEnemyAggroEntrySnapshot& EntrySnapshot =
			Snapshot.Entries.AddDefaulted_GetRef();
		EntrySnapshot.Actor = Entry.Actor.Get();
		EntrySnapshot.Threat = Entry.Threat;
		EntrySnapshot.bVisible = Entry.bVisible;
		EntrySnapshot.bHasDamageEvidence = Entry.bHasDamageEvidence;
		EntrySnapshot.LastKnownLocation = Entry.LastKnownLocation;
	}
	return Snapshot;
}

bool UAIREEnemyAggroComponent::IsSelectedTargetVisible() const
{
	const FAggroEntry* Entry = FindEntry(SelectedTarget.Get());
	return Entry && Entry->bVisible;
}

bool UAIREEnemyAggroComponent::SelectedTargetHasRecentSightEvidence() const
{
	const FAggroEntry* Entry = FindEntry(SelectedTarget.Get());
	return Entry && HasRecentSightEvidence(*Entry);
}

bool UAIREEnemyAggroComponent::SelectedTargetHasRecentDamageEvidence() const
{
	const FAggroEntry* Entry = FindEntry(SelectedTarget.Get());
	const UWorld* World = GetWorld();
	return Entry
		&& Entry->bHasDamageEvidence
		&& IsValid(World)
		&& Entry->LastDamageTime >= 0.0
		&& World->GetTimeSeconds() - Entry->LastDamageTime
			<= DamageEvidenceDuration;
}

FVector UAIREEnemyAggroComponent::GetSelectedTargetLastKnownLocation() const
{
	const FAggroEntry* Entry = FindEntry(SelectedTarget.Get());
	return Entry ? Entry->LastKnownLocation : FVector::ZeroVector;
}

void UAIREEnemyAggroComponent::BeginPlay()
{
	Super::BeginPlay();
	OnTargetPerceptionUpdated.AddUniqueDynamic(
		this,
		&UAIREEnemyAggroComponent::HandleTargetPerceptionUpdated);
	if (!bTracking)
	{
		SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	}
}

void UAIREEnemyAggroComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopAggroTracking();
	OnTargetPerceptionUpdated.RemoveDynamic(
		this,
		&UAIREEnemyAggroComponent::HandleTargetPerceptionUpdated);
	Super::EndPlay(EndPlayReason);
}

void UAIREEnemyAggroComponent::HandleTargetPerceptionUpdated(
	AActor* Actor,
	const FAIStimulus Stimulus)
{
	if (!bTracking || !IsEligiblePartyParticipant(Actor))
	{
		return;
	}
	FAggroEntry& Entry = FindOrAddEntry(Actor);
	const bool bWasVisible = Entry.bVisible;
	Entry.bVisible = Stimulus.WasSuccessfullySensed();
	if (Entry.bVisible || bWasVisible)
	{
		Entry.LastSightTime = GetWorld()
			? GetWorld()->GetTimeSeconds()
			: -1.0;
	}
	if (Entry.bVisible)
	{
		Entry.Threat = FMath::Max(Entry.Threat, InitialSightThreat);
		Entry.LastKnownLocation = Actor->GetActorLocation();
	}
	else if (!Stimulus.StimulusLocation.ContainsNaN())
	{
		Entry.LastKnownLocation = Stimulus.StimulusLocation;
	}
	RefreshSelection();
#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogAIREEnemyAggro,
		Log,
		TEXT("Perception update Candidate=%s SelectedTarget=%s Sensed=%d StimulusAge=%.3f StimulusLocation=%s TargetRevision=%lld"),
		*GetNameSafe(Actor),
		*GetNameSafe(SelectedTarget.Get()),
		Stimulus.WasSuccessfullySensed(),
		Stimulus.GetAge(),
		*Stimulus.StimulusLocation.ToCompactString(),
		TargetRevision);
#endif
}

void UAIREEnemyAggroComponent::HandleCandidateDestroyed(
	AActor* DestroyedActor)
{
	RemoveEntry(DestroyedActor);
	RefreshSelection();
}

bool UAIREEnemyAggroComponent::IsEligiblePartyParticipant(AActor* Actor) const
{
	if (!IsValid(Actor)
		|| Actor->IsActorBeingDestroyed()
		|| Actor == Enemy.Get()
		|| !Actor->GetClass()->ImplementsInterface(
			UAIRECombatDamageTargetInterface::StaticClass()))
	{
		return false;
	}
	const IAIRECombatDamageTargetInterface* Combatant =
		Cast<IAIRECombatDamageTargetInterface>(Actor);
	return Combatant
		&& Combatant->GetCombatAffiliation()
			== EAIRECombatAffiliation::PlayerParty
		&& Combatant->IsCombatTargetAlive();
}

UAIREEnemyAggroComponent::FAggroEntry*
UAIREEnemyAggroComponent::FindEntry(AActor* Actor)
{
	return Entries.FindByPredicate(
		[Actor](const FAggroEntry& Entry)
		{
			return Entry.Actor.Get() == Actor;
		});
}

const UAIREEnemyAggroComponent::FAggroEntry*
UAIREEnemyAggroComponent::FindEntry(const AActor* Actor) const
{
	return Entries.FindByPredicate(
		[Actor](const FAggroEntry& Entry)
		{
			return Entry.Actor.Get() == Actor;
		});
}

UAIREEnemyAggroComponent::FAggroEntry&
UAIREEnemyAggroComponent::FindOrAddEntry(AActor* Actor)
{
	if (FAggroEntry* Existing = FindEntry(Actor))
	{
		return *Existing;
	}
	FAggroEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Actor = Actor;
	Entry.LastKnownLocation = Actor->GetActorLocation();
	Actor->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIREEnemyAggroComponent::HandleCandidateDestroyed);
	return Entry;
}

void UAIREEnemyAggroComponent::RemoveEntry(AActor* Actor)
{
	if (IsValid(Actor))
	{
		Actor->OnDestroyed.RemoveDynamic(
			this,
			&UAIREEnemyAggroComponent::HandleCandidateDestroyed);
	}
	Entries.RemoveAll(
		[Actor](const FAggroEntry& Entry)
		{
			return Entry.Actor.Get() == Actor || !Entry.Actor.IsValid();
		});
	if (SelectedTarget.Get() == Actor || !SelectedTarget.IsValid())
	{
		SelectTarget(nullptr);
	}
}

void UAIREEnemyAggroComponent::SelectTarget(AActor* Target)
{
	if (SelectedTarget.Get() == Target)
	{
		return;
	}
	SelectedTarget = Target;
	++TargetRevision;
}

bool UAIREEnemyAggroComponent::HasRecentSightEvidence(
	const FAggroEntry& Entry) const
{
	if (Entry.bVisible)
	{
		return true;
	}
	const UWorld* World = GetWorld();
	return IsValid(World)
		&& Entry.LastSightTime >= 0.0
		&& World->GetTimeSeconds() - Entry.LastSightTime
			<= SightEvidenceDuration;
}

void UAIREEnemyAggroComponent::ConfigureSight()
{
	check(SightConfig);
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(SightRadius, LoseSightRadius);
	SightConfig->PeripheralVisionAngleDegrees = 75.0f;
	SightConfig->SetMaxAge(3.0f);
	ConfigureSense(*SightConfig);
}

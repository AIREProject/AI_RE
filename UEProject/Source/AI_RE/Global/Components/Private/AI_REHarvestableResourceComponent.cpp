#include "AI_REHarvestableResourceComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AI_REItemDataAsset.h"

UAI_REHarvestableResourceComponent::UAI_REHarvestableResourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// SetIsReplicatedByDefault(true); // Removed for Singleplayer
}

void UAI_REHarvestableResourceComponent::BeginPlay()
{
	Super::BeginPlay();

	// Removed Authority check for singleplayer
	CurrentHealth = MaxHealth;
	RewardDamageProgress = 0.f;
}

bool UAI_REHarvestableResourceComponent::ApplyHarvestDamage(float DamageAmount, AActor* InstigatorActor)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr
		|| bIsDepleted
		|| !FMath::IsFinite(DamageAmount)
		|| DamageAmount <= 0.0f)
	{
		return false;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	const int32 RewardMultiplier = ConsumeRewardIntervals(AppliedDamage);
	const int32 GrantedRewardAmount = RewardMultiplier * RewardAmount;
	const FGuid DeliveryId = GrantedRewardAmount > 0
		? FGuid::NewGuid()
		: FGuid();

	OnHarvested.Broadcast(
		InstigatorActor,
		AppliedDamage,
		CurrentHealth,
		RewardItemAsset,
		GrantedRewardAmount,
		DeliveryId);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Harvested %s: Damage=%.2f CurrentHealth=%.2f Reward=%s x%d"),
		*Owner->GetName(),
		AppliedDamage,
		CurrentHealth,
		RewardItemAsset ? *RewardItemAsset->ItemId.ToString() : TEXT("None"),
		GrantedRewardAmount);

	if (CurrentHealth <= 0.0f)
	{
		DepleteResource(InstigatorActor);
	}

	return true;
}

void UAI_REHarvestableResourceComponent::SetResourceDefaults(
	FGameplayTag InRequiredWorkTag,
	UAI_REItemDataAsset* InRewardItemAsset,
	int32 InRewardAmount,
	float InRewardDamageInterval)
{
	RequiredWorkTag = InRequiredWorkTag;
	RewardItemAsset = InRewardItemAsset;
	RewardAmount = FMath::Max(0, InRewardAmount);
	RewardDamageInterval = FMath::Max(0.f, InRewardDamageInterval);
}

void UAI_REHarvestableResourceComponent::DepleteResource(AActor* InstigatorActor)
{
	if (bIsDepleted)
	{
		return;
	}

	bIsDepleted = true;
	CurrentHealth = 0.0f;

	BroadcastDepletedState();
	OnDepleted.Broadcast(InstigatorActor);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
		World->GetTimerManager().SetTimer(
			RespawnTimerHandle,
			this,
			&UAI_REHarvestableResourceComponent::RespawnResource,
			RespawnDelay,
			false);
	}
}

void UAI_REHarvestableResourceComponent::RespawnResource()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	bIsDepleted = false;
	CurrentHealth = MaxHealth;
	RewardDamageProgress = 0.f;

	BroadcastDepletedState();
	OnRespawned.Broadcast();
}

void UAI_REHarvestableResourceComponent::BroadcastDepletedState()
{
	OnDepletedStateChanged.Broadcast(bIsDepleted);
}

int32 UAI_REHarvestableResourceComponent::ConsumeRewardIntervals(float AppliedDamage)
{
	if (AppliedDamage <= 0.f || RewardAmount <= 0 || !RewardItemAsset)
	{
		return 0;
	}

	if (RewardDamageInterval <= 0.f)
	{
		return 1;
	}

	RewardDamageProgress += AppliedDamage;
	const int32 RewardMultiplier = FMath::FloorToInt(RewardDamageProgress / RewardDamageInterval);
	if (RewardMultiplier > 0)
	{
		RewardDamageProgress = FMath::Fmod(RewardDamageProgress, RewardDamageInterval);
	}

	return RewardMultiplier;
}

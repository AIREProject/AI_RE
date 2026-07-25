#include "AI_REHarvestableResourceComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	if (Owner == nullptr || bIsDepleted || DamageAmount <= 0.0f)
	{
		return false;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	const int32 RewardMultiplier = ConsumeRewardIntervals(AppliedDamage);
	const int32 GrantedRewardAmount = RewardMultiplier * RewardAmount;

	GrantReward(InstigatorActor, RewardMultiplier);
	OnHarvested.Broadcast(InstigatorActor, AppliedDamage, CurrentHealth, RewardName, GrantedRewardAmount);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Harvested %s: Damage=%.2f CurrentHealth=%.2f Reward=%s x%d"),
		*Owner->GetName(),
		AppliedDamage,
		CurrentHealth,
		*RewardName.ToString(),
		GrantedRewardAmount);

	if (CurrentHealth <= 0.0f)
	{
		DepleteResource(InstigatorActor);
	}

	return true;
}

void UAI_REHarvestableResourceComponent::SetResourceDefaults(
	FGameplayTag InRequiredWorkTag,
	FName InRewardName,
	int32 InRewardAmount,
	float InRewardDamageInterval)
{
	RequiredWorkTag = InRequiredWorkTag;
	RewardName = InRewardName;
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
	if (AppliedDamage <= 0.f || RewardAmount <= 0 || RewardName.IsNone())
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

void UAI_REHarvestableResourceComponent::GrantReward(AActor* InstigatorActor, int32 RewardMultiplier)
{
	const int32 GrantedRewardAmount = RewardMultiplier * RewardAmount;
	if (!InstigatorActor || GrantedRewardAmount <= 0 || RewardName.IsNone())
	{
		return;
	}

	// TODO: Inventory System Integration
	// In singleplayer AI_RE project, we will later link this to the Grid Inventory System.
	// For now, we simply log the reward grant.
	UE_LOG(LogTemp, Warning, TEXT("GrantReward (Singleplayer Stub): %s x%d granted to %s. Implement Inventory Link!"), 
		*RewardName.ToString(), GrantedRewardAmount, *InstigatorActor->GetName());
}

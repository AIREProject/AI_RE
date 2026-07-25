#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AI_REHarvestableResourceComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FAI_REHarvestedSignature,
	AActor*, InstigatorActor,
	float, DamageAmount,
	float, CurrentHealth,
	FName, RewardName,
	int32, RewardAmount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAI_REHarvestDepletedSignature, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAI_REHarvestRespawnedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAI_REHarvestDepletedStateChangedSignature, bool, bNewIsDepleted);

UCLASS(ClassGroup = (AI_RE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAI_REHarvestableResourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAI_REHarvestableResourceComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Harvest")
	bool ApplyHarvestDamage(float DamageAmount, AActor* InstigatorActor);

	UFUNCTION(BlueprintPure, Category = "AI_RE|Harvest")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "AI_RE|Harvest")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "AI_RE|Harvest")
	bool IsDepleted() const { return bIsDepleted; }

	UFUNCTION(BlueprintPure, Category = "AI_RE|Harvest")
	FGameplayTag GetRequiredWorkTag() const { return RequiredWorkTag; }

	UFUNCTION(BlueprintPure, Category = "AI_RE|Harvest")
	FName GetRewardName() const { return RewardName; }

	UFUNCTION(BlueprintPure, Category = "AI_RE|Harvest")
	int32 GetRewardAmount() const { return RewardAmount; }

	UFUNCTION(BlueprintPure, Category = "AI_RE|Harvest")
	float GetRewardDamageInterval() const { return RewardDamageInterval; }

	void SetResourceDefaults(FGameplayTag InRequiredWorkTag, FName InRewardName, int32 InRewardAmount, float InRewardDamageInterval = 25.f);

	UPROPERTY(BlueprintAssignable, Category = "AI_RE|Harvest")
	FAI_REHarvestedSignature OnHarvested;

	UPROPERTY(BlueprintAssignable, Category = "AI_RE|Harvest")
	FAI_REHarvestDepletedSignature OnDepleted;

	UPROPERTY(BlueprintAssignable, Category = "AI_RE|Harvest")
	FAI_REHarvestRespawnedSignature OnRespawned;

	UPROPERTY(BlueprintAssignable, Category = "AI_RE|Harvest")
	FAI_REHarvestDepletedStateChangedSignature OnDepletedStateChanged;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI_RE|Harvest", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI_RE|Harvest")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI_RE|Harvest")
	bool bIsDepleted = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI_RE|Harvest", meta = (ClampMin = "0.0"))
	float RespawnDelay = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI_RE|Harvest")
	FGameplayTag RequiredWorkTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI_RE|Harvest")
	FName RewardName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI_RE|Harvest", meta = (ClampMin = "0"))
	int32 RewardAmount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI_RE|Harvest", meta = (ClampMin = "0.0"))
	float RewardDamageInterval = 25.f;

private:
	FTimerHandle RespawnTimerHandle;
	float RewardDamageProgress = 0.f;

	void DepleteResource(AActor* InstigatorActor);
	void RespawnResource();
	void BroadcastDepletedState();
	int32 ConsumeRewardIntervals(float AppliedDamage);
	
	// Singleplayer simplified reward grant
	void GrantReward(AActor* InstigatorActor, int32 RewardMultiplier);
};

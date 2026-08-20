#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIREEnemyVitalityComponent.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UAnimMontage;
struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct AI_RE_API FAIREEnemyVitalitySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Vitality")
	float Health = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Vitality")
	float MaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Vitality")
	bool bDead = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREEnemyHealthChangedSignature,
	float,
	OldHealth,
	float,
	NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIREEnemyDeathSignature,
	AActor*,
	EnemyActor);

UCLASS(ClassGroup = (AIRE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIREEnemyVitalityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIREEnemyVitalityComponent();

	bool InitializeVitality(UAbilitySystemComponent* InAbilitySystem);
	void ShutdownVitality();
	void ConfigureDefaults(float InMaxHealth, float InInitialHealth);

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Vitality")
	FAIREEnemyVitalitySnapshot GetVitalitySnapshot() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Vitality")
	bool IsDead() const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Enemy|Vitality")
	FAIREEnemyHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Enemy|Vitality")
	FAIREEnemyDeathSignature OnDeath;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void SynchronizeDeath(float CurrentHealth);
	void PlayDeathMontage();
	void StopDeathMontage();

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Vitality", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxHealth = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Vitality", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialHealth = 500.0f;

	/** Optional in-place presentation. EnemyBase continues to own removal timing. */
	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Vitality|Presentation")
	TObjectPtr<UAnimMontage> DeathMontage;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<ACharacter> OwnerCharacter;
	FDelegateHandle HealthChangedDelegateHandle;
	bool bDead = false;
};

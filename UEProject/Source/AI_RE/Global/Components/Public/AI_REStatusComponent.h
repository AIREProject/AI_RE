#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI_REStatusComponent.generated.h"

// UI 업데이트를 위한 다이내믹 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatChangedSignature, float, CurrentValue, float, MaxValue);

USTRUCT(BlueprintType)
struct FGradualRecovery
{
	GENERATED_BODY()

	float HPPerTick = 0.f;
	float SPPerTick = 0.f;
	float HungerPerTick = 0.f;
	float ThirstyPerTick = 0.f;
	
	int32 TicksRemaining = 0;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class AI_RE_API UAI_REStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAI_REStatusComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ----------------------------------------------------
    // Survival Settings
    // ----------------------------------------------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Survival")
	float SurvivalTickRate = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Survival")
	float BaseHungerDepleteRate = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Survival")
	float BaseThirstyDepleteRate = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Survival")
	float RunMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Stamina")
	float StaminaRecoveryDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Stamina")
	float StaminaRecoveryRate = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status|Stamina")
	float StaminaRecoveryInterval = 0.1f;

	FTimerHandle SurvivalTimerHandle;

    // ----------------------------------------------------
    // Functions
    // ----------------------------------------------------
    UFUNCTION(BlueprintCallable, Category = "Status|Functions")
    void ConsumeSP(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Status|Functions")
    void ApplyDamage(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Status|Functions")
    void RecoverHP(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Status|Functions")
    void RecoverSP(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Status|Functions")
    void RecoverHunger(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Status|Functions")
    void RecoverThirsty(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Status|Functions")
	void AddGradualRecovery(float HP, float SP, float Hunger, float Thirsty, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Status|Functions")
	void BroadcastCurrentStats();

private:
	void HandleSurvivalStats();
	bool IsOwnerRunning() const;
	void HandleStaminaChanged(const struct FOnAttributeChangeData& ChangeData);
	void HandleHealthChanged(const struct FOnAttributeChangeData& ChangeData);
	void StartStaminaRecovery();
	void ApplyStaminaRecovery();
	void StopStaminaRecovery();

	// Gradual Recovery (HoT)
	UPROPERTY()
	TArray<FGradualRecovery> ActiveRecoveries;

	FTimerHandle RecoveryTimerHandle;
	FTimerHandle StaminaRecoveryDelayTimerHandle;
	FTimerHandle StaminaRecoveryTimerHandle;
	FDelegateHandle StaminaChangedDelegateHandle;
	FDelegateHandle HealthChangedDelegateHandle;
	TWeakObjectPtr<class UAbilitySystemComponent> CachedAbilitySystem;
	
	void ProcessGradualRecovery();
};

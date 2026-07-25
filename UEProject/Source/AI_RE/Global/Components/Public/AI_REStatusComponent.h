#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI_REStatusComponent.generated.h"

// UI 업데이트를 위한 다이내믹 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatChangedSignature, float, CurrentValue, float, MaxValue);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class AI_RE_API UAI_REStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAI_REStatusComponent();

protected:
	virtual void BeginPlay() override;

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

	FTimerHandle SurvivalTimerHandle;

    // ----------------------------------------------------
    // Stats
    // ----------------------------------------------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|HP")
	float MaxHP;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|HP")
	float CurrentHP;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|SP")
	float MaxSP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|SP")
	float CurrentSP;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Hunger")
	float MaxHunger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status|Hunger")
	float CurrentHunger;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Thirsty")
	float MaxThirsty;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Thirsty")
	float CurrentThirsty;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Combat")
	float Attack;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Combat")
	float Defense;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Status|Work")
	float WorkSpeed;

    // ----------------------------------------------------
    // Delegates (UI 연동용)
    // ----------------------------------------------------
	UPROPERTY(BlueprintAssignable, Category = "Status|Events")
	FOnStatChangedSignature OnHPChanged;

	UPROPERTY(BlueprintAssignable, Category = "Status|Events")
	FOnStatChangedSignature OnSPChanged;

	UPROPERTY(BlueprintAssignable, Category = "Status|Events")
	FOnStatChangedSignature OnHungerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Status|Events")
	FOnStatChangedSignature OnThirstyChanged;

    // ----------------------------------------------------
    // Functions
    // ----------------------------------------------------
    UFUNCTION(BlueprintCallable, Category = "Status|Functions")
    void ConsumeSP(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Status|Functions")
    void ApplyDamage(float Amount);

private:
	void HandleSurvivalStats();
	bool IsOwnerRunning() const;
};

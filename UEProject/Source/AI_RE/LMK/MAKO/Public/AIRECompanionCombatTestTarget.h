#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AIREThreatTargetInterface.h"
#include "GameFramework/Actor.h"
#include "AIRECompanionCombatTestTarget.generated.h"

class UAIRECompanionAttributeSet;
class UAIPerceptionStimuliSourceComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/** Temporary ASC-backed PIE fixture for M03-E03-T03 combat verification. */
UCLASS(Blueprintable)
class AI_RE_API AAIRECompanionCombatTestTarget
	: public AActor
	, public IAbilitySystemInterface
	, public IAIREThreatTargetInterface
{
	GENERATED_BODY()

public:
	AAIRECompanionCombatTestTarget();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual bool IsHostileThreatFor_Implementation(const AActor* Observer) const override;
	virtual bool IsAliveThreatTarget_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Testing")
	const UAIRECompanionAttributeSet* GetTestAttributeSet() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void SynchronizeDeadState(float CurrentHealth);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Testing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Testing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Testing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Testing", meta = (AllowPrivateAccess = "true"))
	bool bIsHostile = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Testing", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float InitialHealth = 100.0f;

	FDelegateHandle HealthChangedDelegateHandle;
};

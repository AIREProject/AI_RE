#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIREEnemyReactionComponent.generated.h"

class UAIREEnemyReactionAttributeSet;
class UAbilitySystemComponent;
class ACharacter;
class UAnimMontage;
struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class EAIREEnemyReactionState : uint8
{
	None,
	Flinching,
	Stunned
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREEnemyReactionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	EAIREEnemyReactionState State = EAIREEnemyReactionState::None;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	float FlinchGauge = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	float StunGauge = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	float FlinchThreshold = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	float StunThreshold = 200.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREEnemyReactionStateChangedSignature,
	EAIREEnemyReactionState,
	PreviousState,
	EAIREEnemyReactionState,
	CurrentState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREEnemyGroggyChangedSignature,
	float,
	CurrentGroggy,
	float,
	MaxGroggy);

UCLASS(ClassGroup = (AIRE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIREEnemyReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIREEnemyReactionComponent();

	bool InitializeReaction(
		UAbilitySystemComponent* InAbilitySystem,
		const UAIREEnemyReactionAttributeSet* InAttributeSet);
	void ShutdownReaction();
	void ConfigureThresholds(
		float InFlinchThreshold,
		float InFlinchDuration,
		float InStunThreshold,
		float InStunDuration);

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Reaction")
	FAIREEnemyReactionSnapshot GetReactionSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Reaction")
	bool IsAcceptingStagger() const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Reaction")
	void ResetForReturnHome();

	void HandleOwnerDeath();

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Enemy|Reaction")
	FAIREEnemyReactionStateChangedSignature OnReactionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Enemy|Reaction")
	FAIREEnemyGroggyChangedSignature OnGroggyChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void ScheduleReactionEvaluation();
	void EvaluateReactionGauges();
	void StartFlinch();
	void StartStun();
	void FinishReaction();
	void ResetGauges();
	void BroadcastGroggyChanged();
	void SetReactionState(EAIREEnemyReactionState NewState);
	void PlayReactionMontage(UAnimMontage* Montage);
	void StopActiveReactionMontage();

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FlinchThreshold = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float FlinchDuration = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float StunThreshold = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float StunDuration = 2.5f;

	/** Optional in-place presentation. Root motion policy is owned by the asset. */
	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Reaction|Presentation")
	TObjectPtr<UAnimMontage> FlinchMontage;

	/** Optional in-place presentation. Root motion policy is owned by the asset. */
	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Reaction|Presentation")
	TObjectPtr<UAnimMontage> StunMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveReactionMontage;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<const UAIREEnemyReactionAttributeSet> AttributeSet;
	TWeakObjectPtr<ACharacter> OwnerCharacter;
	FDelegateHandle FlinchChangedDelegateHandle;
	FDelegateHandle StunChangedDelegateHandle;
	FTimerHandle EvaluationTimerHandle;
	FTimerHandle ReactionTimerHandle;
	EAIREEnemyReactionState ReactionState = EAIREEnemyReactionState::None;
	bool bResettingGauges = false;
	bool bOwnerDead = false;
	bool bPendingFlinchEvaluation = false;
};

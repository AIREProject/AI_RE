#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AIREEnemyAggroComponent.h"
#include "AIREEnemyAttackComponent.h"
#include "AIREEnemyReactionComponent.h"
#include "AIREEnemyAIController.generated.h"

class AAIREEnemyBase;
class UAIREEnemyAggroComponent;
class UStateTreeAIComponent;

UENUM(BlueprintType)
enum class EAIREEnemyAwarenessState : uint8
{
	IdleUnaware,
	Alerted,
	EngagedChase,
	EngagedAttack,
	Searching,
	Returning,
	Flinching,
	Stunned,
	Dead
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREEnemyCombatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|AI")
	EAIREEnemyAwarenessState AwarenessState =
		EAIREEnemyAwarenessState::IdleUnaware;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|AI")
	FAIREEnemyAggroSnapshot Aggro;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|AI")
	FAIREEnemyAttackSnapshot Attack;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREEnemyAwarenessStateChangedSignature,
	EAIREEnemyAwarenessState,
	PreviousState,
	EAIREEnemyAwarenessState,
	CurrentState);

UCLASS(Blueprintable)
class AI_RE_API AAIREEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAIREEnemyAIController();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|AI")
	UAIREEnemyAggroComponent* GetAggroComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|AI")
	EAIREEnemyAwarenessState GetAwarenessState() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|AI")
	FAIREEnemyCombatSnapshot GetCombatSnapshot() const;

	/** Advances the fallback state machine from the active StateTree leaf task. */
	void TickStateTree(float DeltaSeconds);

	/** Cleans requests only when a StateTree leaf is stopped before changing state. */
	void ExitStateTreeState(EAIREEnemyAwarenessState ExpectedState);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|AI|StateTree")
	bool ReportStateTreeAwarenessState(
		EAIREEnemyAwarenessState NewState);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|AI")
	void RequestReturnHome();

	void ReportCombatDamage(AActor* Source, float Damage);
	void HandleEnemyDeath();
	void HandleEnemyReactionChanged(EAIREEnemyReactionState ReactionState);

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Enemy|AI")
	FAIREEnemyAwarenessStateChangedSignature OnAwarenessStateChanged;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateAwareness();
	void UpdateEngagement(AActor* Target);
	void BeginSearching();
	void BeginReturning();
	void CompleteReturnHome();
	void SetAwarenessState(
		EAIREEnemyAwarenessState NewState,
		const TCHAR* Reason);
	bool HasReachedHome() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIREEnemyAggroComponent> AggroComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|AI", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float AlertDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|AI", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float SearchDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|AI", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float HomeAcceptanceRadius = 75.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|AI", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float HomeLeashRadius = 2500.0f;

	TWeakObjectPtr<AAIREEnemyBase> Enemy;
	FVector HomeLocation = FVector::ZeroVector;
	FVector SearchLocation = FVector::ZeroVector;
	double StateDeadline = 0.0;
	EAIREEnemyAwarenessState AwarenessState =
		EAIREEnemyAwarenessState::IdleUnaware;
	bool bReturnRequested = false;
};

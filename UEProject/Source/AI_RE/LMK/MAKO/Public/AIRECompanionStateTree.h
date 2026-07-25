#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "AIRECompanionStateTree.generated.h"

class AAIRECompanionCharacter;
class AAIRECompanionAIController;
class AActor;
class APawn;
class UAIRECompanionEquipmentComponent;
class UAbilitySystemComponent;

UENUM(BlueprintType)
enum class EAIRECompanionBehaviorState : uint8
{
	None,
	Initialize,
	Disabled,
	Survival,
	Combat,
	DirectCommand,
	Work,
	ReturnToPlayer,
	FollowPlayer,
	Idle,
	MoveRetryDelay
};

USTRUCT()
struct FAIRECompanionContextEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIRECompanionCharacter> CompanionCharacter;

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIRECompanionAIController> CompanionController;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<APawn> PlayerPawn;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<AActor> ThreatTarget;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<UAIRECompanionEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (Units = "cm"))
	float DistanceToPlayer = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (Units = "cm/s"))
	float MovementSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (Units = "cm"))
	float FollowStopDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (Units = "cm"))
	float ReturnStartDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (Units = "cm"))
	float CombatDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (Units = "s"))
	float CombatCooldown = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasPlayer = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bShouldFollow = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bShouldReturn = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bIsDisabledRequested = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bIsSurvivalRequested = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bIsCombatRequested = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bIsDirectCommandRequested = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bIsWorkRequested = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bBehaviorSelectionChanged = false;

	UPROPERTY(Transient)
	uint16 PreviousBehaviorInputMask = 0;

	UPROPERTY(Transient)
	bool bHasPreviousBehaviorInput = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> PreviousPlayerPawn;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PreviousThreatTarget;
};

USTRUCT(meta = (DisplayName = "AIRE Companion Context", Category = "AIRE|Companion"))
struct FAIRECompanionContextEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIRECompanionContextEvaluatorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	void UpdateContext(FStateTreeExecutionContext& Context) const;
};

USTRUCT()
struct FAIREApplyCompanionMovementSettingsTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIRECompanionCharacter> CompanionCharacter;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Units = "cm/s"))
	float MovementSpeed = 0.0f;
};

USTRUCT(meta = (DisplayName = "Apply Companion Movement Settings", Category = "AIRE|Companion"))
struct FAIREApplyCompanionMovementSettingsTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIREApplyCompanionMovementSettingsTaskInstanceData;

	FAIREApplyCompanionMovementSettingsTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FAIRECompanionIdleTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "Companion Idle", Category = "AIRE|Companion"))
struct FAIRECompanionIdleTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIRECompanionIdleTaskInstanceData;

	FAIRECompanionIdleTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FAIRECompanionBehaviorDebugTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EAIRECompanionBehaviorState BehaviorState = EAIRECompanionBehaviorState::None;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bOwnsMovementRequest = false;
};

USTRUCT(meta = (DisplayName = "Companion Behavior Debug", Category = "AIRE|Companion"))
struct FAIRECompanionBehaviorDebugTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIRECompanionBehaviorDebugTaskInstanceData;

	FAIRECompanionBehaviorDebugTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FAIRECompanionEngageThreatTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIRECompanionAIController> CompanionController;

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UAIRECompanionEquipmentComponent> EquipmentComponent;

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> ThreatTarget;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Units = "cm"))
	float CombatDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Units = "s"))
	float CombatCooldown = 0.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ActiveTarget;

	UPROPERTY(Transient)
	float RetryTimeRemaining = 0.0f;

	UPROPERTY(Transient)
	bool bMoveRequested = false;
};

USTRUCT(meta = (DisplayName = "Engage Companion Threat", Category = "AIRE|Companion"))
struct FAIRECompanionEngageThreatTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIRECompanionEngageThreatTaskInstanceData;

	FAIRECompanionEngageThreatTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

private:
	static bool IsTargetUsable(const AActor* TargetActor);
	static bool IsTargetInRange(
		const APawn& CompanionPawn,
		const AActor& TargetActor,
		float CombatDistance);
	static void CancelOwnedRequests(FInstanceDataType& InstanceData);
};

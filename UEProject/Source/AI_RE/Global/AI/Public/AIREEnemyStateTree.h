#pragma once

#include "CoreMinimal.h"
#include "AIREEnemyAIController.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "Tasks/StateTreeAITask.h"
#include "AIREEnemyStateTree.generated.h"

class AAIREEnemyAIController;

/** AI schema pinned to the project-owned Enemy controller and pawn classes. */
UCLASS()
class AI_RE_API UAIREEnemyStateTreeSchema final
	: public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UAIREEnemyStateTreeSchema(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());
};

USTRUCT()
struct FAIREEnemyAwarenessConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIREEnemyAIController> EnemyController;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EAIREEnemyAwarenessState RequiredState =
		EAIREEnemyAwarenessState::IdleUnaware;
};

USTRUCT(meta = (DisplayName = "Enemy Awareness State", Category = "AIRE|Enemy"))
struct FAIREEnemyAwarenessCondition final
	: public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIREEnemyAwarenessConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(
		FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct FAIREEnemyStateTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIREEnemyAIController> EnemyController;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EAIREEnemyAwarenessState ExpectedState =
		EAIREEnemyAwarenessState::IdleUnaware;
};

/** Runs one awareness state's lifetime and yields when the controller changes state. */
USTRUCT(meta = (DisplayName = "Run Enemy State", Category = "AIRE|Enemy"))
struct FAIREEnemyStateTask final : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIREEnemyStateTaskInstanceData;

	FAIREEnemyStateTask();

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
};

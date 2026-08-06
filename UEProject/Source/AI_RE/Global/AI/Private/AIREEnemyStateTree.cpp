#include "AIREEnemyStateTree.h"

#include "AIREEnemyBase.h"
#include "StateTreeExecutionContext.h"

UAIREEnemyStateTreeSchema::UAIREEnemyStateTreeSchema(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AAIREEnemyAIController::StaticClass();
	ContextActorClass = AAIREEnemyBase::StaticClass();
	check(ContextDataDescs.Num() >= 2);
	ContextDataDescs[0].Struct = ContextActorClass.Get();
	ContextDataDescs[1].Struct = AIControllerClass.Get();
}

bool FAIREEnemyAwarenessCondition::TestCondition(
	FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	return IsValid(InstanceData.EnemyController)
		&& InstanceData.EnemyController->GetAwarenessState()
			== InstanceData.RequiredState;
}

FAIREEnemyStateTask::FAIREEnemyStateTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIREEnemyStateTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.EnemyController)
		|| !IsValid(InstanceData.EnemyController->GetPawn()))
	{
		return EStateTreeRunStatus::Failed;
	}

	return InstanceData.EnemyController->GetAwarenessState()
		== InstanceData.ExpectedState
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FAIREEnemyStateTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.EnemyController)
		|| !IsValid(InstanceData.EnemyController->GetPawn()))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.EnemyController->TickStateTree(DeltaTime);
	return InstanceData.EnemyController->GetAwarenessState()
		== InstanceData.ExpectedState
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}

void FAIREEnemyStateTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (IsValid(InstanceData.EnemyController))
	{
		InstanceData.EnemyController->ExitStateTreeState(
			InstanceData.ExpectedState);
	}
}

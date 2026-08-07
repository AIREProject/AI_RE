#include "AIREEnemyMeleeTraceAnimNotifyState.h"

#include "AIREEnemyAttackComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
UAIREEnemyAttackComponent* FindEnemyAttackComponent(
	USkeletalMeshComponent* MeshComp)
{
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	return IsValid(Owner)
		? Owner->FindComponentByClass<UAIREEnemyAttackComponent>()
		: nullptr;
}
}

void UAIREEnemyMeleeTraceAnimNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	RemoveStaleExecutionIds();

	UAIREEnemyAttackComponent* AttackComponent =
		FindEnemyAttackComponent(MeshComp);
	if (!IsValid(AttackComponent))
	{
		return;
	}

	const FAIREEnemyAttackSnapshot Snapshot =
		AttackComponent->GetAttackSnapshot();
	if (!Snapshot.bActive || !Snapshot.ExecutionId.IsValid())
	{
		return;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	ActiveExecutionIds.Add(MeshKey, Snapshot.ExecutionId);
	AttackComponent->BeginMeleeTraceWindow(Snapshot.ExecutionId);
}

void UAIREEnemyMeleeTraceAnimNotifyState::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	const FGuid* ExecutionId = ActiveExecutionIds.Find(MeshKey);
	UAIREEnemyAttackComponent* AttackComponent =
		FindEnemyAttackComponent(MeshComp);
	if (!ExecutionId || !IsValid(AttackComponent))
	{
		ActiveExecutionIds.Remove(MeshKey);
		return;
	}

	const FAIREEnemyAttackSnapshot Snapshot =
		AttackComponent->GetAttackSnapshot();
	if (!Snapshot.bActive || Snapshot.ExecutionId != *ExecutionId)
	{
		ActiveExecutionIds.Remove(MeshKey);
		return;
	}

	AttackComponent->UpdateMeleeTraceWindow(*ExecutionId);
}

void UAIREEnemyMeleeTraceAnimNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	const FGuid* ExecutionId = ActiveExecutionIds.Find(MeshKey);
	UAIREEnemyAttackComponent* AttackComponent =
		FindEnemyAttackComponent(MeshComp);
	if (ExecutionId && IsValid(AttackComponent))
	{
		AttackComponent->EndMeleeTraceWindow(*ExecutionId);
	}
	ActiveExecutionIds.Remove(MeshKey);
	RemoveStaleExecutionIds();
}

void UAIREEnemyMeleeTraceAnimNotifyState::RemoveStaleExecutionIds()
{
	for (auto It = ActiveExecutionIds.CreateIterator(); It; ++It)
	{
		USkeletalMeshComponent* MeshComp = It.Key().Get();
		UAIREEnemyAttackComponent* AttackComponent =
			FindEnemyAttackComponent(MeshComp);
		if (!IsValid(AttackComponent))
		{
			It.RemoveCurrent();
			continue;
		}

		const FAIREEnemyAttackSnapshot Snapshot =
			AttackComponent->GetAttackSnapshot();
		if (!Snapshot.bActive || Snapshot.ExecutionId != It.Value())
		{
			It.RemoveCurrent();
		}
	}
}

#include "AIREEnemyAttackMovementAnimNotifyState.h"

#include "AIREEnemyAttackComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
UAIREEnemyAttackComponent* FindEnemyAttackMovementComponent(
	USkeletalMeshComponent* MeshComp)
{
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	return IsValid(Owner)
		? Owner->FindComponentByClass<UAIREEnemyAttackComponent>()
		: nullptr;
}
}

void UAIREEnemyAttackMovementAnimNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	RemoveStaleExecutionIds();

	UAIREEnemyAttackComponent* AttackComponent =
		FindEnemyAttackMovementComponent(MeshComp);
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
	AttackComponent->BeginAttackMovementWindow(
		Snapshot.ExecutionId,
		TotalDuration);
}

void UAIREEnemyAttackMovementAnimNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	const FGuid* ExecutionId = ActiveExecutionIds.Find(MeshKey);
	UAIREEnemyAttackComponent* AttackComponent =
		FindEnemyAttackMovementComponent(MeshComp);
	if (ExecutionId && IsValid(AttackComponent))
	{
		AttackComponent->EndAttackMovementWindow(*ExecutionId);
	}
	ActiveExecutionIds.Remove(MeshKey);
	RemoveStaleExecutionIds();
}

void UAIREEnemyAttackMovementAnimNotifyState::RemoveStaleExecutionIds()
{
	for (auto It = ActiveExecutionIds.CreateIterator(); It; ++It)
	{
		USkeletalMeshComponent* MeshComp = It.Key().Get();
		UAIREEnemyAttackComponent* AttackComponent =
			FindEnemyAttackMovementComponent(MeshComp);
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

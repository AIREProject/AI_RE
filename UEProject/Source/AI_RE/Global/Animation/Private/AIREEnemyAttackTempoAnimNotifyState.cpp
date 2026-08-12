#include "AIREEnemyAttackTempoAnimNotifyState.h"

#include "AIREEnemyAttackComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
UAIREEnemyAttackComponent* FindEnemyAttackTempoComponent(
	USkeletalMeshComponent* MeshComp)
{
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	return IsValid(Owner)
		? Owner->FindComponentByClass<UAIREEnemyAttackComponent>()
		: nullptr;
}
}

void UAIREEnemyAttackTempoAnimNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	RemoveStaleExecutionIds();

	UAIREEnemyAttackComponent* AttackComponent =
		FindEnemyAttackTempoComponent(MeshComp);
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

	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	if (!IsValid(Montage)
		|| !AttackComponent->BeginAttackTempoWindow(
		Snapshot.ExecutionId,
		Montage,
		StrikeIndex,
		StrikeStartTime,
		WindowEndTime,
		AnticipationPlayRateMultiplier,
		StrikePlayRateMultiplier))
	{
		return;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	ActiveExecutionIds.Add(MeshKey, Snapshot.ExecutionId);
	AttackComponent->UpdateAttackTempoWindow(
		Snapshot.ExecutionId,
		StrikeIndex);
}

void UAIREEnemyAttackTempoAnimNotifyState::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	const FGuid* ExecutionId = ActiveExecutionIds.Find(MeshKey);
	UAIREEnemyAttackComponent* AttackComponent =
		FindEnemyAttackTempoComponent(MeshComp);
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

	AttackComponent->UpdateAttackTempoWindow(*ExecutionId, StrikeIndex);
}

void UAIREEnemyAttackTempoAnimNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	const FGuid* ExecutionId = ActiveExecutionIds.Find(MeshKey);
	UAIREEnemyAttackComponent* AttackComponent =
		FindEnemyAttackTempoComponent(MeshComp);
	if (ExecutionId && IsValid(AttackComponent))
	{
		AttackComponent->EndAttackTempoWindow(*ExecutionId, StrikeIndex);
	}
	ActiveExecutionIds.Remove(MeshKey);
	RemoveStaleExecutionIds();
}

void UAIREEnemyAttackTempoAnimNotifyState::RemoveStaleExecutionIds()
{
	for (auto It = ActiveExecutionIds.CreateIterator(); It; ++It)
	{
		UAIREEnemyAttackComponent* AttackComponent =
			FindEnemyAttackTempoComponent(It.Key().Get());
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

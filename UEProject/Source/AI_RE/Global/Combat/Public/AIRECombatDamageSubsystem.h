#pragma once

#include "CoreMinimal.h"
#include "AIRECombatDamageTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIRECombatDamageSubsystem.generated.h"

UCLASS()
class AI_RE_API UAIRECombatDamageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AIRE|Combat")
	EAIRECombatDamageResult ApplyDamageRequest(
		const FAIRECombatDamageRequest& Request);

	virtual void Deinitialize() override;

private:
	struct FAppliedExecutionRecord
	{
		TWeakObjectPtr<AActor> Target;
		FGuid ExecutionId;
	};

	void PruneExecutionHistory();
	bool HasAppliedExecution(AActor* Target, const FGuid& ExecutionId) const;
	void RecordAppliedExecution(AActor* Target, const FGuid& ExecutionId);
	void RemoveAppliedExecution(AActor* Target, const FGuid& ExecutionId);

	TMap<TWeakObjectPtr<AActor>, TSet<FGuid>> AppliedExecutionIdsByTarget;
	TArray<FAppliedExecutionRecord> AppliedExecutionHistory;
};

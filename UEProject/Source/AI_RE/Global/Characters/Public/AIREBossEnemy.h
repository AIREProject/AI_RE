#pragma once

#include "CoreMinimal.h"
#include "AIREEnemyBase.h"
#include "AIREBossEnemy.generated.h"

/** The M03-E09 gate enemy. Boss phases, replication, and encounter orchestration are out of scope. */
UCLASS(Blueprintable)
class AI_RE_API AAIREBossEnemy : public AAIREEnemyBase
{
	GENERATED_BODY()

public:
	AAIREBossEnemy();

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Boss")
	FText GetBossDisplayName() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Enemy|Boss")
	FText BossDisplayName;
};

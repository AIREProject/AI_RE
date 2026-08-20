#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "AIRECombatDamageTypes.generated.h"

UENUM(BlueprintType)
enum class EAIRECombatTargetingMode : uint8
{
	SingleTarget,
	Area
};

UENUM(BlueprintType)
enum class EAIRECombatDamageResult : uint8
{
	Applied,
	InvalidWorld,
	InvalidSource,
	SourceDead,
	InvalidTarget,
	SelfTarget,
	InvalidExecutionId,
	InvalidMagnitude,
	UnsupportedTarget,
	MissingSourceAbilitySystem,
	MissingTargetAbilitySystem,
	TargetDead,
	TargetInvulnerable,
	DuplicateExecution,
	EffectSpecFailed
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRECombatDamageRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AIRE|Combat")
	TObjectPtr<AActor> Source;

	UPROPERTY(BlueprintReadWrite, Category = "AIRE|Combat")
	TObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadWrite, Category = "AIRE|Combat")
	float Damage = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "AIRE|Combat")
	float StaggerValue = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "AIRE|Combat")
	FGuid ExecutionId;

	UPROPERTY(BlueprintReadWrite, Category = "AIRE|Combat")
	bool bHasHitResult = false;

	UPROPERTY(BlueprintReadWrite, Category = "AIRE|Combat", meta = (EditCondition = "bHasHitResult"))
	FHitResult HitResult;
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"

class AActor;
class UWorld;

enum class EAIRECombatMeleeTraceResult : uint8
{
	NoHit,
	TargetHit,
	Blocked,
	Invalid
};

struct AI_RE_API FAIRECombatMeleeTraceSegment
{
	FAIRECombatMeleeTraceSegment() = default;
	FAIRECombatMeleeTraceSegment(
		const FVector& InStart,
		const FVector& InEnd)
		: Start(InStart)
		, End(InEnd)
	{
	}

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
};

struct AI_RE_API FAIRECombatMeleeTraceRequest
{
	/** All object pointers are observed only for the duration of Resolve. */
	UWorld* World = nullptr;
	AActor* Source = nullptr;
	AActor* Target = nullptr;
	TArray<FAIRECombatMeleeTraceSegment> Segments;
	float Radius = 0.0f;
	ECollisionChannel TraceChannel = ECC_MAX;
};

struct AI_RE_API FAIRECombatMeleeTraceResolution
{
	EAIRECombatMeleeTraceResult Result =
		EAIRECombatMeleeTraceResult::Invalid;
	/** Populated only when Result is TargetHit. */
	FHitResult HitResult;
};

/**
 * Resolves sphere-swept segments without selecting a target or applying damage.
 * Source and recursively attached actors are ignored. Target contact wins across
 * the supplied segments; otherwise any non-target blocking contact is Blocked.
 */
class AI_RE_API FAIRECombatMeleeTraceResolver
{
public:
	static FAIRECombatMeleeTraceResolution Resolve(
		const FAIRECombatMeleeTraceRequest& Request);
};

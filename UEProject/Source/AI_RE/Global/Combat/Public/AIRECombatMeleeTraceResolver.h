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

enum class EAIRECombatMeleeTraceShape : uint8
{
	Sphere,
	Capsule
};

struct AI_RE_API FAIRECombatMeleeTraceSegment
{
	FAIRECombatMeleeTraceSegment() = default;
	FAIRECombatMeleeTraceSegment(
		const FVector& InStart,
		const FVector& InEnd,
		const FQuat& InRotation = FQuat::Identity)
		: Start(InStart)
		, End(InEnd)
		, Rotation(InRotation)
	{
	}

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	/** Shape rotation used by capsule requests. Capsules use local Z as their axis. */
	FQuat Rotation = FQuat::Identity;
};

struct AI_RE_API FAIRECombatMeleeTraceRequest
{
	/** All object pointers are observed only for the duration of Resolve. */
	UWorld* World = nullptr;
	AActor* Source = nullptr;
	AActor* Target = nullptr;
	TArray<FAIRECombatMeleeTraceSegment> Segments;
	EAIRECombatMeleeTraceShape Shape = EAIRECombatMeleeTraceShape::Sphere;
	float Radius = 0.0f;
	/** Required only for capsule requests and includes the hemisphere caps. */
	float CapsuleHalfHeight = 0.0f;
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
 * Resolves sphere- or capsule-swept segments without selecting a target or applying damage.
 * Source and recursively attached actors are ignored. Target contact wins across
 * the supplied segments; otherwise any non-target blocking contact is Blocked.
 */
class AI_RE_API FAIRECombatMeleeTraceResolver
{
public:
	static FAIRECombatMeleeTraceResolution Resolve(
		const FAIRECombatMeleeTraceRequest& Request);
};

#include "AIRECombatMeleeTraceResolver.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarAIRECombatMeleeTraceDebug(
		TEXT("aire.Combat.MeleeTrace.Debug"),
		0,
		TEXT("Draw shared combat melee sphere and capsule sweeps.\n")
		TEXT("0: Disabled\n")
		TEXT("1: Enabled (cyan=miss, green=target, red=blocked)"),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarAIREEnemyMeleeTraceDebug(
		TEXT("aire.Enemy.MeleeTrace.Debug"),
		0,
		TEXT("Deprecated alias for aire.Combat.MeleeTrace.Debug.\n")
		TEXT("0: Disabled\n")
		TEXT("1: Enabled"),
		ECVF_Cheat);

	bool IsMeleeTraceDebugEnabled()
	{
		return CVarAIRECombatMeleeTraceDebug.GetValueOnGameThread() > 0
			|| CVarAIREEnemyMeleeTraceDebug.GetValueOnGameThread() > 0;
	}

	void DrawSphereTraceSweep(
		const UWorld* World,
		const FVector& Start,
		const FVector& End,
		const float Radius,
		const FColor& Color)
	{
		constexpr float DebugLifetime = 1.0f;
		constexpr float DebugThickness = 1.5f;
		const FVector SweepDelta = End - Start;
		if (SweepDelta.IsNearlyZero())
		{
			DrawDebugSphere(
				World,
				Start,
				Radius,
				16,
				Color,
				false,
				DebugLifetime,
				0,
				DebugThickness);
			return;
		}

		const FVector SweepCenter = (Start + End) * 0.5f;
		const float HalfHeight = (SweepDelta.Size() * 0.5f) + Radius;
		const FQuat SweepRotation = FQuat::FindBetweenNormals(
			FVector::UpVector,
			SweepDelta.GetSafeNormal());
		DrawDebugCapsule(
			World,
			SweepCenter,
			HalfHeight,
			Radius,
			SweepRotation,
			Color,
			false,
			DebugLifetime,
			0,
			DebugThickness);
	}

	void DrawCapsuleTraceSweep(
		const UWorld* World,
		const FVector& Start,
		const FVector& End,
		const float Radius,
		const float HalfHeight,
		const FQuat& Rotation,
		const FColor& Color)
	{
		constexpr float DebugLifetime = 1.0f;
		constexpr float DebugThickness = 1.5f;
		DrawDebugCapsule(
			World,
			Start,
			HalfHeight,
			Radius,
			Rotation,
			Color,
			false,
			DebugLifetime,
			0,
			DebugThickness);
		if (!Start.Equals(End))
		{
			DrawDebugCapsule(
				World,
				End,
				HalfHeight,
				Radius,
				Rotation,
				Color,
				false,
				DebugLifetime,
				0,
				DebugThickness);
			DrawDebugLine(
				World,
				Start,
				End,
				Color,
				false,
				DebugLifetime,
				0,
				DebugThickness);
		}
	}
}
#endif

FAIRECombatMeleeTraceResolution FAIRECombatMeleeTraceResolver::Resolve(
	const FAIRECombatMeleeTraceRequest& Request)
{
	FAIRECombatMeleeTraceResolution Resolution;
	const bool bValidRequest = IsValid(Request.World)
		&& IsValid(Request.Source)
		&& (!Request.Target || (IsValid(Request.Target) && Request.Source != Request.Target && !Request.Target->IsActorBeingDestroyed()))
		&& !Request.Source->IsActorBeingDestroyed()
		&& Request.Source->GetWorld() == Request.World
		&& (!Request.Target || Request.Target->GetWorld() == Request.World)
		&& !Request.Segments.IsEmpty()
		&& (Request.Shape == EAIRECombatMeleeTraceShape::Sphere
			|| Request.Shape == EAIRECombatMeleeTraceShape::Capsule)
		&& FMath::IsFinite(Request.Radius)
		&& Request.Radius > 0.0f
		&& (Request.Shape != EAIRECombatMeleeTraceShape::Capsule
			|| (FMath::IsFinite(Request.CapsuleHalfHeight)
				&& Request.CapsuleHalfHeight >= Request.Radius))
		&& Request.TraceChannel >= ECC_WorldStatic
		&& Request.TraceChannel < ECC_MAX;
	if (!bValidRequest)
	{
		return Resolution;
	}

	for (const FAIRECombatMeleeTraceSegment& Segment : Request.Segments)
	{
		if (Segment.Start.ContainsNaN()
			|| Segment.End.ContainsNaN()
			|| Segment.Rotation.ContainsNaN()
			|| !Segment.Rotation.IsNormalized())
		{
			return Resolution;
		}
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(AIRECombatMeleeTrace),
		false,
		Request.Source);
	TArray<AActor*> AttachedActors;
	Request.Source->GetAttachedActors(AttachedActors, true, true);
	QueryParams.AddIgnoredActors(AttachedActors);
	QueryParams.AddIgnoredActors(Request.IgnoredActors);

	bool bTargetHit = false;
	bool bBlocked = false;
	const FCollisionShape CollisionShape =
		Request.Shape == EAIRECombatMeleeTraceShape::Capsule
			? FCollisionShape::MakeCapsule(
				Request.Radius,
				Request.CapsuleHalfHeight)
			: FCollisionShape::MakeSphere(Request.Radius);
	for (const FAIRECombatMeleeTraceSegment& Segment : Request.Segments)
	{
		FHitResult HitResult;
		const bool bHit = Request.World->SweepSingleByChannel(
			HitResult,
			Segment.Start,
			Segment.End,
			Segment.Rotation,
			Request.TraceChannel,
			CollisionShape,
			QueryParams);
		const bool bSegmentTargetHit =
			bHit && (HitResult.GetActor() == Request.Target || (!Request.Target && HitResult.GetActor() && HitResult.GetActor() != Request.Source));

#if ENABLE_DRAW_DEBUG
		if (IsMeleeTraceDebugEnabled())
		{
			const FColor DebugColor = !bHit
				? FColor::Cyan
				: bSegmentTargetHit
					? FColor::Green
					: FColor::Red;
			if (Request.Shape == EAIRECombatMeleeTraceShape::Capsule)
			{
				DrawCapsuleTraceSweep(
					Request.World,
					Segment.Start,
					Segment.End,
					Request.Radius,
					Request.CapsuleHalfHeight,
					Segment.Rotation,
					DebugColor);
			}
			else
			{
				DrawSphereTraceSweep(
					Request.World,
					Segment.Start,
					Segment.End,
					Request.Radius,
					DebugColor);
			}
			if (bHit)
			{
				DrawDebugPoint(
					Request.World,
					HitResult.ImpactPoint,
					12.0f,
					DebugColor,
					false,
					1.0f);
			}
		}
#endif

		if (bSegmentTargetHit)
		{
			if (!bTargetHit)
			{
				Resolution.HitResult = HitResult;
				bTargetHit = true;
			}
		}
		else if (bHit)
		{
			// If we haven't found a target yet, store this blocked hit so it can be processed
			if (!bTargetHit && !bBlocked)
			{
				Resolution.HitResult = HitResult;
			}
			bBlocked = true;
		}
	}

	Resolution.Result = bTargetHit
		? EAIRECombatMeleeTraceResult::TargetHit
		: bBlocked
			? EAIRECombatMeleeTraceResult::Blocked
			: EAIRECombatMeleeTraceResult::NoHit;
	return Resolution;
}

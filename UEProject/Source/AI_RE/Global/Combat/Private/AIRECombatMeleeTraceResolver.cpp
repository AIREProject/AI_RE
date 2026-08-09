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
		TEXT("Draw shared combat melee sphere sweeps.\n")
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

	void DrawMeleeTraceSweep(
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
}
#endif

FAIRECombatMeleeTraceResolution FAIRECombatMeleeTraceResolver::Resolve(
	const FAIRECombatMeleeTraceRequest& Request)
{
	FAIRECombatMeleeTraceResolution Resolution;
	const bool bValidRequest = IsValid(Request.World)
		&& IsValid(Request.Source)
		&& IsValid(Request.Target)
		&& Request.Source != Request.Target
		&& !Request.Source->IsActorBeingDestroyed()
		&& !Request.Target->IsActorBeingDestroyed()
		&& Request.Source->GetWorld() == Request.World
		&& Request.Target->GetWorld() == Request.World
		&& !Request.Segments.IsEmpty()
		&& FMath::IsFinite(Request.Radius)
		&& Request.Radius > 0.0f
		&& Request.TraceChannel >= ECC_WorldStatic
		&& Request.TraceChannel < ECC_MAX;
	if (!bValidRequest)
	{
		return Resolution;
	}

	for (const FAIRECombatMeleeTraceSegment& Segment : Request.Segments)
	{
		if (Segment.Start.ContainsNaN() || Segment.End.ContainsNaN())
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

	bool bTargetHit = false;
	bool bBlocked = false;
	for (const FAIRECombatMeleeTraceSegment& Segment : Request.Segments)
	{
		FHitResult HitResult;
		const bool bHit = Request.World->SweepSingleByChannel(
			HitResult,
			Segment.Start,
			Segment.End,
			FQuat::Identity,
			Request.TraceChannel,
			FCollisionShape::MakeSphere(Request.Radius),
			QueryParams);
		const bool bSegmentTargetHit =
			bHit && HitResult.GetActor() == Request.Target;

#if ENABLE_DRAW_DEBUG
		if (IsMeleeTraceDebugEnabled())
		{
			const FColor DebugColor = !bHit
				? FColor::Cyan
				: bSegmentTargetHit
					? FColor::Green
					: FColor::Red;
			DrawMeleeTraceSweep(
				Request.World,
				Segment.Start,
				Segment.End,
				Request.Radius,
				DebugColor);
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

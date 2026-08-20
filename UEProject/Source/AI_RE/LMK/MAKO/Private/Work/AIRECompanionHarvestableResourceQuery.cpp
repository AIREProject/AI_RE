#include "Work/AIRECompanionHarvestableResourceQuery.h"

#include "AI_REHarvestGameplayTags.h"
#include "AI_REHarvestableResourceActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Work/AIRECompanionHarvestWorkRequest.h"

namespace
{
	struct FHarvestableResourceCandidate
	{
		TWeakObjectPtr<AAI_REHarvestableResourceActor> Resource;
		double DistanceSquared = 0.0;
	};

	bool IsValidResource(const AAI_REHarvestableResourceActor* Resource)
	{
		return IsValid(Resource)
			&& !Resource->IsActorBeingDestroyed()
			&& IsValid(Resource->GetHarvestableResourceComponent())
			&& Resource->GetHarvestableResourceComponent()->GetRequiredWorkTag().IsValid()
			&& FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(Resource);
	}

	bool CollectNearbyResourcesInternal(
		const AActor& Origin,
		TArray<TWeakObjectPtr<AAI_REHarvestableResourceActor>>& OutResources,
		const float RadiusCentimeters,
		const FGameplayTag RequiredResourceTag)
	{
		OutResources.Reset();
		if (!IsValid(&Origin)
			|| Origin.IsActorBeingDestroyed()
			|| !FMath::IsFinite(RadiusCentimeters)
			|| RadiusCentimeters <= 0.0f
			|| RadiusCentimeters
				> FAIRECompanionHarvestableResourceQuery::DefaultRadiusCentimeters)
		{
			return false;
		}

		UWorld* World = Origin.GetWorld();
		if (!IsValid(World))
		{
			return false;
		}

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(AIRECompanionHarvestableResourceQuery),
			false);
		QueryParams.AddIgnoredActor(&Origin);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			Origin.GetActorLocation(),
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(RadiusCentimeters),
			QueryParams);

		const FVector OriginLocation = Origin.GetActorLocation();
		const double MaximumDistanceSquared =
			static_cast<double>(RadiusCentimeters) * RadiusCentimeters;
		TArray<FHarvestableResourceCandidate> Candidates;
		TSet<const AAI_REHarvestableResourceActor*> SeenResources;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AAI_REHarvestableResourceActor* Resource =
				Cast<AAI_REHarvestableResourceActor>(Overlap.GetActor());
			if (!IsValidResource(Resource) || SeenResources.Contains(Resource))
			{
				continue;
			}

			const UAI_REHarvestableResourceComponent* ResourceComponent =
				Resource->GetHarvestableResourceComponent();
			if (RequiredResourceTag.IsValid()
				&& ResourceComponent->GetRequiredWorkTag() != RequiredResourceTag)
			{
				continue;
			}

			const double DistanceSquared = FVector::DistSquared(
				OriginLocation,
				Resource->GetActorLocation());
			if (!FMath::IsFinite(DistanceSquared)
				|| DistanceSquared > MaximumDistanceSquared)
			{
				continue;
			}

			SeenResources.Add(Resource);
			FHarvestableResourceCandidate& Candidate =
				Candidates.AddDefaulted_GetRef();
			Candidate.Resource = Resource;
			Candidate.DistanceSquared = DistanceSquared;
		}

		Candidates.Sort(
			[](const FHarvestableResourceCandidate& Left,
				const FHarvestableResourceCandidate& Right)
			{
				return Left.DistanceSquared < Right.DistanceSquared;
			});
		if (Candidates.Num()
			> FAIRECompanionHarvestableResourceQuery::MaxNearbyResources)
		{
			Candidates.SetNum(
				FAIRECompanionHarvestableResourceQuery::MaxNearbyResources,
				EAllowShrinking::No);
		}

		OutResources.Reserve(Candidates.Num());
		for (const FHarvestableResourceCandidate& Candidate : Candidates)
		{
			if (IsValidResource(Candidate.Resource.Get()))
			{
				OutResources.Add(Candidate.Resource);
			}
		}
		return true;
	}
}

bool FAIRECompanionHarvestableResourceQuery::CollectNearbyResources(
	const AActor& Origin,
	TArray<TWeakObjectPtr<AAI_REHarvestableResourceActor>>& OutResources,
	const float RadiusCentimeters)
{
	return CollectNearbyResourcesInternal(
		Origin,
		OutResources,
		RadiusCentimeters,
		FGameplayTag());
}

AAI_REHarvestableResourceActor*
FAIRECompanionHarvestableResourceQuery::FindNearestCompatible(
	const AActor& Origin,
	const FGameplayTag RequiredResourceTag,
	const float RadiusCentimeters)
{
	if (!RequiredResourceTag.IsValid())
	{
		return nullptr;
	}
	TArray<TWeakObjectPtr<AAI_REHarvestableResourceActor>> Resources;
	if (!CollectNearbyResourcesInternal(
			Origin,
			Resources,
			RadiusCentimeters,
			RequiredResourceTag))
	{
		return nullptr;
	}
	return Resources.IsEmpty() ? nullptr : Resources[0].Get();
}

bool FAIRECompanionHarvestableResourceQuery::GetNearbyWoodCount(
	const AActor& Origin,
	int32& OutCount,
	const float RadiusCentimeters)
{
	OutCount = 0;
	TArray<TWeakObjectPtr<AAI_REHarvestableResourceActor>> Resources;
	if (!CollectNearbyResourcesInternal(
			Origin,
			Resources,
			RadiusCentimeters,
			AI_REHarvestGameplayTags::Resource_Wood))
	{
		return false;
	}
	OutCount = Resources.Num();
	return true;
}

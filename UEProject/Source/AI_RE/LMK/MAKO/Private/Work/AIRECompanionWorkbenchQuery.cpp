#include "Work/AIRECompanionWorkbenchQuery.h"

#include "AI_REWorkBenchBase.h"
#include "AI_REWorkbenchGameplayTags.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

namespace
{
	struct FWorkbenchCandidate
	{
		TWeakObjectPtr<AAI_REWorkBenchBase> Workbench;
		double DistanceSquared = 0.0;
	};

	FGameplayTag GetCanonicalWorkbenchTag(const EWorkbenchType WorkbenchType)
	{
		switch (WorkbenchType)
		{
		case EWorkbenchType::Basic:
			return AI_REWorkbenchGameplayTags::Workbench_Basic;
		case EWorkbenchType::Blacksmith:
			return AI_REWorkbenchGameplayTags::Workbench_Blacksmith;
		case EWorkbenchType::Smelter:
			return AI_REWorkbenchGameplayTags::Workbench_Smelter;
		case EWorkbenchType::Alchemy:
			return AI_REWorkbenchGameplayTags::Workbench_Alchemy;
		case EWorkbenchType::Cook:
			return AI_REWorkbenchGameplayTags::Workbench_Cook;
		case EWorkbenchType::None:
		default:
			return FGameplayTag();
		}
	}

	bool IsValidQueryRadius(const float RadiusCentimeters)
	{
		return FMath::IsFinite(RadiusCentimeters)
			&& RadiusCentimeters > 0.0f
			&& RadiusCentimeters <= FAIRECompanionWorkbenchQuery::DefaultRadiusCentimeters;
	}

	bool TryGetQueryWorld(
		const AActor& Origin,
		const float RadiusCentimeters,
		UWorld*& OutWorld)
	{
		OutWorld = nullptr;
		if (!IsValid(&Origin)
			|| Origin.IsActorBeingDestroyed()
			|| !IsValidQueryRadius(RadiusCentimeters))
		{
			return false;
		}

		OutWorld = Origin.GetWorld();
		return IsValid(OutWorld);
	}

	bool IsValidWorkbench(const AAI_REWorkBenchBase* Workbench)
	{
		return IsValid(Workbench) && !Workbench->IsActorBeingDestroyed();
	}

	void SortAndLimitCandidates(
		TArray<FWorkbenchCandidate>& Candidates)
	{
		Candidates.Sort(
			[](const FWorkbenchCandidate& Left,
				const FWorkbenchCandidate& Right)
			{
				return Left.DistanceSquared < Right.DistanceSquared;
			});

		if (Candidates.Num() > FAIRECompanionWorkbenchQuery::MaxNearbyWorkbenches)
		{
			Candidates.SetNum(
				FAIRECompanionWorkbenchQuery::MaxNearbyWorkbenches,
				EAllowShrinking::No);
		}
	}
}

bool FAIRECompanionWorkbenchQuery::CollectNearbyWorkbenches(
	const AActor& Origin,
	TArray<TWeakObjectPtr<AAI_REWorkBenchBase>>& OutWorkbenches,
	const float RadiusCentimeters)
{
	OutWorkbenches.Reset();

	UWorld* World = nullptr;
	if (!TryGetQueryWorld(Origin, RadiusCentimeters, World))
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(AIRECompanionWorkbenchQuery),
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

	TArray<FWorkbenchCandidate> Candidates;
	Candidates.Reserve(FMath::Min(Overlaps.Num(), MaxNearbyWorkbenches));
	TSet<const AAI_REWorkBenchBase*> SeenWorkbenches;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AAI_REWorkBenchBase* Workbench = Cast<AAI_REWorkBenchBase>(Overlap.GetActor());
		if (!IsValidWorkbench(Workbench)
			|| SeenWorkbenches.Contains(Workbench))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(
			Origin.GetActorLocation(),
			Workbench->GetActorLocation());
		if (!FMath::IsFinite(DistanceSquared))
		{
			continue;
		}

		SeenWorkbenches.Add(Workbench);
		FWorkbenchCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Workbench = Workbench;
		Candidate.DistanceSquared = DistanceSquared;
	}

	SortAndLimitCandidates(Candidates);
	OutWorkbenches.Reserve(Candidates.Num());
	for (const FWorkbenchCandidate& Candidate : Candidates)
	{
		if (IsValidWorkbench(Candidate.Workbench.Get()))
		{
			OutWorkbenches.Add(Candidate.Workbench);
		}
	}

	return true;
}

bool FAIRECompanionWorkbenchQuery::GetNearbyCapabilityIds(
	const AActor& Origin,
	TArray<FString>& OutCapabilityIds,
	const float RadiusCentimeters)
{
	OutCapabilityIds.Reset();

	TArray<TWeakObjectPtr<AAI_REWorkBenchBase>> Workbenches;
	if (!CollectNearbyWorkbenches(Origin, Workbenches, RadiusCentimeters))
	{
		return false;
	}

	TSet<FString> CanonicalCapabilityIds;
	TSet<FString> AdditionalCapabilityIds;
	for (const TWeakObjectPtr<AAI_REWorkBenchBase>& WorkbenchReference : Workbenches)
	{
		const AAI_REWorkBenchBase* Workbench = WorkbenchReference.Get();
		if (!IsValidWorkbench(Workbench))
		{
			continue;
		}

		const FGameplayTag CanonicalTag =
			GetCanonicalWorkbenchTag(Workbench->WorkbenchType);
		if (CanonicalTag.IsValid())
		{
			CanonicalCapabilityIds.Add(CanonicalTag.ToString());
		}

		TArray<FGameplayTag> CapabilityTagArray;
		Workbench->WorkbenchTags.GetGameplayTagArray(CapabilityTagArray);
		for (const FGameplayTag& CapabilityTag : CapabilityTagArray)
		{
			if (!CapabilityTag.IsValid())
			{
				continue;
			}

			const FString CapabilityId = CapabilityTag.ToString();
			if (CapabilityId.IsEmpty())
			{
				continue;
			}

			AdditionalCapabilityIds.Add(CapabilityId);
		}
	}

	TArray<FString> CanonicalIds = CanonicalCapabilityIds.Array();
	CanonicalIds.Sort();
	for (const FString& CapabilityId : CanonicalIds)
	{
		OutCapabilityIds.Add(CapabilityId);
	}
	TArray<FString> AdditionalIds = AdditionalCapabilityIds.Array();
	AdditionalIds.Sort();
	for (const FString& CapabilityId : AdditionalIds)
	{
		if (OutCapabilityIds.Num() >= MaxNearbyWorkbenches)
		{
			break;
		}
		OutCapabilityIds.AddUnique(CapabilityId);
	}
	OutCapabilityIds.Sort();
	return true;
}

AAI_REWorkBenchBase* FAIRECompanionWorkbenchQuery::FindNearestCompatible(
	const AActor& Origin,
	const EWorkbenchType RequiredType,
	const float RadiusCentimeters)
{
	if (RequiredType == EWorkbenchType::None)
	{
		return nullptr;
	}

	TArray<TWeakObjectPtr<AAI_REWorkBenchBase>> Workbenches;
	if (!CollectNearbyWorkbenches(Origin, Workbenches, RadiusCentimeters))
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<AAI_REWorkBenchBase>& WorkbenchReference : Workbenches)
	{
		AAI_REWorkBenchBase* Workbench = WorkbenchReference.Get();
		if (IsValidWorkbench(Workbench)
			&& Workbench->WorkbenchType == RequiredType)
		{
			return Workbench;
		}
	}

	return nullptr;
}

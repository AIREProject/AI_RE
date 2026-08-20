#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AAI_REHarvestableResourceActor;
class AActor;

class AI_RE_API FAIRECompanionHarvestableResourceQuery final
{
public:
	static constexpr float DefaultRadiusCentimeters = 5000.0f;
	static constexpr int32 MaxNearbyResources = 8;

	static bool CollectNearbyResources(
		const AActor& Origin,
		TArray<TWeakObjectPtr<AAI_REHarvestableResourceActor>>& OutResources,
		float RadiusCentimeters = DefaultRadiusCentimeters);

	static AAI_REHarvestableResourceActor* FindNearestCompatible(
		const AActor& Origin,
		FGameplayTag RequiredResourceTag,
		float RadiusCentimeters = DefaultRadiusCentimeters);

	static bool GetNearbyWoodCount(
		const AActor& Origin,
		int32& OutCount,
		float RadiusCentimeters = DefaultRadiusCentimeters);
};

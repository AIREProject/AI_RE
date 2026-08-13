#pragma once

#include "CoreMinimal.h"
#include "AI_RECraftingTypes.h"

class AAI_REWorkBenchBase;
class AActor;

/**
 * Stateless, bounded awareness queries for Companion workbenches.
 *
 * Results are non-owning weak references. Callers must re-check object validity
 * before using a workbench because an Actor can be destroyed after a query.
 */
class AI_RE_API FAIRECompanionWorkbenchQuery final
{
public:
	static constexpr float DefaultRadiusCentimeters = 5000.0f;
	static constexpr int32 MaxNearbyWorkbenches = 8;

	/**
	 * Collects up to MaxNearbyWorkbenches valid workbench Actors around Origin.
	 * The output is ordered from nearest to farthest. A true result means the
	 * bounded overlap query was executed; the output may still be empty.
	 * Radius values must be in (0, DefaultRadiusCentimeters].
	 */
	static bool CollectNearbyWorkbenches(
		const AActor& Origin,
		TArray<TWeakObjectPtr<AAI_REWorkBenchBase>>& OutWorkbenches,
		float RadiusCentimeters = DefaultRadiusCentimeters);

	/**
	 * Collects unique stable Gameplay Tag strings advertised by nearby
	 * workbenches. Canonical tags are derived from EWorkbenchType and are added
	 * alongside any native tags configured on the workbench Actor.
	 */
	static bool GetNearbyCapabilityIds(
		const AActor& Origin,
		TArray<FString>& OutCapabilityIds,
		float RadiusCentimeters = DefaultRadiusCentimeters);

	/**
	 * Returns the nearest valid workbench whose EWorkbenchType matches the
	 * requested type, or nullptr when no compatible workbench is nearby.
	 */
	static AAI_REWorkBenchBase* FindNearestCompatible(
		const AActor& Origin,
		EWorkbenchType RequiredType,
		float RadiusCentimeters = DefaultRadiusCentimeters);
};

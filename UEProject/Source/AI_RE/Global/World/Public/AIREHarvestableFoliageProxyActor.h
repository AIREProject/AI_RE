#pragma once

#include "CoreMinimal.h"
#include "AI_REHarvestableResourceActor.h"
#include "AIREHarvestableFoliageProxyActor.generated.h"

class UAI_REItemDataAsset;
class UStaticMesh;
class AAI_REItemActor;

UCLASS()
class AI_RE_API AAIREHarvestableFoliageProxyActor
	: public AAI_REHarvestableResourceActor
{
	GENERATED_BODY()

public:
	void Configure(
		UStaticMesh* SourceMesh,
		UAI_REItemDataAsset* RewardItem,
		TSubclassOf<AAI_REItemActor> DroppedItemClass);
};

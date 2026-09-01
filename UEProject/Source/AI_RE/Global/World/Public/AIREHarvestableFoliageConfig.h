#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AIREHarvestableFoliageConfig.generated.h"

class AAI_REItemActor;
class UFoliageType;
class UStaticMesh;
class UAI_REItemDataAsset;

USTRUCT(BlueprintType)
struct AI_RE_API FAIREHarvestableFoliageTypeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UFoliageType> FoliageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> SourceMesh;
};

UCLASS(BlueprintType)
class AI_RE_API UAIREHarvestableFoliageConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Foliage")
	TArray<FAIREHarvestableFoliageTypeConfig> Types;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Foliage")
	TObjectPtr<UAI_REItemDataAsset> RewardItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Foliage")
	TSubclassOf<AAI_REItemActor> DroppedItemClass;
};

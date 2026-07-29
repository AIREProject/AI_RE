// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "AI_REItemTypes.h"
#include "AI_REItemDataAsset.generated.h"

class UTexture2D;
class UStaticMesh;

/**
 * Base PrimaryDataAsset for all items in the game.
 * Used to store static data like Icons, Names, Descriptions, and Meshes.
 */
UCLASS(BlueprintType)
class AI_RE_API UAI_REItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// The core Item ID used to match with Inventory and Crafting tables (e.g., "StoneAxe")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	FName ItemId;

	// In-game display name
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	FText DisplayName;

	// In-game lore/description
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data", meta = (MultiLine = "true"))
	FText Description;

	// Type of item for sorting/filtering
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	EAI_REItemType ItemType = EAI_REItemType::Resource;

	// Maximum amount this item can stack in a single inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data", meta = (ClampMin = "1"))
	int32 MaxStackSize = 99;

	// 2D Icon used in UI (Inventory, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UTexture2D> ItemIcon;

	// Larger or specific 2D image used in Crafting UI
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UTexture2D> CraftingImage;

	// 3D Mesh used when the item is dropped in the world
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UStaticMesh> WorldMesh;
	
	// Utility function to get the primary asset id for async loading if needed
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FName("ItemData"), GetFName());
	}
};

/**
 * DataTable Row struct to map an ItemId (RowName) to its corresponding DataAsset.
 */
USTRUCT(BlueprintType)
struct FAI_REItemDataMapping : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	TObjectPtr<UAI_REItemDataAsset> ItemAsset;
};

// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AI_RECraftingTypes.generated.h"

/**
 * Type of workbench required to craft a recipe
 */
UENUM(BlueprintType)
enum class EWorkbenchType : uint8
{
	None		UMETA(DisplayName = "None (Handcraft)"),
	Basic		UMETA(DisplayName = "Basic Workbench"),
	Blacksmith	UMETA(DisplayName = "Blacksmith Anvil/Furnace"),
	Alchemy		UMETA(DisplayName = "Alchemy Table"),
	Cook		UMETA(DisplayName = "Cooking Station")
};

/**
 * Single ingredient required for a crafting recipe
 */
USTRUCT(BlueprintType)
struct FAI_RECraftingIngredient
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	int32 Amount = 1;
};

/**
 * Data-driven recipe structure for crafting
 */
USTRUCT(BlueprintType)
struct FAI_RECraftingRecipe : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	FName ResultItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	int32 ResultAmount = 1;

	// The workbench required to craft this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	EWorkbenchType RequiredWorkbench = EWorkbenchType::None;

	// Set to 0 for instant crafting (useful for demos), > 0 for delayed crafting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	float CraftingTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TArray<FAI_RECraftingIngredient> Ingredients;
};

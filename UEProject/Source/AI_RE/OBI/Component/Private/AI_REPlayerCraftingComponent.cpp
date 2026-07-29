// Copyright MixUpProject. All Rights Reserved.

#include "AI_REPlayerCraftingComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

UAI_REPlayerCraftingComponent::UAI_REPlayerCraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// C++에서 데이터 테이블을 강제로 연결하여 블루프린트 초기화 버그 방지
	static ConstructorHelpers::FObjectFinder<UDataTable> DT_Recipes(TEXT("DataTable'/Game/Work/OBI/Datas/DT_crafting_recipes.DT_crafting_recipes'"));
	if (DT_Recipes.Succeeded())
	{
		CraftingRecipeTable = DT_Recipes.Object;
	}
}

void UAI_REPlayerCraftingComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UAI_REPlayerCraftingComponent::CanCraftRecipe(FName RecipeRowName) const
{
	if (!CraftingRecipeTable || !CachedInventory)
	{
		return false;
	}

	FAI_RECraftingRecipe* Recipe = CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(RecipeRowName, TEXT("Crafting Check"));
	if (!Recipe)
	{
		return false;
	}

	// Check if we have enough of each ingredient
	for (const FAI_RECraftingIngredient& Ingredient : Recipe->Ingredients)
	{
		if (!CachedInventory->HasItem(Ingredient.ItemId, Ingredient.Amount))
		{
			return false; // Missing ingredient
		}
	}

	// Optionally check if inventory is full before crafting, but since we consume ingredients first, 
	// it usually frees up space. For now, just return true.
	return true;
}

bool UAI_REPlayerCraftingComponent::StartCrafting(FName RecipeRowName)
{
	if (bIsCrafting)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("[Crafting] 이미 다른 아이템을 제작 중입니다!"));
		return false;
	}
	if (!CanCraftRecipe(RecipeRowName))
	{
		FAI_RECraftingRecipe* Recipe = CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(RecipeRowName, TEXT("Start Crafting"));
		if (Recipe && CachedInventory)
		{
			for (const FAI_RECraftingIngredient& Ingredient : Recipe->Ingredients)
			{
				if (!CachedInventory->HasItem(Ingredient.ItemId, Ingredient.Amount))
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("[Crafting] 재료 부족: %s (필요: %d)"), *Ingredient.ItemId.ToString(), Ingredient.Amount));
				}
			}
		}
		return false;
	}

	FAI_RECraftingRecipe* Recipe = CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(RecipeRowName, TEXT("Start Crafting"));
	if (!Recipe)
	{
		return false;
	}

	// 1. Consume all ingredients
	for (const FAI_RECraftingIngredient& Ingredient : Recipe->Ingredients)
	{
		CachedInventory->ConsumeItem(Ingredient.ItemId, Ingredient.Amount);
	}

	bIsCrafting = true;
	CurrentRecipeName = RecipeRowName;

	// 2. Handle Instant vs Delayed Crafting
	if (Recipe->CraftingTime <= 0.0f)
	{
		// Instant crafting
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("[Crafting] 즉시 제작 완료!"));
		CompleteCrafting();
	}
	else
	{
		// Delayed crafting
		GEngine->AddOnScreenDebugMessage(-1, Recipe->CraftingTime, FColor::Yellow, FString::Printf(TEXT("[Crafting] 제작 시작... %.1f초 대기"), Recipe->CraftingTime));
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CraftingTimerHandle, this, &UAI_REPlayerCraftingComponent::CompleteCrafting, Recipe->CraftingTime, false);
		}
	}

	return true;
}

TArray<FName> UAI_REPlayerCraftingComponent::GetRecipesByWorkbench(EWorkbenchType WorkbenchType) const
{
	TArray<FName> Result;
	if (!CraftingRecipeTable) return Result;

	TArray<FName> RowNames = CraftingRecipeTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FAI_RECraftingRecipe* Recipe = CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(RowName, TEXT("GetRecipesByWorkbench"));
		if (Recipe && Recipe->RequiredWorkbench == WorkbenchType)
		{
			Result.Add(RowName);
		}
	}
	return Result;
}

void UAI_REPlayerCraftingComponent::CancelCrafting()
{
	if (!bIsCrafting) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CraftingTimerHandle);
	}

	// Refund ingredients
	if (CraftingRecipeTable && CachedInventory)
	{
		FAI_RECraftingRecipe* Recipe = CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(CurrentRecipeName, TEXT("Cancel Crafting"));
		if (Recipe)
		{
			for (const FAI_RECraftingIngredient& Ingredient : Recipe->Ingredients)
			{
				CachedInventory->AddItem(Ingredient.ItemId, Ingredient.Amount);
			}
		}
	}

	bIsCrafting = false;
	CurrentRecipeName = NAME_None;
}

void UAI_REPlayerCraftingComponent::CompleteCrafting()
{
	if (!bIsCrafting || !CraftingRecipeTable || !CachedInventory)
	{
		bIsCrafting = false;
		return;
	}

	FAI_RECraftingRecipe* Recipe = CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(CurrentRecipeName, TEXT("Complete Crafting"));
	if (Recipe)
	{
		// Add result item
		CachedInventory->AddItem(Recipe->ResultItemId, Recipe->ResultAmount);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[Crafting] 제작 완료! 획득: %s x%d"), *Recipe->ResultItemId.ToString(), Recipe->ResultAmount));
		
		// Broadcast success
		OnCraftingCompletedEvent.Broadcast(Recipe->ResultItemId);
	}

	bIsCrafting = false;
	CurrentRecipeName = NAME_None;
}

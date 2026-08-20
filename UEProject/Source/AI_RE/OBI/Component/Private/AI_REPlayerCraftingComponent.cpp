// Copyright MixUpProject. All Rights Reserved.

#include "AI_REPlayerCraftingComponent.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AIREGameplayInventoryTypes.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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

	const FAI_RECraftingRecipe* Recipe =
		CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(
			RecipeRowName,
			TEXT("Crafting Check"));
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		GetInventorySubsystem();
	if (!Recipe || !IsValid(InventorySubsystem))
	{
		return false;
	}

	FAIREPlayerCraftRequest Request;
	if (!BuildCraftRequest(
			*Recipe,
			FGuid::NewGuid(),
			InventorySubsystem->GetInventorySessionId(),
			Request))
	{
		return false;
	}

	FAIREInventoryMutationResult Result;
	return InventorySubsystem->CanCompletePlayerCraft(
		CachedInventory.Get(),
		Request,
		Result);
}

bool UAI_REPlayerCraftingComponent::StartCrafting(FName RecipeRowName)
{
	if (bIsCrafting)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("[Crafting] 이미 다른 아이템을 제작 중입니다!"));
		return false;
	}
	if (!CraftingRecipeTable || !CachedInventory)
	{
		return false;
	}

	const FAI_RECraftingRecipe* Recipe =
		CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(
			RecipeRowName,
			TEXT("Start Crafting"));
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		GetInventorySubsystem();
	if (!Recipe || !IsValid(InventorySubsystem))
	{
		return false;
	}

	const FGuid CraftMutationId = FGuid::NewGuid();
	const FGuid CraftSessionId =
		InventorySubsystem->GetInventorySessionId();
	FAIREPlayerCraftRequest Request;
	FAIREInventoryMutationResult PreflightResult;
	if (!BuildCraftRequest(
			*Recipe,
			CraftMutationId,
			CraftSessionId,
			Request)
		|| !InventorySubsystem->CanCompletePlayerCraft(
			CachedInventory.Get(),
			Request,
			PreflightResult))
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.f,
			FColor::Red,
			TEXT("[Crafting] 재료가 부족하거나 결과를 보관할 공간이 없습니다."));
		return false;
	}

	bIsCrafting = true;
	CurrentRecipeName = RecipeRowName;
	CurrentCraftMutationId = CraftMutationId;
	CurrentCraftSessionId = CraftSessionId;

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
		else
		{
			ResetCraftingState();
			return false;
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

	ResetCraftingState();
}

void UAI_REPlayerCraftingComponent::CompleteCrafting()
{
	if (!bIsCrafting || !CraftingRecipeTable || !CachedInventory)
	{
		ResetCraftingState();
		return;
	}

	const FAI_RECraftingRecipe* Recipe =
		CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(
			CurrentRecipeName,
			TEXT("Complete Crafting"));
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		GetInventorySubsystem();
	FAIREPlayerCraftRequest Request;
	if (Recipe
		&& IsValid(InventorySubsystem)
		&& BuildCraftRequest(
			*Recipe,
			CurrentCraftMutationId,
			CurrentCraftSessionId,
			Request))
	{
		const FAIREInventoryMutationResult Result =
			InventorySubsystem->TryCompletePlayerCraft(
				CachedInventory.Get(),
				Request);
		if (Result.WasApplied())
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				5.f,
				FColor::Green,
				FString::Printf(
					TEXT("[Crafting] 제작 완료! 획득: %s x%d"),
					*Recipe->ResultItemId.ToString(),
					Recipe->ResultAmount));
			OnCraftingCompletedEvent.Broadcast(Recipe->ResultItemId);
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				5.f,
				FColor::Red,
				TEXT("[Crafting] 완료 시점에 재료 또는 결과 공간이 부족해 취소되었습니다."));
		}
	}

	ResetCraftingState();
}

bool UAI_REPlayerCraftingComponent::BuildCraftRequest(
	const FAI_RECraftingRecipe& Recipe,
	const FGuid& MutationId,
	const FGuid& SessionId,
	FAIREPlayerCraftRequest& OutRequest) const
{
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		GetInventorySubsystem();
	if (!IsValid(InventorySubsystem))
	{
		return false;
	}

	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!InventorySubsystem->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			StorageSnapshot)
		|| StorageSnapshot.SessionId != SessionId)
	{
		return false;
	}

	OutRequest = FAIREPlayerCraftRequest();
	OutRequest.SessionId = SessionId;
	OutRequest.MutationId = MutationId;
	OutRequest.ExpectedStorageRevision = StorageSnapshot.Revision;
	OutRequest.Result.ItemId = Recipe.ResultItemId;
	OutRequest.Result.Count = Recipe.ResultAmount;
	OutRequest.Ingredients.Reserve(Recipe.Ingredients.Num());
	for (const FAI_RECraftingIngredient& Ingredient : Recipe.Ingredients)
	{
		FAIREInventoryItemQuantity& Quantity =
			OutRequest.Ingredients.AddDefaulted_GetRef();
		Quantity.ItemId = Ingredient.ItemId;
		Quantity.Count = Ingredient.Amount;
	}
	return true;
}

UAIREGameplayInventorySubsystem*
UAI_REPlayerCraftingComponent::GetInventorySubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World)
		? World->GetGameInstance()
		: nullptr;
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
		: nullptr;
}

void UAI_REPlayerCraftingComponent::ResetCraftingState()
{
	bIsCrafting = false;
	CurrentRecipeName = NAME_None;
	CurrentCraftMutationId.Invalidate();
	CurrentCraftSessionId.Invalidate();
}

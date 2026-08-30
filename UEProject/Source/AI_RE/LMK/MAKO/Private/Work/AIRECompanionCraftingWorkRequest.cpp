#include "Work/AIRECompanionCraftingWorkRequest.h"

#include "AI_RECraftingTypes.h"
#include "AIREGameplayInventorySubsystem.h"
#include "AIREGameplayInventoryTypes.h"
#include "AI_REWorkBenchBase.h"
#include "Core/AIRECompanionCharacter.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "Work/AIRECompanionWorkOrderTypes.h"

bool FAIRECompanionCraftingWorkRequest::IsValidRequestInputs(
	const AActor* WorkTarget,
	const UDataTable* RecipeTable,
	FName RecipeRowId)
{
	if (!IsValid(WorkTarget)
		|| WorkTarget->IsActorBeingDestroyed()
		|| !IsValid(RecipeTable)
		|| RecipeRowId.IsNone()
		|| RecipeTable->GetRowStruct() != FAI_RECraftingRecipe::StaticStruct())
	{
		return false;
	}

	const FAI_RECraftingRecipe* Recipe = RecipeTable->FindRow<FAI_RECraftingRecipe>(
		RecipeRowId,
		TEXT("AIRECompanionCraftingWorkRequest"),
		false);
	const AAI_REWorkBenchBase* Workbench =
		Cast<AAI_REWorkBenchBase>(WorkTarget);
	const bool bValidHandcraftTarget =
		Recipe != nullptr
		&& Recipe->RequiredWorkbench == EWorkbenchType::None
		&& Workbench == nullptr;
	const bool bValidWorkbenchTarget =
		Recipe != nullptr
		&& Recipe->RequiredWorkbench != EWorkbenchType::None
		&& IsValid(Workbench)
		&& Workbench->WorkbenchType == Recipe->RequiredWorkbench;
	if (Recipe == nullptr
		|| (!bValidHandcraftTarget && !bValidWorkbenchTarget)
		|| Recipe->ResultItemId.IsNone()
		|| Recipe->ResultAmount <= 0
		|| !FMath::IsFinite(Recipe->CraftingTime)
		|| Recipe->CraftingTime < 0.0f
		|| Recipe->Ingredients.IsEmpty())
	{
		return false;
	}

	for (const FAI_RECraftingIngredient& Ingredient : Recipe->Ingredients)
	{
		if (Ingredient.ItemId.IsNone() || Ingredient.Amount <= 0)
		{
			return false;
		}
	}

	return true;
}

bool FAIRECompanionCraftingWorkRequest::TryRequest(
	UAIRECompanionWorkOrderComponent* WorkOrderComponent,
	AActor* WorkTarget,
	UDataTable* RecipeTable,
	FName RecipeRowId,
	FGuid& OutWorkOrderId,
	const bool bRequireMakoDestination)
{
	OutWorkOrderId.Invalidate();
	if (!IsValid(WorkOrderComponent)
		|| WorkOrderComponent->HasActiveWorkOrder()
		|| !IsValidRequestInputs(WorkTarget, RecipeTable, RecipeRowId))
	{
		return false;
	}

	FAIRECompanionWorkOrderRequest Request;
	Request.WorkType = EAIRECompanionWorkOrderType::Crafting;
	Request.TargetActor = WorkTarget;
	Request.RecipeTable = RecipeTable;
	Request.RecipeRowId = RecipeRowId;
	Request.bRequireMakoDestination = bRequireMakoDestination;
	return WorkOrderComponent->TryRequestWorkOrder(Request, OutWorkOrderId);
}

bool FAIRECompanionCraftingWorkRequest::BuildInventoryWorkRequest(
	AAIRECompanionCharacter& CompanionCharacter,
	UAIRECompanionInventoryComponent& InventoryComponent,
	const FGuid& WorkOrderId,
	const FAI_RECraftingRecipe& Recipe,
	const bool bCanWorldDrop,
	FAIREMakoCraftWorkRequest& OutRequest)
{
	FAIREInventoryContainerSnapshot MakoSnapshot;
	if (!WorkOrderId.IsValid()
		|| !InventoryComponent.GetInventorySnapshot(MakoSnapshot))
	{
		return false;
	}

	const UWorld* World = CompanionCharacter.GetWorld();
	const UGameInstance* GameInstance =
		IsValid(World) ? World->GetGameInstance() : nullptr;
	UAIREGameplayInventorySubsystem* GameplayInventory =
		IsValid(GameInstance)
			? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
			: nullptr;
	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!IsValid(GameplayInventory)
		|| !GameplayInventory->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			StorageSnapshot)
		|| MakoSnapshot.SessionId != StorageSnapshot.SessionId)
	{
		return false;
	}

	OutRequest = FAIREMakoCraftWorkRequest();
	OutRequest.SessionId = MakoSnapshot.SessionId;
	OutRequest.WorkOrderId = WorkOrderId;
	OutRequest.ExpectedMakoRevision = MakoSnapshot.Revision;
	OutRequest.ExpectedStorageRevision = StorageSnapshot.Revision;
	OutRequest.Result.ItemId = Recipe.ResultItemId;
	OutRequest.Result.Count = Recipe.ResultAmount;
	OutRequest.bCanWorldDrop = bCanWorldDrop;
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

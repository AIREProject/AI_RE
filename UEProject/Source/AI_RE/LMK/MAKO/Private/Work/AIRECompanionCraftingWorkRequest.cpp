#include "Work/AIRECompanionCraftingWorkRequest.h"

#include "AI_RECraftingTypes.h"
#include "AI_REWorkBenchBase.h"
#include "Engine/DataTable.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "Work/AIRECompanionWorkOrderTypes.h"

bool FAIRECompanionCraftingWorkRequest::IsValidRequestInputs(
	const AAI_REWorkBenchBase* Workbench,
	const UDataTable* RecipeTable,
	FName RecipeRowId)
{
	if (!IsValid(Workbench)
		|| Workbench->IsActorBeingDestroyed()
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
	if (Recipe == nullptr
		|| Recipe->RequiredWorkbench == EWorkbenchType::None
		|| Workbench->WorkbenchType == EWorkbenchType::None
		|| Recipe->RequiredWorkbench != Workbench->WorkbenchType
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
	AAI_REWorkBenchBase* Workbench,
	UDataTable* RecipeTable,
	FName RecipeRowId,
	FGuid& OutWorkOrderId)
{
	OutWorkOrderId.Invalidate();
	if (!IsValid(WorkOrderComponent)
		|| WorkOrderComponent->HasActiveWorkOrder()
		|| !IsValidRequestInputs(Workbench, RecipeTable, RecipeRowId))
	{
		return false;
	}

	FAIRECompanionWorkOrderRequest Request;
	Request.WorkType = EAIRECompanionWorkOrderType::Crafting;
	Request.TargetActor = Workbench;
	Request.RecipeTable = RecipeTable;
	Request.RecipeRowId = RecipeRowId;
	return WorkOrderComponent->TryRequestWorkOrder(Request, OutWorkOrderId);
}

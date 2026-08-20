#pragma once

#include "CoreMinimal.h"

class AAI_REWorkBenchBase;
class AAIRECompanionCharacter;
class UAIRECompanionInventoryComponent;
class UAIRECompanionWorkOrderComponent;
class UDataTable;
struct FAI_RECraftingRecipe;
struct FAIREMakoCraftWorkRequest;

class AI_RE_API FAIRECompanionCraftingWorkRequest final
{
public:
	static bool IsValidRequestInputs(
		const AAI_REWorkBenchBase* Workbench,
		const UDataTable* RecipeTable,
		FName RecipeRowId);

	static bool TryRequest(
		UAIRECompanionWorkOrderComponent* WorkOrderComponent,
		AAI_REWorkBenchBase* Workbench,
		UDataTable* RecipeTable,
		FName RecipeRowId,
		FGuid& OutWorkOrderId,
		bool bRequireMakoDestination = false);

	static bool BuildInventoryWorkRequest(
		AAIRECompanionCharacter& CompanionCharacter,
		UAIRECompanionInventoryComponent& InventoryComponent,
		const FGuid& WorkOrderId,
		const FAI_RECraftingRecipe& Recipe,
		bool bCanWorldDrop,
		FAIREMakoCraftWorkRequest& OutRequest);
};

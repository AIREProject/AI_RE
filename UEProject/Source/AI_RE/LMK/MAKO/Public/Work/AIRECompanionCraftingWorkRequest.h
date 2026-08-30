#pragma once

#include "CoreMinimal.h"

class AActor;
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
		const AActor* WorkTarget,
		const UDataTable* RecipeTable,
		FName RecipeRowId);

	static bool TryRequest(
		UAIRECompanionWorkOrderComponent* WorkOrderComponent,
		AActor* WorkTarget,
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

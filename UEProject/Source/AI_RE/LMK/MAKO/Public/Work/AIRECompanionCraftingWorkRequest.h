#pragma once

#include "CoreMinimal.h"

class AAI_REWorkBenchBase;
class UAIRECompanionWorkOrderComponent;
class UDataTable;

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
		FGuid& OutWorkOrderId);
};

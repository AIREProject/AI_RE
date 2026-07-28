// Copyright MixUpProject. All Rights Reserved.

#include "AI_REItemSubsystem.h"
#include "Engine/DataTable.h"

void UAI_REItemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Normally you'd load the DataTable reference from Project Settings or an Asset Manager here.
	// For now, it will be set by the GameMode or UI during initialization.
}

void UAI_REItemSubsystem::SetItemDataTable(UDataTable* InTable)
{
	if (InTable)
	{
		ItemDataTable = InTable;
		
		// Clear old cache
		ItemCache.Empty();
		
		// Pre-cache all items for fast lookup O(1)
		TArray<FAI_REItemDataMapping*> AllRows;
		ItemDataTable->GetAllRows<FAI_REItemDataMapping>(TEXT("ItemSubsystem"), AllRows);
		
		TArray<FName> RowNames = ItemDataTable->GetRowNames();
		for (int32 i = 0; i < AllRows.Num(); ++i)
		{
			if (AllRows[i] && AllRows[i]->ItemAsset)
			{
				// Cache by RowName (which is the ItemId)
				ItemCache.Add(RowNames[i], AllRows[i]->ItemAsset);
			}
		}
	}
}

UAI_REItemDataAsset* UAI_REItemSubsystem::GetItemDataAsset(FName ItemId) const
{
	if (ItemId.IsNone())
		return nullptr;

	if (UAI_REItemDataAsset* const* FoundAsset = ItemCache.Find(ItemId))
	{
		return *FoundAsset;
	}

	return nullptr;
}

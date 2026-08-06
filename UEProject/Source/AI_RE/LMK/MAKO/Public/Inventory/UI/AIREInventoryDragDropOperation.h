#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Inventory/UI/AIREInventorySlotWidget.h"
#include "AIREInventoryDragDropOperation.generated.h"

UCLASS()
class AI_RE_API UAIREInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|DragDrop")
	EAIREInventorySlotSource Source = EAIREInventorySlotSource::Player;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|DragDrop")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|DragDrop")
	FName ItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|DragDrop")
	int32 ItemCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|DragDrop")
	bool bExactQuantityRequested = false;
};

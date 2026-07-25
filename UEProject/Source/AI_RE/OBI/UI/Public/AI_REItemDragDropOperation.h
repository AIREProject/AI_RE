#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "AI_REItemDragDropOperation.generated.h"

class UAI_REInventorySlotUI;

UCLASS()
class AI_RE_API UAI_REItemDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DragDrop", meta = (ExposeOnSpawn = "true"))
	int32 SourceSlotIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DragDrop", meta = (ExposeOnSpawn = "true"))
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DragDrop", meta = (ExposeOnSpawn = "true"))
	int32 ItemCount = 0;

	// UI reference for visual updates
	UPROPERTY()
	TObjectPtr<UAI_REInventorySlotUI> SourceSlotWidget;
};

#include "Inventory/UI/AIREInventorySlotWidget.h"

#include "AI_REItemDataAsset.h"
#include "AI_REItemSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Inventory/UI/AIREInventoryDragDropOperation.h"

void UAIREInventorySlotWidget::SetSlotData(
	const EAIREInventorySlotSource InSource,
	const int32 InSlotIndex,
	const FName InItemId,
	const int32 InCount,
	const bool bInEquipped,
	const bool bInPending,
	const EAIREEquipmentTransitionState InState)
{
	Source = InSource;
	SlotIndex = InSlotIndex;
	ItemId = InItemId;
	ItemCount = InCount;
	bEquipped = bInEquipped;
	bPending = bInPending;
	EquipmentState = InState;
	RefreshVisuals();
}

void UAIREInventorySlotWidget::SetSelected(const bool bInSelected)
{
	if (IsValid(SelectionBorder))
	{
		SelectionBorder->SetVisibility(
			bInSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

EAIREInventorySlotSource UAIREInventorySlotWidget::GetSource() const
{
	return Source;
}

int32 UAIREInventorySlotWidget::GetSlotIndex() const
{
	return SlotIndex;
}

FName UAIREInventorySlotWidget::GetItemId() const
{
	return ItemId;
}

int32 UAIREInventorySlotWidget::GetItemCount() const
{
	return ItemCount;
}

FAIREInventorySlotClicked& UAIREInventorySlotWidget::OnSlotClicked()
{
	return SlotClicked;
}

FAIREInventorySlotDragStarted& UAIREInventorySlotWidget::OnSlotDragStarted()
{
	return SlotDragStarted;
}

FAIREInventorySlotDropped& UAIREInventorySlotWidget::OnSlotDropped()
{
	return SlotDropped;
}

FReply UAIREInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		SlotClicked.ExecuteIfBound(this);
		return UWidgetBlueprintLibrary::DetectDragIfPressed(
			InMouseEvent,
			this,
			EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UAIREInventorySlotWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	(void)InGeometry;
	if (ItemId.IsNone() || ItemCount <= 0)
	{
		return;
	}

	UAIREInventoryDragDropOperation* Operation =
		NewObject<UAIREInventoryDragDropOperation>();
	Operation->Source = Source;
	Operation->SourceSlotIndex = SlotIndex;
	Operation->ItemId = ItemId;
	Operation->ItemCount = ItemCount;
	Operation->bExactQuantityRequested = InMouseEvent.IsShiftDown();
	SlotDragStarted.ExecuteIfBound(Operation);
	OutOperation = Operation;
}

bool UAIREInventorySlotWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	if (UAIREInventoryDragDropOperation* Operation =
		Cast<UAIREInventoryDragDropOperation>(InOperation))
	{
		SlotDropped.ExecuteIfBound(Operation, this);
		return true;
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UAIREInventorySlotWidget::RefreshVisuals()
{
	const bool bHasItem = !ItemId.IsNone() && ItemCount > 0;
	const UAI_REItemDataAsset* Definition = nullptr;
	if (bHasItem)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (const UAI_REItemSubsystem* Items =
					GameInstance->GetSubsystem<UAI_REItemSubsystem>())
			{
				Definition = Items->GetItemDataAsset(ItemId);
			}
		}
	}

	if (IsValid(ItemNameText))
	{
		ItemNameText->SetText(
			!bHasItem
				? FText::GetEmpty()
				: IsValid(Definition)
					? Definition->DisplayName
					: FText::FromName(ItemId));
	}
	if (IsValid(ItemCountText))
	{
		ItemCountText->SetText(
			bHasItem
				? FText::AsNumber(ItemCount)
				: FText::GetEmpty());
	}
	if (IsValid(ItemIcon))
	{
		const bool bHasIcon =
			bHasItem
			&& IsValid(Definition)
			&& IsValid(Definition->ItemIcon);
		ItemIcon->SetBrushFromTexture(
			bHasIcon ? Definition->ItemIcon : nullptr);
		ItemIcon->SetVisibility(
			bHasIcon
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (IsValid(StateText))
	{
		FText StateLabel;
		if (EquipmentState == EAIREEquipmentTransitionState::RecoveryFailed)
		{
			StateLabel = FText::FromString(TEXT("RecoveryFailed"));
		}
		else if (bPending)
		{
			StateLabel = FText::FromString(TEXT("Pending"));
		}
		else if (bEquipped)
		{
			StateLabel = FText::FromString(TEXT("Equipped"));
		}
		StateText->SetText(StateLabel);
	}
}

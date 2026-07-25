// Fill out your copyright notice in the Description page of Project Settings.

#include "AI_REInventorySlotUI.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "AI_REItemDragDropOperation.h"
#include "AI_REPlayerInventoryComponent.h"

void UAI_REInventorySlotUI::RefreshSlot(const FName& InItemId, int32 InCount)
{
	CurrentItemId = InItemId;
	CurrentItemCount = InCount;

	if (InItemId.IsNone() || InCount <= 0)
	{
		if (ItemNameText) ItemNameText->SetText(FText::GetEmpty());
		if (ItemCountText) ItemCountText->SetText(FText::GetEmpty());
		if (ItemIcon) ItemIcon->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		if (ItemNameText) ItemNameText->SetText(FText::FromName(InItemId));
		if (ItemCountText) ItemCountText->SetText(FText::AsNumber(InCount));
		if (ItemIcon) ItemIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

FReply UAI_REInventorySlotUI::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !CurrentItemId.IsNone() && CurrentItemCount > 0)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UAI_REInventorySlotUI::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	UAI_REItemDragDropOperation* DragOp = NewObject<UAI_REItemDragDropOperation>(this);
	DragOp->SourceSlotIndex = SlotIndex;
	DragOp->ItemId = CurrentItemId;
	DragOp->ItemCount = CurrentItemCount;
	DragOp->SourceSlotWidget = this;
	
	// Create visual payload (현재 슬롯과 동일한 위젯을 생성해서 마우스에 붙임)
	if (UAI_REInventorySlotUI* DragVisual = CreateWidget<UAI_REInventorySlotUI>(this, GetClass()))
	{
		DragVisual->RefreshSlot(CurrentItemId, CurrentItemCount);
		DragOp->DefaultDragVisual = DragVisual;
	}
	
	OutOperation = DragOp;
	
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
}

bool UAI_REInventorySlotUI::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (UAI_REItemDragDropOperation* DragOp = Cast<UAI_REItemDragDropOperation>(InOperation))
	{
		if (InventoryComp && DragOp->SourceSlotIndex != SlotIndex)
		{
			if (bIsQuickSlot)
			{
				// 퀵슬롯이라면 인벤토리에서 자리를 옮기지 않고 정보만 복사
				RefreshSlot(DragOp->ItemId, DragOp->ItemCount);
			}
			else
			{
				// 일반 슬롯이라면 인벤토리 컴포넌트에 자리 이동 요청
				InventoryComp->MoveItemSlot(DragOp->SourceSlotIndex, SlotIndex);
			}
			return true;
		}
	}
	
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

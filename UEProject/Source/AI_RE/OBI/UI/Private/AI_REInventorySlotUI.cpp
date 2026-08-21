// Fill out your copyright notice in the Description page of Project Settings.

#include "AI_REInventorySlotUI.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "AI_REItemDragDropOperation.h"
#include "AI_REPlayerInventoryComponent.h"
#include "AI_REItemSubsystem.h"
#include "AI_REItemDataAsset.h"
#include "AI_REPlayerCombatComponent.h"
#include "AIREGameplayInventorySubsystem.h"
#include "AIREGameplayInventoryTypes.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

void UAI_REInventorySlotUI::RefreshSlot(const FName& InItemId, int32 InCount)
{
	CurrentItemId = InItemId;
	CurrentItemCount = InCount;

	if (InItemId.IsNone() || InCount <= 0)
	{
		if (ItemNameText) ItemNameText->SetText(FText::GetEmpty());
		if (ItemCountText) ItemCountText->SetText(FText::GetEmpty());
		if (BackgroundIMG) 
		{
			// 빈 슬롯일 때는 숨기지 않고, 텍스처를 비운 뒤 반투명한 어두운 색으로 배경 유지
			BackgroundIMG->SetBrushFromTexture(nullptr);
			BackgroundIMG->SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 0.6f));
			BackgroundIMG->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else
	{
		// Subsystem을 통해 DataAsset 조회
		UAI_REItemDataAsset* DataAsset = nullptr;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UAI_REItemSubsystem* ItemSubsystem = GI->GetSubsystem<UAI_REItemSubsystem>())
			{
				DataAsset = ItemSubsystem->GetItemDataAsset(InItemId);
			}
		}

		if (DataAsset)
		{
			if (ItemNameText) ItemNameText->SetText(DataAsset->DisplayName);
			if (BackgroundIMG) 
			{
				BackgroundIMG->SetBrushFromTexture(DataAsset->ItemIcon);
				BackgroundIMG->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)); // 원래 색상 복구
				BackgroundIMG->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
		else
		{
			// DataAsset을 못 찾은 경우 임시 폴백
			if (ItemNameText) ItemNameText->SetText(FText::FromName(InItemId));
			if (BackgroundIMG) 
			{
				BackgroundIMG->SetBrushFromTexture(nullptr);
				BackgroundIMG->SetColorAndOpacity(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f)); // 에러: 빨간색
				BackgroundIMG->SetVisibility(ESlateVisibility::Visible);
			}
		}

		if (ItemCountText) ItemCountText->SetText(FText::AsNumber(InCount));
	}
}

FReply UAI_REInventorySlotUI::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsEquipmentSlot)
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !CurrentItemId.IsNone() && CurrentItemCount > 0)
		{
			return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
		}
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !CurrentItemId.IsNone() && CurrentItemCount > 0)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && !CurrentItemId.IsNone() && CurrentItemCount > 0)
	{
		if (InventoryComp)
		{
			InventoryComp->UseItem(SlotIndex);
			return FReply::Handled();
		}
	}
	
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UAI_REInventorySlotUI::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !CurrentItemId.IsNone() && CurrentItemCount > 0)
	{
		if (InventoryComp)
		{
			InventoryComp->UseItem(SlotIndex);
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
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
		if (bIsEquipmentSlot)
		{
			if (!InventoryComp || DragOp->ItemCount != 1)
			{
				return false;
			}

			UGameInstance* GameInstance = GetGameInstance();
			UAIREGameplayInventorySubsystem* GameplayInventory = GameInstance
				? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
				: nullptr;
			UAI_REPlayerCombatComponent* PlayerCombat = InventoryComp->GetOwner()
				? InventoryComp->GetOwner()->FindComponentByClass<UAI_REPlayerCombatComponent>()
				: nullptr;
			if (!GameplayInventory || !PlayerCombat)
			{
				return false;
			}

			FAIREPlayerWeaponEquipRequest Request;
			Request.SessionId = GameplayInventory->GetInventorySessionId();
			Request.MutationId = FGuid::NewGuid();
			Request.ExpectedPlayerRevision = InventoryComp->GetInventoryRevision();
			Request.SourceSlotIndex = DragOp->SourceSlotIndex;
			return GameplayInventory->TryEquipPlayerWeapon(
				InventoryComp,
				PlayerCombat,
				Request).WasApplied();
		}

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

// Fill out your copyright notice in the Description page of Project Settings.

#include "AI_REInventoryUI.h"
#include "AI_REInventorySlotUI.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Engine.h"

void UAI_REInventoryUI::InitializeInventory(UAI_REPlayerInventoryComponent* InInventoryComp)
{
	if (!InInventoryComp) return;

	InventoryComp = InInventoryComp;
	InventoryComp->OnInventoryChanged.AddUniqueDynamic(this, &UAI_REInventoryUI::RefreshInventory);

	if (InventoryGrid)
	{
		if (!SlotWidgetClass)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[UI Error] SlotWidgetClass is NULL! WBP_InventoryUI 디테일 패널에서 Slot Widget Class를 지정하세요."));
			return;
		}

		InventoryGrid->ClearChildren();
		SlotWidgets.Empty();

		for (int32 i = 0; i < InventoryComp->MaxSlots; ++i)
		{
			if (UAI_REInventorySlotUI* SlotUI = CreateWidget<UAI_REInventorySlotUI>(this, SlotWidgetClass))
			{
				SlotUI->SlotIndex = i;
				SlotUI->InventoryComp = InventoryComp.Get();
				
				UUniformGridSlot* GridSlot = InventoryGrid->AddChildToUniformGrid(SlotUI, i / Columns, i % Columns);
				if (GridSlot)
				{
					GridSlot->SetHorizontalAlignment(HAlign_Fill);
					GridSlot->SetVerticalAlignment(VAlign_Fill);
				}
				SlotWidgets.Add(SlotUI);
			}
		}
		
		FString DebugStr = FString::Printf(TEXT("[UI Success] Created %d slots! (MaxSlots: %d)"), SlotWidgets.Num(), InventoryComp->MaxSlots);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, DebugStr);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[UI Error] InventoryGrid is NULL! WBP_InventoryUI에서 UniformGridPanel의 이름을 'InventoryGrid'로 변경하고 IsVariable 체크하세요."));
	}



	RefreshInventory();
}

void UAI_REInventoryUI::RefreshInventory()
{
	if (!InventoryComp.IsValid()) return;

	// 1. 모든 슬롯을 빈 상태로 초기화
	for (UAI_REInventorySlotUI* SlotUI : SlotWidgets)
	{
		if (SlotUI)
		{
			SlotUI->RefreshSlot(NAME_None, 0);
		}
	}

	// 2. 인벤토리에 들어있는 아이템 데이터로 해당 슬롯만 덮어쓰기
	for (const FInventoryItemStack& Stack : InventoryComp->Items)
	{
		if (SlotWidgets.IsValidIndex(Stack.SlotIndex))
		{
			SlotWidgets[Stack.SlotIndex]->RefreshSlot(Stack.ItemId, Stack.Count);
		}
	}
}

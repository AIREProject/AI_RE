// Fill out your copyright notice in the Description page of Project Settings.


#include "AI_REMainUI.h"
#include "AI_RECharacter.h"
#include "AI_REStatePointBar.h"
#include "AI_REStatusComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "AI_REInventorySlotUI.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "GameFramework/Actor.h"

void UAI_REMainUI::InitializeHUD(UAI_REStatusComponent* InStatus)
{
	if (InStatus == nullptr) return; 
	
	InStatus -> OnHPChanged.AddUniqueDynamic(this, &UAI_REMainUI::UpdateHPBar);
	InStatus -> OnSPChanged.AddUniqueDynamic(this, &UAI_REMainUI::UpdateSPBar);
	InStatus -> OnHungerChanged.AddUniqueDynamic(this, &UAI_REMainUI::UpdateHungerBar);
	InStatus -> OnThirstyChanged.AddUniqueDynamic(this, &UAI_REMainUI::UpdateThirstyBar);
	
	// 모든 델리게이트 바인딩이 끝난 후 한 번에 스탯을 뿌려 UI를 초기화합니다.
	InStatus->BroadcastCurrentStats();

	if (QuickSlotGrid && SlotWidgetClass)
	{
		AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(InStatus->GetOwner());
		if (PlayerChar && PlayerChar->GetInventoryComponent())
		{
			UAI_REPlayerInventoryComponent* InvComp = PlayerChar->GetInventoryComponent();
			InventoryComp = InvComp;
			InvComp->OnInventoryChanged.AddUniqueDynamic(this, &UAI_REMainUI::RefreshQuickSlots);
			
			QuickSlotGrid->ClearChildren();
			QuickSlotWidgets.Empty();
			for (int32 i = 0; i < QuickSlotCount; ++i)
			{
				if (UAI_REInventorySlotUI* SlotUI = CreateWidget<UAI_REInventorySlotUI>(this, SlotWidgetClass))
				{
					SlotUI->SlotIndex = 100 + i; // 퀵슬롯은 100번대 인덱스를 가지는 독립적인 저장 공간
					SlotUI->InventoryComp = InvComp;
					SlotUI->bIsQuickSlot = false; // 일반 슬롯과 동일하게 작동하도록 처리 (드래그 시 실제 이동)
					
					UUniformGridSlot* GridSlot = QuickSlotGrid->AddChildToUniformGrid(SlotUI, 0, i);
					if (GridSlot)
					{
						GridSlot->SetHorizontalAlignment(HAlign_Fill);
						GridSlot->SetVerticalAlignment(VAlign_Fill);
					}
					QuickSlotWidgets.Add(SlotUI);
				}
			}
			RefreshQuickSlots();
		}
	}
}

void UAI_REMainUI::RefreshQuickSlots()
{
	if (!InventoryComp.IsValid()) return;

	// 모든 퀵슬롯 초기화
	for (UAI_REInventorySlotUI* SlotUI : QuickSlotWidgets)
	{
		if (SlotUI)
		{
			SlotUI->RefreshSlot(NAME_None, 0);
		}
	}

	// 퀵슬롯 인덱스(100 ~ 100+QuickSlotCount-1)에 해당하는 아이템만 업데이트
	for (const FInventoryItemStack& Stack : InventoryComp->Items)
	{
		int32 QuickIndex = Stack.SlotIndex - 100;
		if (QuickIndex >= 0 && QuickIndex < QuickSlotCount && QuickSlotWidgets.IsValidIndex(QuickIndex))
		{
			QuickSlotWidgets[QuickIndex]->RefreshSlot(Stack.ItemId, Stack.Count);
		}
	}
}

void UAI_REMainUI::UpdateHPBar(float Current, float Max)
{
	if (HPBar && Max > 0.f)
	{
		HPBar->SetTargetPercent(Current/Max);
	}
}

void UAI_REMainUI::UpdateSPBar(float Current, float Max)
{
	if (SPBar && Max > 0.f)
	{
		SPBar->SetTargetPercent(Current/Max);
	}
}

void UAI_REMainUI::UpdateHungerBar(float Current, float Max)
{
	if (HungerBar && Max > 0.f)
	{
		HungerBar->SetTargetPercent(Current/Max);
	}
}

void UAI_REMainUI::UpdateThirstyBar(float Current, float Max)
{
	if (ThirstyBar && Max > 0.f)
	{
		ThirstyBar->SetTargetPercent(Current/Max);
	}
}


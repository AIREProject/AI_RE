// Fill out your copyright notice in the Description page of Project Settings.


#include "AI_REMainUI.h"

#include "AI_REStatePointBar.h"
#include "AI_REStatusComponent.h"
#include "../../Component/Public/AI_REPlayerInventoryComponent.h"
#include "../Public/AI_REInventorySlotUI.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "GameFramework/Actor.h"


void UAI_REMainUI::InitializeHUD(UAI_REStatusComponent* InStatus)
{
	if (InStatus == nullptr) return; 
	
	InStatus -> OnHPChanged.AddDynamic(this, &UAI_REMainUI::UpdateHPBar);
	InStatus -> OnSPChanged.AddDynamic(this, &UAI_REMainUI::UpdateSPBar);
	InStatus -> OnHungerChanged.AddDynamic(this, &UAI_REMainUI::UpdateHungerBar);
	InStatus -> OnThirstyChanged.AddDynamic(this, &UAI_REMainUI::UpdateThirstyBar);
	
	// 초기화 시점에는 애니메이션 없이 즉시 값을 세팅 (InitializeHUD 방식)
	if (HPBar && InStatus->MaxHP > 0.f) HPBar->SetPercentInstantly(InStatus->CurrentHP / InStatus->MaxHP);
	if (SPBar && InStatus->MaxSP > 0.f) SPBar->SetPercentInstantly(InStatus->CurrentSP / InStatus->MaxSP);
	if (HungerBar && InStatus->MaxHunger > 0.f) HungerBar->SetPercentInstantly(InStatus->CurrentHunger / InStatus->MaxHunger);
	if (ThirstyBar && InStatus->MaxThirsty > 0.f) ThirstyBar->SetPercentInstantly(InStatus->CurrentThirsty / InStatus->MaxThirsty);

	if (QuickSlotGrid && SlotWidgetClass)
	{
		if (UAI_REPlayerInventoryComponent* InvComp = InStatus->GetOwner()->FindComponentByClass<UAI_REPlayerInventoryComponent>())
		{
			InventoryComp = InvComp;
			InvComp->OnInventoryChanged.AddDynamic(this, &UAI_REMainUI::RefreshQuickSlots);
			
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


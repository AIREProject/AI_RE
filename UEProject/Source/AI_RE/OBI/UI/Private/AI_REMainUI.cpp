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
#include "AbilitySystemComponent.h"
#include "AI_REAttributeSet.h"
#include "AbilitySystemInterface.h"

void UAI_REMainUI::InitializeHUD(UAI_REStatusComponent* InStatus)
{
	UnbindHUD();
	if (InStatus == nullptr) return; 
	
	AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(InStatus->GetOwner());
	if (PlayerChar)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PlayerChar))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				CachedASC = ASC;
				
				HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAI_REAttributeSet::GetHPAttribute()).AddUObject(this, &UAI_REMainUI::OnHealthAttributeChanged);
				SPChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAI_REAttributeSet::GetSPAttribute()).AddUObject(this, &UAI_REMainUI::OnSPAttributeChanged);
				HungerChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAI_REAttributeSet::GetHungerAttribute()).AddUObject(this, &UAI_REMainUI::OnHungerAttributeChanged);
				ThirstyChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAI_REAttributeSet::GetThirstyAttribute()).AddUObject(this, &UAI_REMainUI::OnThirstyAttributeChanged);
				
				// 초기 UI 세팅
				UpdateHPBar(ASC->GetNumericAttribute(UAI_REAttributeSet::GetHPAttribute()), ASC->GetNumericAttribute(UAI_REAttributeSet::GetMaxHPAttribute()));
				UpdateSPBar(ASC->GetNumericAttribute(UAI_REAttributeSet::GetSPAttribute()), ASC->GetNumericAttribute(UAI_REAttributeSet::GetMaxSPAttribute()));
				UpdateHungerBar(ASC->GetNumericAttribute(UAI_REAttributeSet::GetHungerAttribute()), ASC->GetNumericAttribute(UAI_REAttributeSet::GetMaxHungerAttribute()));
				UpdateThirstyBar(ASC->GetNumericAttribute(UAI_REAttributeSet::GetThirstyAttribute()), ASC->GetNumericAttribute(UAI_REAttributeSet::GetMaxThirstyAttribute()));
			}
		}
	}

	if (QuickSlotGrid && SlotWidgetClass)
	{
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

void UAI_REMainUI::NativeDestruct()
{
	UnbindHUD();
	QuickSlotWidgets.Reset();
	Super::NativeDestruct();
}

void UAI_REMainUI::UnbindHUD()
{
	if (InventoryComp.IsValid())
	{
		InventoryComp->OnInventoryChanged.RemoveDynamic(
			this,
			&UAI_REMainUI::RefreshQuickSlots);
	}
	InventoryComp.Reset();

	if (CachedASC.IsValid())
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			CachedASC->GetGameplayAttributeValueChangeDelegate(
				UAI_REAttributeSet::GetHPAttribute())
				.Remove(HealthChangedDelegateHandle);
		}
		if (SPChangedDelegateHandle.IsValid())
		{
			CachedASC->GetGameplayAttributeValueChangeDelegate(
				UAI_REAttributeSet::GetSPAttribute())
				.Remove(SPChangedDelegateHandle);
		}
		if (HungerChangedDelegateHandle.IsValid())
		{
			CachedASC->GetGameplayAttributeValueChangeDelegate(
				UAI_REAttributeSet::GetHungerAttribute())
				.Remove(HungerChangedDelegateHandle);
		}
		if (ThirstyChangedDelegateHandle.IsValid())
		{
			CachedASC->GetGameplayAttributeValueChangeDelegate(
				UAI_REAttributeSet::GetThirstyAttribute())
				.Remove(ThirstyChangedDelegateHandle);
		}
	}

	HealthChangedDelegateHandle.Reset();
	SPChangedDelegateHandle.Reset();
	HungerChangedDelegateHandle.Reset();
	ThirstyChangedDelegateHandle.Reset();
	CachedASC.Reset();
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

void UAI_REMainUI::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (CachedASC.IsValid())
	{
		UpdateHPBar(Data.NewValue, CachedASC->GetNumericAttribute(UAI_REAttributeSet::GetMaxHPAttribute()));
	}
}

void UAI_REMainUI::OnSPAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (CachedASC.IsValid())
	{
		UpdateSPBar(Data.NewValue, CachedASC->GetNumericAttribute(UAI_REAttributeSet::GetMaxSPAttribute()));
	}
}

void UAI_REMainUI::OnHungerAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (CachedASC.IsValid())
	{
		UpdateHungerBar(Data.NewValue, CachedASC->GetNumericAttribute(UAI_REAttributeSet::GetMaxHungerAttribute()));
	}
}

void UAI_REMainUI::OnThirstyAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (CachedASC.IsValid())
	{
		UpdateThirstyBar(Data.NewValue, CachedASC->GetNumericAttribute(UAI_REAttributeSet::GetMaxThirstyAttribute()));
	}
}


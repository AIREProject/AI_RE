// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AI_REInventorySlotUI.generated.h"

class UTextBlock;
class UImage;

UCLASS()
class AI_RE_API UAI_REInventorySlotUI : public UUserWidget
{
	GENERATED_BODY()

public:
	void RefreshSlot(const FName& InItemId, int32 InCount);
	
	// 슬롯의 인덱스를 부모가 세팅해줄 때 사용할 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 SlotIndex = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	FName CurrentItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 CurrentItemCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuickSlot")
	bool bIsQuickSlot = false;

	UPROPERTY()
	TObjectPtr<class UAI_REPlayerInventoryComponent> InventoryComp;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemCountText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HotkeyText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;
};

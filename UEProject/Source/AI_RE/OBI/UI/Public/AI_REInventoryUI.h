// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AI_REInventoryUI.generated.h"

class UAI_REPlayerInventoryComponent;
class UAI_REInventorySlotUI;
class UUniformGridPanel;

UCLASS()
class AI_RE_API UAI_REInventoryUI : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void InitializeInventory(UAI_REPlayerInventoryComponent* InInventoryComp);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> InventoryGrid;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> PlayerEquipmentGrid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UAI_REInventorySlotUI> SlotWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Columns = 5;

private:
	TWeakObjectPtr<UAI_REPlayerInventoryComponent> InventoryComp;
	
	UPROPERTY()
	TArray<UAI_REInventorySlotUI*> SlotWidgets;

	UPROPERTY()
	TObjectPtr<UAI_REInventorySlotUI> EquipmentSlotWidget;

	UFUNCTION()
	void RefreshInventory();

	UFUNCTION()
	void HandleWeaponEquipResult(FName WeaponItemId, bool bSucceeded);
};

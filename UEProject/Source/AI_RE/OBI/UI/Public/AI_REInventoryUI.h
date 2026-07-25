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
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> InventoryGrid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UAI_REInventorySlotUI> SlotWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Columns = 5;

private:
	TWeakObjectPtr<UAI_REPlayerInventoryComponent> InventoryComp;
	
	UPROPERTY()
	TArray<UAI_REInventorySlotUI*> SlotWidgets;

	UFUNCTION()
	void RefreshInventory();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AI_REMainUI.generated.h"

class UAI_REStatePointBar;
class UAI_REStatusComponent;
class UImage;
/**
 * 
 */

UCLASS()
class AI_RE_API UAI_REMainUI : public UUserWidget
{
	GENERATED_BODY()
	
private:
	UFUNCTION(BlueprintCallable)
	void UpdateHPBar(float Current, float Max);
	
	UFUNCTION(BlueprintCallable)
	void UpdateSPBar(float Current, float Max);
	
	UFUNCTION(BlueprintCallable)
	void UpdateHungerBar(float Current, float Max);

	UFUNCTION(BlueprintCallable)
	void UpdateThirstyBar(float Current, float Max);

	UFUNCTION()
	void RefreshQuickSlots();
	
	// 타이머를 켜고 끌 때 필요한 리모콘(핸들)
	FTimerHandle HPSmoothTimerHandle;

	TWeakObjectPtr<class UAI_REPlayerInventoryComponent> InventoryComp;

	UPROPERTY()
	TArray<class UAI_REInventorySlotUI*> QuickSlotWidgets;
	
protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAI_REStatePointBar> HPBar;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAI_REStatePointBar> SPBar;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAI_REStatePointBar> HungerBar;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAI_REStatePointBar> ThirstyBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UUniformGridPanel> QuickSlotGrid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QuickSlot")
	TSubclassOf<class UAI_REInventorySlotUI> SlotWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot")
	int32 QuickSlotCount = 4;
	
public:
	UFUNCTION(BlueprintCallable)
	void InitializeHUD(UAI_REStatusComponent* InStatus);
	
};

#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/UI/AIREInventorySlotWidget.h"
#include "AIRECompanionInventoryPanelWidget.generated.h"

class UAIRECompanionInventoryComponent;
class UAIREGameplayInventorySubsystem;
class UAIREInventorySlotWidget;
class UAIREInventoryDragDropOperation;
class UAI_REPlayerCombatComponent;
class UAI_REPlayerInventoryComponent;
class UBorder;
class UButton;
class USpinBox;
class UTextBlock;
class UUniformGridPanel;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIRECompanionInventoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializePanel(
		UAIREGameplayInventorySubsystem* InGameplayInventory,
		UAIRECompanionInventoryComponent* InMakoInventory,
		UAI_REPlayerInventoryComponent* InPlayerInventory,
		UAI_REPlayerCombatComponent* InPlayerCombat);
	void SetPanelOpen(bool bOpen);
	bool IsPanelOpen() const;
	FSimpleMulticastDelegate& OnCloseRequested();

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> MakoGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> EquipmentGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> PlayerGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> PlayerEquipmentGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> QuantityPicker;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USpinBox> QuantitySpinBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuantityConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuantityCancelButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "AIRE|Inventory|UI")
	TSubclassOf<UAIREInventorySlotWidget> SlotWidgetClass;

private:
	UFUNCTION() void HandleInventoryChanged();
	UFUNCTION() void HandleMakoEquipResult(FName WeaponItemId, bool bSucceeded);
	UFUNCTION() void HandlePlayerEquipResult(FName WeaponItemId, bool bSucceeded);
	UFUNCTION() void HandleQuantityConfirmClicked();
	UFUNCTION() void HandleQuantityCancelClicked();
	UFUNCTION() void HandleCloseClicked();
	void HandleSlotDragStarted(UAIREInventoryDragDropOperation* Operation);
	void HandleSlotDropped(
		UAIREInventoryDragDropOperation* Operation,
		UAIREInventorySlotWidget* DestinationSlot);
	bool SubmitTransfer(
		EAIREInventorySlotSource Source,
		int32 SourceSlotIndex,
		FName ItemId,
		int32 ExpectedSourceCount,
		int32 Count);
	bool SubmitMakoEquip(UAIREInventoryDragDropOperation* Operation);
	bool SubmitPlayerEquip(UAIREInventoryDragDropOperation* Operation);
	bool ValidateTransferSource(
		EAIREInventorySlotSource Source,
		int32 SourceSlotIndex,
		FName ItemId,
		int32 ExpectedSourceCount,
		FAIREInventoryContainerSnapshot& OutMakoSnapshot,
		int32& OutFreshSourceCount) const;
	void ShowQuantityPicker();
	void HideQuantityPicker();
	void ClearPendingTransfer();
	void QueueRefresh();
	void Refresh();
	void UnbindSources();
	TWeakObjectPtr<UAIREGameplayInventorySubsystem> GameplayInventory;
	TWeakObjectPtr<UAIRECompanionInventoryComponent> MakoInventory;
	TWeakObjectPtr<UAI_REPlayerInventoryComponent> PlayerInventory;
	TWeakObjectPtr<UAI_REPlayerCombatComponent> PlayerCombat;
	EAIREInventorySlotSource PendingSource = EAIREInventorySlotSource::Player;
	int32 PendingSourceSlotIndex = INDEX_NONE;
	FName PendingItemId;
	int32 PendingSourceCount = 0;
	FGuid ActiveDragSessionId;
	FGuid DisplayedSessionId;
	FTimerHandle RefreshTimerHandle;
	bool bPanelOpen = false;
	bool bRefreshQueued = false;
	FSimpleMulticastDelegate CloseRequested;
};

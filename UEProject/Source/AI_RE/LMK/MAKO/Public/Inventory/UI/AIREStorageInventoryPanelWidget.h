#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/UI/AIREInventorySlotWidget.h"
#include "AIREStorageInventoryPanelWidget.generated.h"

class UAI_REPlayerInventoryComponent;
class UAIREGameplayInventorySubsystem;
class UAIREInventoryDragDropOperation;
class UButton;
class UBorder;
class USpinBox;
class UTextBlock;
class UUniformGridPanel;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREStorageInventoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializePanel(
		UAIREGameplayInventorySubsystem* InInventory,
		UAI_REPlayerInventoryComponent* InPlayerInventory);
	void SetPanelOpen(bool bOpen);
	bool IsPanelOpen() const;
	FSimpleMulticastDelegate& OnCloseRequested();

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> PlayerGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> StorageGrid;

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
	UFUNCTION() void HandleContainerChanged(FName ContainerId, int64 Revision);
	UFUNCTION() void HandlePlayerInventoryChanged();
	UFUNCTION() void HandleQuantityConfirmClicked();
	UFUNCTION() void HandleQuantityCancelClicked();
	UFUNCTION() void HandleCloseClicked();
	void HandleSlotDragStarted(UAIREInventoryDragDropOperation* Operation);
	void HandleSlotDropped(
		UAIREInventoryDragDropOperation* Operation,
		UAIREInventorySlotWidget* DestinationSlot);
	void QueueRefresh();
	void Refresh();
	bool SubmitTransfer(
		EAIREInventorySlotSource Source,
		int32 SourceSlotIndex,
		FName ItemId,
		int32 ExpectedSourceCount,
		int32 Count);
	bool ValidateSource(
		EAIREInventorySlotSource Source,
		int32 SourceSlotIndex,
		FName ItemId,
		int32 ExpectedSourceCount,
		FAIREInventoryContainerSnapshot& OutStorageSnapshot,
		int32& OutFreshSourceCount);
	void ShowQuantityPicker();
	void HideQuantityPicker();
	void ClearPendingTransfer();
	void UnbindSources();
	TWeakObjectPtr<UAIREGameplayInventorySubsystem> Inventory;
	TWeakObjectPtr<UAI_REPlayerInventoryComponent> PlayerInventory;
	EAIREInventorySlotSource PendingSource = EAIREInventorySlotSource::Player;
	int32 PendingSourceSlotIndex = INDEX_NONE;
	FName PendingItemId;
	int32 PendingSourceCount = 0;
	FGuid ActiveDragSessionId;
	FGuid DisplayedSessionId;
	bool bPanelOpen = false;
	bool bRefreshQueued = false;
	FSimpleMulticastDelegate CloseRequested;
};

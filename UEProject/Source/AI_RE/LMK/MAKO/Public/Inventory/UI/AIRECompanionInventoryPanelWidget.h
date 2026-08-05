#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIRECompanionInventoryPanelWidget.generated.h"

class UAIRECompanionInventoryComponent;
class UAIREInventorySlotWidget;
class UButton;
class UTextBlock;
class UUniformGridPanel;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIRECompanionInventoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializePanel(UAIRECompanionInventoryComponent* InInventory);
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
	TObjectPtr<UButton> EquipButton;

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
	UFUNCTION() void HandleEquipResult(FName WeaponItemId, bool bSucceeded);
	UFUNCTION() void HandleEquipClicked();
	UFUNCTION() void HandleCloseClicked();
	void HandleSlotClicked(UAIREInventorySlotWidget* ClickedSlot);
	void QueueRefresh();
	void Refresh();
	void UnbindInventory();
	void ClearSelection();
	TWeakObjectPtr<UAIRECompanionInventoryComponent> Inventory;
	int32 SelectedMakoSlotIndex = INDEX_NONE;
	FName SelectedItemId;
	FGuid DisplayedSessionId;
	bool bPanelOpen = false;
	bool bRefreshQueued = false;
	FSimpleMulticastDelegate CloseRequested;
};

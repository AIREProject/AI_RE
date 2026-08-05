#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "Blueprint/UserWidget.h"
#include "AIREInventorySlotWidget.generated.h"

class UBorder;
class UAIREInventoryDragDropOperation;
class UImage;
class UTextBlock;

UENUM(BlueprintType)
enum class EAIREInventorySlotSource : uint8
{
	Player,
	Storage,
	Mako,
	Equipment
};

DECLARE_DELEGATE_TwoParams(
	FAIREInventorySlotDropped,
	UAIREInventoryDragDropOperation*,
	class UAIREInventorySlotWidget*);
DECLARE_DELEGATE_OneParam(
	FAIREInventorySlotDragStarted,
	UAIREInventoryDragDropOperation*);
DECLARE_DELEGATE_OneParam(FAIREInventorySlotClicked, class UAIREInventorySlotWidget*);

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSlotData(
		EAIREInventorySlotSource InSource,
		int32 InSlotIndex,
		FName InItemId,
		int32 InCount,
		bool bInEquipped = false,
		bool bInPending = false,
		EAIREEquipmentTransitionState InState =
			EAIREEquipmentTransitionState::Idle);
	void SetSelected(bool bInSelected);
	EAIREInventorySlotSource GetSource() const;
	int32 GetSlotIndex() const;
	FName GetItemId() const;
	int32 GetItemCount() const;
	FAIREInventorySlotClicked& OnSlotClicked();
	FAIREInventorySlotDragStarted& OnSlotDragStarted();
	FAIREInventorySlotDropped& OnSlotDropped();

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemCountText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> SelectionBorder;

private:
	void RefreshVisuals();
	EAIREInventorySlotSource Source = EAIREInventorySlotSource::Player;
	int32 SlotIndex = INDEX_NONE;
	FName ItemId;
	int32 ItemCount = 0;
	bool bEquipped = false;
	bool bPending = false;
	EAIREEquipmentTransitionState EquipmentState = EAIREEquipmentTransitionState::Idle;
	FAIREInventorySlotClicked SlotClicked;
	FAIREInventorySlotDragStarted SlotDragStarted;
	FAIREInventorySlotDropped SlotDropped;
};

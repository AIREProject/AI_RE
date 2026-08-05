#include "Inventory/UI/AIREStorageInventoryPanelWidget.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/World.h"
#include "Inventory/UI/AIREInventoryDragDropOperation.h"
#include "Inventory/UI/AIREInventorySlotWidget.h"
#include "TimerManager.h"

namespace
{
	FText GetMutationStatusText(const EAIREInventoryMutationCode Code)
	{
		const UEnum* MutationCodeEnum = StaticEnum<EAIREInventoryMutationCode>();
		return IsValid(MutationCodeEnum)
			? MutationCodeEnum->GetDisplayNameTextByValue(static_cast<int64>(Code))
			: FText::GetEmpty();
	}
}

void UAIREStorageInventoryPanelWidget::InitializePanel(
	UAIREGameplayInventorySubsystem* InInventory,
	UAI_REPlayerInventoryComponent* InPlayerInventory)
{
	UnbindSources();
	Inventory = InInventory;
	PlayerInventory = InPlayerInventory;
	ClearPendingTransfer();
	DisplayedSessionId.Invalidate();

	if (Inventory.IsValid())
	{
		Inventory->OnContainerChanged.AddUniqueDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandleContainerChanged);
	}
	if (PlayerInventory.IsValid())
	{
		PlayerInventory->OnInventoryChanged.AddUniqueDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandlePlayerInventoryChanged);
	}
	if (IsValid(QuantityConfirmButton))
	{
		QuantityConfirmButton->OnClicked.AddUniqueDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandleQuantityConfirmClicked);
	}
	if (IsValid(QuantityCancelButton))
	{
		QuantityCancelButton->OnClicked.AddUniqueDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandleQuantityCancelClicked);
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandleCloseClicked);
	}

	HideQuantityPicker();
	QueueRefresh();
}

void UAIREStorageInventoryPanelWidget::SetPanelOpen(const bool bOpen)
{
	bPanelOpen = bOpen;
	SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bOpen)
	{
		ClearPendingTransfer();
	}
	if (bOpen)
	{
		QueueRefresh();
	}
}

bool UAIREStorageInventoryPanelWidget::IsPanelOpen() const
{
	return bPanelOpen;
}

FSimpleMulticastDelegate& UAIREStorageInventoryPanelWidget::OnCloseRequested()
{
	return CloseRequested;
}

void UAIREStorageInventoryPanelWidget::NativeDestruct()
{
	if (IsValid(QuantityConfirmButton))
	{
		QuantityConfirmButton->OnClicked.RemoveDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandleQuantityConfirmClicked);
	}
	if (IsValid(QuantityCancelButton))
	{
		QuantityCancelButton->OnClicked.RemoveDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandleQuantityCancelClicked);
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandleCloseClicked);
	}
	ClearPendingTransfer();
	UnbindSources();
	CloseRequested.Clear();
	Super::NativeDestruct();
}

void UAIREStorageInventoryPanelWidget::HandleContainerChanged(
	const FName ContainerId, const int64 Revision)
{
	(void)Revision;
	if (ContainerId == UAIREGameplayInventorySubsystem::GetSharedStorageContainerId())
	{
		QueueRefresh();
	}
}

void UAIREStorageInventoryPanelWidget::HandlePlayerInventoryChanged()
{
	QueueRefresh();
}

void UAIREStorageInventoryPanelWidget::HandleQuantityConfirmClicked()
{
	if (PendingSourceSlotIndex == INDEX_NONE || PendingItemId.IsNone())
	{
		return;
	}

	const int32 Count = IsValid(QuantitySpinBox)
		? FMath::RoundToInt(QuantitySpinBox->GetValue())
		: 0;
	SubmitTransfer(
		PendingSource,
		PendingSourceSlotIndex,
		PendingItemId,
		PendingSourceCount,
		Count);
	ClearPendingTransfer();
}

void UAIREStorageInventoryPanelWidget::HandleQuantityCancelClicked()
{
	ClearPendingTransfer();
}

void UAIREStorageInventoryPanelWidget::HandleCloseClicked()
{
	CloseRequested.Broadcast();
}

void UAIREStorageInventoryPanelWidget::HandleSlotDragStarted(
	UAIREInventoryDragDropOperation* Operation)
{
	if (IsValid(Operation))
	{
		ActiveDragSessionId = DisplayedSessionId;
	}
}

void UAIREStorageInventoryPanelWidget::HandleSlotDropped(
	UAIREInventoryDragDropOperation* Operation,
	UAIREInventorySlotWidget* DestinationSlot)
{
	if (!IsValid(Operation) || !IsValid(DestinationSlot)
		|| !bPanelOpen
		|| Operation->Source == DestinationSlot->GetSource()
		|| (Operation->Source != EAIREInventorySlotSource::Player
			&& Operation->Source != EAIREInventorySlotSource::Storage)
		|| (DestinationSlot->GetSource() != EAIREInventorySlotSource::Player
			&& DestinationSlot->GetSource() != EAIREInventorySlotSource::Storage))
	{
		ActiveDragSessionId.Invalidate();
		return;
	}

	FAIREInventoryContainerSnapshot StorageSnapshot;
	int32 FreshSourceCount = 0;
	if (!ValidateSource(
			Operation->Source,
			Operation->SourceSlotIndex,
			Operation->ItemId,
			Operation->ItemCount,
			StorageSnapshot,
			FreshSourceCount))
	{
		ClearPendingTransfer();
		ActiveDragSessionId.Invalidate();
		QueueRefresh();
		return;
	}

	if (Operation->bExactQuantityRequested)
	{
		PendingSource = Operation->Source;
		PendingSourceSlotIndex = Operation->SourceSlotIndex;
		PendingItemId = Operation->ItemId;
		PendingSourceCount = FreshSourceCount;
		ShowQuantityPicker();
		return;
	}

	SubmitTransfer(
		Operation->Source,
		Operation->SourceSlotIndex,
		Operation->ItemId,
		FreshSourceCount,
		FreshSourceCount);
	ActiveDragSessionId.Invalidate();
}

void UAIREStorageInventoryPanelWidget::QueueRefresh()
{
	if (bRefreshQueued)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	bRefreshQueued = true;
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bRefreshQueued = false;
			Refresh();
		}));
}

void UAIREStorageInventoryPanelWidget::Refresh()
{
	if (!Inventory.IsValid() || !PlayerInventory.IsValid() || !SlotWidgetClass
		|| !IsValid(PlayerGrid) || !IsValid(StorageGrid))
	{
		return;
	}

	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!Inventory->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(), StorageSnapshot))
	{
		return;
	}
	if (DisplayedSessionId.IsValid() && DisplayedSessionId != StorageSnapshot.SessionId)
	{
		ClearPendingTransfer();
	}
	DisplayedSessionId = StorageSnapshot.SessionId;

	PlayerGrid->ClearChildren();
	for (int32 SlotIndex = 0; SlotIndex < PlayerInventory->MaxSlots; ++SlotIndex)
	{
		const FInventoryItemStack* Stack = PlayerInventory->Items.FindByPredicate(
			[SlotIndex](const FInventoryItemStack& Candidate) { return Candidate.SlotIndex == SlotIndex; });
		UAIREInventorySlotWidget* SlotWidget = CreateWidget<UAIREInventorySlotWidget>(this, SlotWidgetClass);
		if (!IsValid(SlotWidget))
		{
			continue;
		}
		SlotWidget->SetSlotData(EAIREInventorySlotSource::Player, SlotIndex,
			Stack ? Stack->ItemId : NAME_None, Stack ? Stack->Count : 0);
		SlotWidget->SetSelected(false);
		SlotWidget->OnSlotDragStarted().BindUObject(this, &UAIREStorageInventoryPanelWidget::HandleSlotDragStarted);
		SlotWidget->OnSlotDropped().BindUObject(this, &UAIREStorageInventoryPanelWidget::HandleSlotDropped);
		PlayerGrid->AddChildToUniformGrid(SlotWidget, SlotIndex / 5, SlotIndex % 5);
	}

	StorageGrid->ClearChildren();
	for (int32 SlotIndex = 0; SlotIndex < StorageSnapshot.Capacity; ++SlotIndex)
	{
		const FAIREInventoryItemStackSnapshot* Stack = StorageSnapshot.ItemStacks.FindByPredicate(
			[SlotIndex](const FAIREInventoryItemStackSnapshot& Candidate) { return Candidate.SlotIndex == SlotIndex; });
		UAIREInventorySlotWidget* SlotWidget = CreateWidget<UAIREInventorySlotWidget>(this, SlotWidgetClass);
		if (!IsValid(SlotWidget))
		{
			continue;
		}
		SlotWidget->SetSlotData(EAIREInventorySlotSource::Storage, SlotIndex,
			Stack ? Stack->ItemId : NAME_None, Stack ? Stack->Count : 0);
		SlotWidget->SetSelected(false);
		SlotWidget->OnSlotDragStarted().BindUObject(this, &UAIREStorageInventoryPanelWidget::HandleSlotDragStarted);
		SlotWidget->OnSlotDropped().BindUObject(this, &UAIREStorageInventoryPanelWidget::HandleSlotDropped);
		StorageGrid->AddChildToUniformGrid(SlotWidget, SlotIndex / 10, SlotIndex % 10);
	}
}

bool UAIREStorageInventoryPanelWidget::SubmitTransfer(
	const EAIREInventorySlotSource Source,
	const int32 SourceSlotIndex,
	const FName ItemId,
	const int32 ExpectedSourceCount,
	const int32 Count)
{
	FAIREInventoryContainerSnapshot StorageSnapshot;
	int32 FreshSourceCount = 0;
	if (!ValidateSource(Source, SourceSlotIndex, ItemId, ExpectedSourceCount,
		StorageSnapshot, FreshSourceCount) || Count <= 0 || Count > FreshSourceCount)
	{
		QueueRefresh();
		return false;
	}

	FAIREPlayerStorageTransferRequest Request;
	Request.SessionId = StorageSnapshot.SessionId;
	Request.MutationId = FGuid::NewGuid();
	Request.Direction = Source == EAIREInventorySlotSource::Player
		? EAIREPlayerStorageTransferDirection::DepositPlayerToStorage
		: EAIREPlayerStorageTransferDirection::WithdrawStorageToPlayer;
	Request.ExpectedStorageRevision = StorageSnapshot.Revision;
	Request.SourceSlotIndex = SourceSlotIndex;
	Request.Count = Count;

	const FAIREInventoryMutationResult Result =
		Inventory->TryTransferPlayerStorage(PlayerInventory.Get(), Request);
	if (IsValid(StatusText))
	{
		StatusText->SetText(GetMutationStatusText(Result.Code));
	}
	QueueRefresh();
	return Result.Code == EAIREInventoryMutationCode::Succeeded;
}

bool UAIREStorageInventoryPanelWidget::ValidateSource(
	const EAIREInventorySlotSource Source,
	const int32 SourceSlotIndex,
	const FName ItemId,
	const int32 ExpectedSourceCount,
	FAIREInventoryContainerSnapshot& OutStorageSnapshot,
	int32& OutFreshSourceCount)
{
	OutFreshSourceCount = 0;
	if (!Inventory.IsValid() || !PlayerInventory.IsValid() || ItemId.IsNone()
		|| ExpectedSourceCount <= 0
		|| !ActiveDragSessionId.IsValid()
		|| !Inventory->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(), OutStorageSnapshot)
		|| ActiveDragSessionId != OutStorageSnapshot.SessionId
		|| (DisplayedSessionId.IsValid() && DisplayedSessionId != OutStorageSnapshot.SessionId))
	{
		return false;
	}

	if (Source == EAIREInventorySlotSource::Player)
	{
		const FInventoryItemStack* Stack = PlayerInventory->Items.FindByPredicate(
			[SourceSlotIndex](const FInventoryItemStack& Candidate) { return Candidate.SlotIndex == SourceSlotIndex; });
		OutFreshSourceCount = Stack && Stack->ItemId == ItemId ? Stack->Count : 0;
	}
	else if (Source == EAIREInventorySlotSource::Storage)
	{
		const FAIREInventoryItemStackSnapshot* Stack = OutStorageSnapshot.ItemStacks.FindByPredicate(
			[SourceSlotIndex](const FAIREInventoryItemStackSnapshot& Candidate) { return Candidate.SlotIndex == SourceSlotIndex; });
		OutFreshSourceCount = Stack && Stack->ItemId == ItemId ? Stack->Count : 0;
	}

	return OutFreshSourceCount == ExpectedSourceCount;
}

void UAIREStorageInventoryPanelWidget::ShowQuantityPicker()
{
	if (IsValid(QuantitySpinBox))
	{
		QuantitySpinBox->SetMinValue(1.0f);
		QuantitySpinBox->SetMaxValue(static_cast<float>(PendingSourceCount));
		QuantitySpinBox->SetValue(static_cast<float>(PendingSourceCount));
	}
	if (IsValid(QuantityPicker))
	{
		QuantityPicker->SetVisibility(ESlateVisibility::Visible);
	}
}

void UAIREStorageInventoryPanelWidget::HideQuantityPicker()
{
	if (IsValid(QuantityPicker))
	{
		QuantityPicker->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAIREStorageInventoryPanelWidget::ClearPendingTransfer()
{
	PendingSource = EAIREInventorySlotSource::Player;
	PendingSourceSlotIndex = INDEX_NONE;
	PendingItemId = NAME_None;
	PendingSourceCount = 0;
	ActiveDragSessionId.Invalidate();
	HideQuantityPicker();
}

void UAIREStorageInventoryPanelWidget::UnbindSources()
{
	if (Inventory.IsValid())
	{
		Inventory->OnContainerChanged.RemoveDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandleContainerChanged);
	}
	if (PlayerInventory.IsValid())
	{
		PlayerInventory->OnInventoryChanged.RemoveDynamic(
			this, &UAIREStorageInventoryPanelWidget::HandlePlayerInventoryChanged);
	}
	Inventory.Reset();
	PlayerInventory.Reset();
}

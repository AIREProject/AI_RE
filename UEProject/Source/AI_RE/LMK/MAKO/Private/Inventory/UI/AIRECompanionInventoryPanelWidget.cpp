#include "Inventory/UI/AIRECompanionInventoryPanelWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/World.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Inventory/UI/AIREInventorySlotWidget.h"
#include "TimerManager.h"

namespace
{
	FText GetEquipRequestStatusText(const EAIREInventoryMutationCode Code)
	{
		const UEnum* MutationCodeEnum =
			StaticEnum<EAIREInventoryMutationCode>();
		return IsValid(MutationCodeEnum)
			? MutationCodeEnum->GetDisplayNameTextByValue(
				static_cast<int64>(Code))
			: FText::GetEmpty();
	}
}

void UAIRECompanionInventoryPanelWidget::InitializePanel(
	UAIRECompanionInventoryComponent* InInventory)
{
	UnbindInventory();
	Inventory = InInventory;
	ClearSelection();
	DisplayedSessionId.Invalidate();

	if (Inventory.IsValid())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleInventoryChanged);
		Inventory->OnWeaponEquipResult.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleEquipResult);
	}
	if (IsValid(EquipButton))
	{
		EquipButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleEquipClicked);
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleCloseClicked);
	}
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::SetPanelOpen(const bool bOpen)
{
	bPanelOpen = bOpen;
	SetVisibility(
		bOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (bOpen)
	{
		QueueRefresh();
	}
}

bool UAIRECompanionInventoryPanelWidget::IsPanelOpen() const
{
	return bPanelOpen;
}

FSimpleMulticastDelegate&
UAIRECompanionInventoryPanelWidget::OnCloseRequested()
{
	return CloseRequested;
}

void UAIRECompanionInventoryPanelWidget::NativeDestruct()
{
	if (IsValid(EquipButton))
	{
		EquipButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleEquipClicked);
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleCloseClicked);
	}
	UnbindInventory();
	CloseRequested.Clear();
	Super::NativeDestruct();
}

void UAIRECompanionInventoryPanelWidget::HandleInventoryChanged()
{
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::HandleEquipResult(
	const FName WeaponItemId,
	const bool bSucceeded)
{
	if (IsValid(StatusText))
	{
		StatusText->SetText(
			bSucceeded
				? FText::Format(
					FText::FromString(TEXT("Equipped: {0}")),
					FText::FromName(WeaponItemId))
				: FText::FromString(TEXT("Equip failed")));
	}
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::HandleEquipClicked()
{
	if (!Inventory.IsValid()
		|| SelectedMakoSlotIndex == INDEX_NONE
		|| SelectedItemId.IsNone())
	{
		return;
	}

	FAIREInventoryContainerSnapshot Snapshot;
	if (!Inventory->GetInventorySnapshot(Snapshot))
	{
		return;
	}
	if (DisplayedSessionId.IsValid()
		&& DisplayedSessionId != Snapshot.SessionId)
	{
		ClearSelection();
		QueueRefresh();
		return;
	}

	const FAIREInventoryItemStackSnapshot* SelectedStack =
		Snapshot.ItemStacks.FindByPredicate(
			[this](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.SlotIndex == SelectedMakoSlotIndex;
			});
	if (SelectedStack == nullptr
		|| SelectedStack->ItemId != SelectedItemId)
	{
		ClearSelection();
		QueueRefresh();
		return;
	}

	FAIREInventoryEquipRequest Request;
	Request.SessionId = Snapshot.SessionId;
	Request.MutationId = FGuid::NewGuid();
	Request.ExpectedRevision = Snapshot.Revision;
	Request.SourceSlotIndex = SelectedMakoSlotIndex;
	const FAIREInventoryMutationResult Result =
		Inventory->RequestEquipWeaponItem(Request);
	if (IsValid(StatusText))
	{
		StatusText->SetText(GetEquipRequestStatusText(Result.Code));
	}
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::HandleCloseClicked()
{
	CloseRequested.Broadcast();
}

void UAIRECompanionInventoryPanelWidget::HandleSlotClicked(
	UAIREInventorySlotWidget* ClickedSlot)
{
	if (!IsValid(ClickedSlot)
		|| ClickedSlot->GetSource() != EAIREInventorySlotSource::Mako
		|| ClickedSlot->GetItemId().IsNone())
	{
		return;
	}

	SelectedMakoSlotIndex = ClickedSlot->GetSlotIndex();
	SelectedItemId = ClickedSlot->GetItemId();
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::QueueRefresh()
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

void UAIRECompanionInventoryPanelWidget::Refresh()
{
	if (!Inventory.IsValid()
		|| !SlotWidgetClass
		|| !IsValid(MakoGrid)
		|| !IsValid(EquipmentGrid))
	{
		return;
	}

	FAIREInventoryContainerSnapshot Snapshot;
	if (!Inventory->GetInventorySnapshot(Snapshot))
	{
		return;
	}
	if (DisplayedSessionId.IsValid()
		&& DisplayedSessionId != Snapshot.SessionId)
	{
		ClearSelection();
	}
	DisplayedSessionId = Snapshot.SessionId;

	if (SelectedMakoSlotIndex != INDEX_NONE)
	{
		const FAIREInventoryItemStackSnapshot* SelectedStack =
			Snapshot.ItemStacks.FindByPredicate(
				[this](const FAIREInventoryItemStackSnapshot& Stack)
				{
					return Stack.SlotIndex == SelectedMakoSlotIndex;
				});
		if (SelectedStack == nullptr
			|| SelectedStack->ItemId != SelectedItemId)
		{
			ClearSelection();
		}
	}

	MakoGrid->ClearChildren();
	for (int32 SlotIndex = 0;
		SlotIndex < Snapshot.Capacity;
		++SlotIndex)
	{
		const FAIREInventoryItemStackSnapshot* Stack =
			Snapshot.ItemStacks.FindByPredicate(
				[SlotIndex](const FAIREInventoryItemStackSnapshot& Candidate)
				{
					return Candidate.SlotIndex == SlotIndex;
				});
		UAIREInventorySlotWidget* SlotWidget =
			CreateWidget<UAIREInventorySlotWidget>(this, SlotWidgetClass);
		if (!IsValid(SlotWidget))
		{
			continue;
		}
		const FName ItemId = Stack ? Stack->ItemId : NAME_None;
		SlotWidget->SetSlotData(
			EAIREInventorySlotSource::Mako,
			SlotIndex,
			ItemId,
			Stack ? Stack->Count : 0,
			false,
			!ItemId.IsNone()
				&& ItemId == Snapshot.Equipment.PendingItemId);
		SlotWidget->SetSelected(
			SelectedMakoSlotIndex == SlotIndex);
		SlotWidget->OnSlotClicked().BindUObject(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleSlotClicked);
		MakoGrid->AddChildToUniformGrid(
			SlotWidget,
			SlotIndex / 5,
			SlotIndex % 5);
	}

	EquipmentGrid->ClearChildren();
	UAIREInventorySlotWidget* EquipmentSlot =
		CreateWidget<UAIREInventorySlotWidget>(this, SlotWidgetClass);
	if (IsValid(EquipmentSlot))
	{
		EquipmentSlot->SetSlotData(
			EAIREInventorySlotSource::Equipment,
			0,
			Snapshot.Equipment.EquippedItemId,
			Snapshot.Equipment.EquippedItemId.IsNone() ? 0 : 1,
			!Snapshot.Equipment.EquippedItemId.IsNone(),
			false,
			Snapshot.Equipment.TransitionState);
		EquipmentGrid->AddChildToUniformGrid(EquipmentSlot, 0, 0);
	}
}

void UAIRECompanionInventoryPanelWidget::UnbindInventory()
{
	if (Inventory.IsValid())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleInventoryChanged);
		Inventory->OnWeaponEquipResult.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleEquipResult);
	}
	Inventory.Reset();
}

void UAIRECompanionInventoryPanelWidget::ClearSelection()
{
	SelectedMakoSlotIndex = INDEX_NONE;
	SelectedItemId = NAME_None;
}

#include "Inventory/UI/AIRECompanionInventoryPanelWidget.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AI_REPlayerCombatComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/World.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Inventory/UI/AIREInventoryDragDropOperation.h"
#include "Inventory/UI/AIREInventorySlotWidget.h"
#include "TimerManager.h"

namespace
{
	FText GetInventoryStatusText(const EAIREInventoryMutationCode Code)
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
	UAIREGameplayInventorySubsystem* InGameplayInventory,
	UAIRECompanionInventoryComponent* InMakoInventory,
	UAI_REPlayerInventoryComponent* InPlayerInventory,
	UAI_REPlayerCombatComponent* InPlayerCombat)
{
	UnbindSources();
	GameplayInventory = InGameplayInventory;
	MakoInventory = InMakoInventory;
	PlayerInventory = InPlayerInventory;
	PlayerCombat = InPlayerCombat;
	DisplayedSessionId.Invalidate();
	ClearPendingTransfer();

	if (MakoInventory.IsValid())
	{
		MakoInventory->OnInventoryChanged.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleInventoryChanged);
		MakoInventory->OnWeaponEquipResult.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleMakoEquipResult);
	}
	if (PlayerInventory.IsValid())
	{
		PlayerInventory->OnInventoryChanged.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleInventoryChanged);
		PlayerInventory->OnWeaponEquipResult.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandlePlayerEquipResult);
	}
	if (IsValid(QuantityConfirmButton))
	{
		QuantityConfirmButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleQuantityConfirmClicked);
	}
	if (IsValid(QuantityCancelButton))
	{
		QuantityCancelButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleQuantityCancelClicked);
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleCloseClicked);
	}

	HideQuantityPicker();
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::SetPanelOpen(const bool bOpen)
{
	bPanelOpen = bOpen;
	SetVisibility(
		bOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (!bOpen)
	{
		ClearPendingTransfer();
	}
	else
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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	RefreshTimerHandle.Invalidate();
	bRefreshQueued = false;
	if (IsValid(QuantityConfirmButton))
	{
		QuantityConfirmButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleQuantityConfirmClicked);
	}
	if (IsValid(QuantityCancelButton))
	{
		QuantityCancelButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleQuantityCancelClicked);
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleCloseClicked);
	}
	ClearPendingTransfer();
	UnbindSources();
	CloseRequested.Clear();
	Super::NativeDestruct();
}

void UAIRECompanionInventoryPanelWidget::HandleInventoryChanged()
{
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::HandleMakoEquipResult(
	const FName WeaponItemId,
	const bool bSucceeded)
{
	if (IsValid(StatusText))
	{
		StatusText->SetText(
			bSucceeded
				? FText::Format(
					FText::FromString(TEXT("MAKO Equipped: {0}")),
					FText::FromName(WeaponItemId))
				: FText::FromString(TEXT("MAKO Equip failed")));
	}
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::HandlePlayerEquipResult(
	const FName WeaponItemId,
	const bool bSucceeded)
{
	if (IsValid(StatusText))
	{
		StatusText->SetText(
			bSucceeded
				? FText::Format(
					FText::FromString(TEXT("Player Equipped: {0}")),
					FText::FromName(WeaponItemId))
				: FText::FromString(TEXT("Player Equip failed")));
	}
	QueueRefresh();
}

void UAIRECompanionInventoryPanelWidget::HandleQuantityConfirmClicked()
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

void UAIRECompanionInventoryPanelWidget::HandleQuantityCancelClicked()
{
	ClearPendingTransfer();
}

void UAIRECompanionInventoryPanelWidget::HandleCloseClicked()
{
	CloseRequested.Broadcast();
}

void UAIRECompanionInventoryPanelWidget::HandleSlotDragStarted(
	UAIREInventoryDragDropOperation* Operation)
{
	if (IsValid(Operation))
	{
		ActiveDragSessionId = DisplayedSessionId;
	}
}

void UAIRECompanionInventoryPanelWidget::HandleSlotDropped(
	UAIREInventoryDragDropOperation* Operation,
	UAIREInventorySlotWidget* DestinationSlot)
{
	if (!IsValid(Operation) || !IsValid(DestinationSlot) || !bPanelOpen)
	{
		ActiveDragSessionId.Invalidate();
		return;
	}

	if (DestinationSlot->GetSource()
		== EAIREInventorySlotSource::Equipment)
	{
		SubmitMakoEquip(Operation);
		ActiveDragSessionId.Invalidate();
		return;
	}
	if (DestinationSlot->GetSource()
		== EAIREInventorySlotSource::PlayerEquipment)
	{
		SubmitPlayerEquip(Operation);
		ActiveDragSessionId.Invalidate();
		return;
	}

	const bool bCrossInventoryDrop =
		(Operation->Source == EAIREInventorySlotSource::Mako
			&& DestinationSlot->GetSource()
				== EAIREInventorySlotSource::Player)
		|| (Operation->Source == EAIREInventorySlotSource::Player
			&& DestinationSlot->GetSource()
				== EAIREInventorySlotSource::Mako);
	if (!bCrossInventoryDrop)
	{
		ActiveDragSessionId.Invalidate();
		return;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	int32 FreshSourceCount = 0;
	if (!ValidateTransferSource(
			Operation->Source,
			Operation->SourceSlotIndex,
			Operation->ItemId,
			Operation->ItemCount,
			MakoSnapshot,
			FreshSourceCount))
	{
		ClearPendingTransfer();
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

bool UAIRECompanionInventoryPanelWidget::SubmitTransfer(
	const EAIREInventorySlotSource Source,
	const int32 SourceSlotIndex,
	const FName ItemId,
	const int32 ExpectedSourceCount,
	const int32 Count)
{
	FAIREInventoryContainerSnapshot MakoSnapshot;
	int32 FreshSourceCount = 0;
	if (!ValidateTransferSource(
			Source,
			SourceSlotIndex,
			ItemId,
			ExpectedSourceCount,
			MakoSnapshot,
			FreshSourceCount)
		|| Count <= 0
		|| Count > FreshSourceCount)
	{
		QueueRefresh();
		return false;
	}

	FAIREPlayerMakoTransferRequest Request;
	Request.SessionId = MakoSnapshot.SessionId;
	Request.MutationId = FGuid::NewGuid();
	Request.Direction = Source == EAIREInventorySlotSource::Player
		? EAIREPlayerMakoTransferDirection::PlayerToMako
		: EAIREPlayerMakoTransferDirection::MakoToPlayer;
	Request.ExpectedPlayerRevision =
		PlayerInventory->GetInventoryRevision();
	Request.ExpectedMakoRevision = MakoSnapshot.Revision;
	Request.SourceSlotIndex = SourceSlotIndex;
	Request.Count = Count;
	const FAIREInventoryMutationResult Result =
		GameplayInventory->TryTransferPlayerMako(
			PlayerInventory.Get(),
			Request);
	if (IsValid(StatusText))
	{
		StatusText->SetText(GetInventoryStatusText(Result.Code));
	}
	QueueRefresh();
	return Result.WasApplied();
}

bool UAIRECompanionInventoryPanelWidget::SubmitMakoEquip(
	UAIREInventoryDragDropOperation* Operation)
{
	if (!IsValid(Operation)
		|| Operation->Source != EAIREInventorySlotSource::Mako
		|| Operation->ItemCount != 1
		|| !MakoInventory.IsValid())
	{
		return false;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	int32 FreshSourceCount = 0;
	if (!ValidateTransferSource(
			Operation->Source,
			Operation->SourceSlotIndex,
			Operation->ItemId,
			Operation->ItemCount,
			MakoSnapshot,
			FreshSourceCount)
		|| FreshSourceCount != 1)
	{
		return false;
	}

	FAIREInventoryEquipRequest Request;
	Request.SessionId = MakoSnapshot.SessionId;
	Request.MutationId = FGuid::NewGuid();
	Request.ExpectedRevision = MakoSnapshot.Revision;
	Request.SourceSlotIndex = Operation->SourceSlotIndex;
	const FAIREInventoryMutationResult Result =
		MakoInventory->RequestEquipWeaponItem(Request);
	if (IsValid(StatusText))
	{
		StatusText->SetText(GetInventoryStatusText(Result.Code));
	}
	QueueRefresh();
	return Result.WasApplied();
}

bool UAIRECompanionInventoryPanelWidget::SubmitPlayerEquip(
	UAIREInventoryDragDropOperation* Operation)
{
	if (!IsValid(Operation)
		|| Operation->Source != EAIREInventorySlotSource::Player
		|| Operation->ItemCount != 1
		|| !GameplayInventory.IsValid()
		|| !PlayerInventory.IsValid()
		|| !PlayerCombat.IsValid())
	{
		return false;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	int32 FreshSourceCount = 0;
	if (!ValidateTransferSource(
			Operation->Source,
			Operation->SourceSlotIndex,
			Operation->ItemId,
			Operation->ItemCount,
			MakoSnapshot,
			FreshSourceCount)
		|| FreshSourceCount != 1)
	{
		return false;
	}

	FAIREPlayerWeaponEquipRequest Request;
	Request.SessionId = MakoSnapshot.SessionId;
	Request.MutationId = FGuid::NewGuid();
	Request.ExpectedPlayerRevision =
		PlayerInventory->GetInventoryRevision();
	Request.SourceSlotIndex = Operation->SourceSlotIndex;
	const FAIREInventoryMutationResult Result =
		GameplayInventory->TryEquipPlayerWeapon(
			PlayerInventory.Get(),
			PlayerCombat.Get(),
			Request);
	if (IsValid(StatusText))
	{
		StatusText->SetText(GetInventoryStatusText(Result.Code));
	}
	QueueRefresh();
	return Result.WasApplied();
}

bool UAIRECompanionInventoryPanelWidget::ValidateTransferSource(
	const EAIREInventorySlotSource Source,
	const int32 SourceSlotIndex,
	const FName ItemId,
	const int32 ExpectedSourceCount,
	FAIREInventoryContainerSnapshot& OutMakoSnapshot,
	int32& OutFreshSourceCount) const
{
	OutFreshSourceCount = 0;
	if (!GameplayInventory.IsValid()
		|| !MakoInventory.IsValid()
		|| !PlayerInventory.IsValid()
		|| ItemId.IsNone()
		|| ExpectedSourceCount <= 0
		|| !ActiveDragSessionId.IsValid()
		|| !MakoInventory->GetInventorySnapshot(OutMakoSnapshot)
		|| ActiveDragSessionId != OutMakoSnapshot.SessionId
		|| (DisplayedSessionId.IsValid()
			&& DisplayedSessionId != OutMakoSnapshot.SessionId))
	{
		return false;
	}

	if (Source == EAIREInventorySlotSource::Player)
	{
		const FInventoryItemStack* Stack =
			PlayerInventory->Items.FindByPredicate(
				[SourceSlotIndex](const FInventoryItemStack& Candidate)
				{
					return Candidate.SlotIndex == SourceSlotIndex;
				});
		OutFreshSourceCount = Stack && Stack->ItemId == ItemId
			? Stack->Count
			: 0;
	}
	else if (Source == EAIREInventorySlotSource::Mako)
	{
		const FAIREInventoryItemStackSnapshot* Stack =
			OutMakoSnapshot.ItemStacks.FindByPredicate(
				[SourceSlotIndex](
					const FAIREInventoryItemStackSnapshot& Candidate)
				{
					return Candidate.SlotIndex == SourceSlotIndex;
				});
		OutFreshSourceCount = Stack && Stack->ItemId == ItemId
			? Stack->Count
			: 0;
	}

	return OutFreshSourceCount == ExpectedSourceCount;
}

void UAIRECompanionInventoryPanelWidget::ShowQuantityPicker()
{
	if (IsValid(QuantitySpinBox))
	{
		QuantitySpinBox->SetMinValue(1.0f);
		QuantitySpinBox->SetMaxValue(
			static_cast<float>(PendingSourceCount));
		QuantitySpinBox->SetValue(
			static_cast<float>(PendingSourceCount));
	}
	if (IsValid(QuantityPicker))
	{
		QuantityPicker->SetVisibility(ESlateVisibility::Visible);
	}
}

void UAIRECompanionInventoryPanelWidget::HideQuantityPicker()
{
	if (IsValid(QuantityPicker))
	{
		QuantityPicker->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAIRECompanionInventoryPanelWidget::ClearPendingTransfer()
{
	PendingSource = EAIREInventorySlotSource::Player;
	PendingSourceSlotIndex = INDEX_NONE;
	PendingItemId = NAME_None;
	PendingSourceCount = 0;
	ActiveDragSessionId.Invalidate();
	HideQuantityPicker();
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
	RefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			RefreshTimerHandle.Invalidate();
			bRefreshQueued = false;
			Refresh();
		}));
}

void UAIRECompanionInventoryPanelWidget::Refresh()
{
	if (!GameplayInventory.IsValid()
		|| !MakoInventory.IsValid()
		|| !PlayerInventory.IsValid()
		|| !SlotWidgetClass
		|| !IsValid(MakoGrid)
		|| !IsValid(EquipmentGrid)
		|| !IsValid(PlayerGrid)
		|| !IsValid(PlayerEquipmentGrid))
	{
		return;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	if (!MakoInventory->GetInventorySnapshot(MakoSnapshot))
	{
		return;
	}
	if (DisplayedSessionId.IsValid()
		&& DisplayedSessionId != MakoSnapshot.SessionId)
	{
		ClearPendingTransfer();
	}
	DisplayedSessionId = MakoSnapshot.SessionId;

	MakoGrid->ClearChildren();
	for (int32 SlotIndex = 0;
		SlotIndex < MakoSnapshot.Capacity;
		++SlotIndex)
	{
		const FAIREInventoryItemStackSnapshot* Stack =
			MakoSnapshot.ItemStacks.FindByPredicate(
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
				&& ItemId == MakoSnapshot.Equipment.PendingItemId);
		SlotWidget->SetSelected(false);
		SlotWidget->OnSlotDragStarted().BindUObject(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleSlotDragStarted);
		SlotWidget->OnSlotDropped().BindUObject(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleSlotDropped);
		MakoGrid->AddChildToUniformGrid(
			SlotWidget,
			SlotIndex / 5,
			SlotIndex % 5);
	}

	EquipmentGrid->ClearChildren();
	UAIREInventorySlotWidget* MakoEquipmentSlot =
		CreateWidget<UAIREInventorySlotWidget>(this, SlotWidgetClass);
	if (IsValid(MakoEquipmentSlot))
	{
		MakoEquipmentSlot->SetSlotData(
			EAIREInventorySlotSource::Equipment,
			0,
			MakoSnapshot.Equipment.EquippedItemId,
			MakoSnapshot.Equipment.EquippedItemId.IsNone() ? 0 : 1,
			!MakoSnapshot.Equipment.EquippedItemId.IsNone(),
			false,
			MakoSnapshot.Equipment.TransitionState);
		MakoEquipmentSlot->OnSlotDropped().BindUObject(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleSlotDropped);
		EquipmentGrid->AddChildToUniformGrid(MakoEquipmentSlot, 0, 0);
	}

	PlayerGrid->ClearChildren();
	for (int32 SlotIndex = 0;
		SlotIndex < PlayerInventory->MaxSlots;
		++SlotIndex)
	{
		const FInventoryItemStack* Stack =
			PlayerInventory->Items.FindByPredicate(
				[SlotIndex](const FInventoryItemStack& Candidate)
				{
					return Candidate.SlotIndex == SlotIndex;
				});
		UAIREInventorySlotWidget* SlotWidget =
			CreateWidget<UAIREInventorySlotWidget>(this, SlotWidgetClass);
		if (!IsValid(SlotWidget))
		{
			continue;
		}
		SlotWidget->SetSlotData(
			EAIREInventorySlotSource::Player,
			SlotIndex,
			Stack ? Stack->ItemId : NAME_None,
			Stack ? Stack->Count : 0);
		SlotWidget->SetSelected(false);
		SlotWidget->OnSlotDragStarted().BindUObject(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleSlotDragStarted);
		SlotWidget->OnSlotDropped().BindUObject(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleSlotDropped);
		PlayerGrid->AddChildToUniformGrid(
			SlotWidget,
			SlotIndex / 5,
			SlotIndex % 5);
	}

	PlayerEquipmentGrid->ClearChildren();
	UAIREInventorySlotWidget* PlayerEquipmentSlot =
		CreateWidget<UAIREInventorySlotWidget>(this, SlotWidgetClass);
	if (IsValid(PlayerEquipmentSlot))
	{
		const FName EquippedItemId =
			PlayerInventory->GetEquippedWeaponItemId();
		PlayerEquipmentSlot->SetSlotData(
			EAIREInventorySlotSource::PlayerEquipment,
			0,
			EquippedItemId,
			EquippedItemId.IsNone() ? 0 : 1,
			!EquippedItemId.IsNone());
		PlayerEquipmentSlot->OnSlotDropped().BindUObject(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleSlotDropped);
		PlayerEquipmentGrid->AddChildToUniformGrid(
			PlayerEquipmentSlot,
			0,
			0);
	}
}

void UAIRECompanionInventoryPanelWidget::UnbindSources()
{
	if (MakoInventory.IsValid())
	{
		MakoInventory->OnInventoryChanged.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleInventoryChanged);
		MakoInventory->OnWeaponEquipResult.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleMakoEquipResult);
	}
	if (PlayerInventory.IsValid())
	{
		PlayerInventory->OnInventoryChanged.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandleInventoryChanged);
		PlayerInventory->OnWeaponEquipResult.RemoveDynamic(
			this,
			&UAIRECompanionInventoryPanelWidget::HandlePlayerEquipResult);
	}
	GameplayInventory.Reset();
	MakoInventory.Reset();
	PlayerInventory.Reset();
	PlayerCombat.Reset();
}

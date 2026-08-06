#include "Work/AIRECompanionStorageAutomationComponent.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AIRESharedStorageActor.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"
#include "TimerManager.h"
#include "Work/AIRECompanionStorageWorkRequest.h"
#include "Work/AIRECompanionWorkOrderComponent.h"

namespace
{
	int64 GetCarriedItemCount(
		const FAIREInventoryContainerSnapshot& MakoSnapshot,
		const FName ItemId)
	{
		int64 TotalCount = 0;
		for (const FAIREInventoryItemStackSnapshot& Stack : MakoSnapshot.ItemStacks)
		{
			if (Stack.ItemId == ItemId)
			{
				TotalCount += Stack.Count;
			}
		}
		if (MakoSnapshot.Equipment.EquippedItemId == ItemId)
		{
			++TotalCount;
		}
		return TotalCount;
	}

	const FAIREInventoryItemStackSnapshot* FindTransferableStack(
		const FAIREInventoryContainerSnapshot& Snapshot,
		const FName ItemId,
		const bool bIsMakoSource)
	{
		if (bIsMakoSource
			&& Snapshot.Equipment.PendingItemId == ItemId)
		{
			return nullptr;
		}
		return Snapshot.ItemStacks.FindByPredicate(
			[ItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == ItemId && Stack.Count > 0;
			});
	}
}

UAIRECompanionStorageAutomationComponent::
UAIRECompanionStorageAutomationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIRECompanionStorageAutomationComponent::InitializeAutomation(
	UAIRECompanionInventoryComponent* InInventoryComponent,
	UAIRECompanionWorkOrderComponent* InWorkOrderComponent,
	const UAIRECompanionConfigDataAsset* InCompanionConfig)
{
	ShutdownAutomation();
	if (!IsValid(InInventoryComponent)
		|| !IsValid(InWorkOrderComponent)
		|| !IsValid(InCompanionConfig))
	{
		return false;
	}

	InventoryComponent = InInventoryComponent;
	WorkOrderComponent = InWorkOrderComponent;
	CompanionConfig = const_cast<UAIRECompanionConfigDataAsset*>(
		InCompanionConfig);
	InWorkOrderComponent->OnWorkOrderChanged.AddUniqueDynamic(
		this,
		&UAIRECompanionStorageAutomationComponent::HandleWorkOrderChanged);

	UGameInstance* GameInstance =
		GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		IsValid(GameInstance)
			? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
			: nullptr;
	if (!IsValid(InventorySubsystem))
	{
		ShutdownAutomation();
		return false;
	}
	InventorySubsystem->OnContainerChanged.AddUniqueDynamic(
		this,
		&UAIRECompanionStorageAutomationComponent::HandleContainerChanged);
	bIsInitialized = true;
	ScheduleEvaluation();
	return true;
}

void UAIRECompanionStorageAutomationComponent::ShutdownAutomation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (WorkOrderComponent.IsValid())
	{
		WorkOrderComponent->OnWorkOrderChanged.RemoveDynamic(
			this,
			&UAIRECompanionStorageAutomationComponent::HandleWorkOrderChanged);
	}
	UGameInstance* GameInstance =
		GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (IsValid(GameInstance))
	{
		if (UAIREGameplayInventorySubsystem* InventorySubsystem =
			GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>())
		{
			InventorySubsystem->OnContainerChanged.RemoveDynamic(
				this,
				&UAIRECompanionStorageAutomationComponent::
					HandleContainerChanged);
		}
	}
	InventoryComponent.Reset();
	WorkOrderComponent.Reset();
	CompanionConfig.Reset();
	ClearFailedStorageState();
	bIsInitialized = false;
	bEvaluationScheduled = false;
}

void UAIRECompanionStorageAutomationComponent::SetPreferredStorage(
	AAIRESharedStorageActor* InPreferredStorage)
{
	PreferredStorage = InPreferredStorage;
	ClearFailedStorageState();
	ScheduleEvaluation();
}

void UAIRECompanionStorageAutomationComponent::HandleContainerChanged(
	const FName ContainerId,
	const int64 Revision)
{
	(void)Revision;
	if (ContainerId == UAIREGameplayInventorySubsystem::GetMakoContainerId()
		|| ContainerId
			== UAIREGameplayInventorySubsystem::GetSharedStorageContainerId())
	{
		ClearFailedStorageState();
		ScheduleEvaluation();
	}
}

void UAIRECompanionStorageAutomationComponent::HandleWorkOrderChanged(
	const FAIRECompanionWorkOrderSnapshot PreviousSnapshot,
	const FAIRECompanionWorkOrderSnapshot CurrentSnapshot)
{
	(void)PreviousSnapshot;
	const bool bIsTerminal =
		CurrentSnapshot.State == EAIRECompanionWorkOrderState::Completed
		|| CurrentSnapshot.State
			== EAIRECompanionWorkOrderState::Cancelled
		|| CurrentSnapshot.State == EAIRECompanionWorkOrderState::Failed;
	if (!bIsTerminal)
	{
		return;
	}
	if (CurrentSnapshot.WorkType
		== EAIRECompanionWorkOrderType::StorageTransfer
		&& CurrentSnapshot.State != EAIRECompanionWorkOrderState::Completed)
	{
		CaptureFailedStorageState();
	}
	else
	{
		ClearFailedStorageState();
	}
	ScheduleEvaluation();
}

void UAIRECompanionStorageAutomationComponent::ScheduleEvaluation()
{
	if (!bIsInitialized || bEvaluationScheduled || !GetWorld())
	{
		return;
	}
	bEvaluationScheduled = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bEvaluationScheduled = false;
			EvaluateStorageRules();
		}));
}

void UAIRECompanionStorageAutomationComponent::EvaluateStorageRules()
{
	if (!bIsInitialized
		|| !InventoryComponent.IsValid()
		|| !WorkOrderComponent.IsValid()
		|| !CompanionConfig.IsValid()
		|| !PreferredStorage.IsValid()
		|| WorkOrderComponent->HasActiveWorkOrder()
		|| CompanionConfig->StorageRules.IsEmpty())
	{
		return;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	if (!InventoryComponent->GetInventorySnapshot(MakoSnapshot)
		|| !MakoSnapshot.SessionId.IsValid())
	{
		return;
	}
	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!PreferredStorage->GetStorageSnapshot(StorageSnapshot)
		|| StorageSnapshot.SessionId != MakoSnapshot.SessionId)
	{
		return;
	}
	if (IsFailedStorageStateUnchanged(
			MakoSnapshot,
			StorageSnapshot))
	{
		return;
	}
	ClearFailedStorageState();

	for (const FAIRECompanionStorageRule& Rule
		: CompanionConfig->StorageRules)
	{
		if (!IsValid(Rule.ItemDefinition))
		{
			continue;
		}
		const FName ItemId = Rule.ItemDefinition->ItemId;
		const int64 CarriedCount = GetCarriedItemCount(
			MakoSnapshot,
			ItemId);
		if (CarriedCount > Rule.MaximumCarryCount)
		{
			const FAIREInventoryItemStackSnapshot* SourceStack =
				FindTransferableStack(MakoSnapshot, ItemId, true);
			FGuid WorkOrderId;
			if (SourceStack
				&& FAIRECompanionStorageWorkRequest::TryRequest(
					WorkOrderComponent.Get(),
					PreferredStorage.Get(),
					MakoSnapshot.SessionId,
					EAIRECompanionStorageTransferDirection::
						DepositMakoToStorage,
					ItemId,
					static_cast<int32>(FMath::Min<int64>(
						SourceStack->Count,
						CarriedCount - Rule.MaximumCarryCount)),
					WorkOrderId))
			{
				return;
			}
		}
	}

	for (const FAIRECompanionStorageRule& Rule
		: CompanionConfig->StorageRules)
	{
		if (!IsValid(Rule.ItemDefinition))
		{
			continue;
		}
		const FName ItemId = Rule.ItemDefinition->ItemId;
		const int64 CarriedCount = GetCarriedItemCount(
			MakoSnapshot,
			ItemId);
		if (CarriedCount < Rule.MinimumCarryCount)
		{
			const FAIREInventoryItemStackSnapshot* SourceStack =
				FindTransferableStack(StorageSnapshot, ItemId, false);
			FGuid WorkOrderId;
			if (SourceStack
				&& FAIRECompanionStorageWorkRequest::TryRequest(
					WorkOrderComponent.Get(),
					PreferredStorage.Get(),
					MakoSnapshot.SessionId,
					EAIRECompanionStorageTransferDirection::
						WithdrawStorageToMako,
					ItemId,
					static_cast<int32>(FMath::Min<int64>(
						SourceStack->Count,
						Rule.MinimumCarryCount - CarriedCount)),
					WorkOrderId))
			{
				return;
			}
		}
	}
}

void UAIRECompanionStorageAutomationComponent::
CaptureFailedStorageState()
{
	ClearFailedStorageState();
	if (!InventoryComponent.IsValid() || !PreferredStorage.IsValid())
	{
		return;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!InventoryComponent->GetInventorySnapshot(MakoSnapshot)
		|| !PreferredStorage->GetStorageSnapshot(StorageSnapshot)
		|| MakoSnapshot.SessionId != StorageSnapshot.SessionId)
	{
		return;
	}

	FailedStorageSessionId = MakoSnapshot.SessionId;
	FailedMakoRevision = MakoSnapshot.Revision;
	FailedStorageRevision = StorageSnapshot.Revision;
}

void UAIRECompanionStorageAutomationComponent::
ClearFailedStorageState()
{
	FailedStorageSessionId.Invalidate();
	FailedMakoRevision = INDEX_NONE;
	FailedStorageRevision = INDEX_NONE;
}

bool UAIRECompanionStorageAutomationComponent::
IsFailedStorageStateUnchanged(
		const FAIREInventoryContainerSnapshot& MakoSnapshot,
		const FAIREInventoryContainerSnapshot& StorageSnapshot) const
{
	return FailedStorageSessionId.IsValid()
		&& FailedStorageSessionId == MakoSnapshot.SessionId
		&& FailedStorageSessionId == StorageSnapshot.SessionId
		&& FailedMakoRevision == MakoSnapshot.Revision
		&& FailedStorageRevision == StorageSnapshot.Revision;
}

void UAIRECompanionStorageAutomationComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownAutomation();
	Super::EndPlay(EndPlayReason);
}

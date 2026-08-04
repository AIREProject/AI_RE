#include "Inventory/AIRECompanionInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AIREGameplayInventorySubsystem.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Engine/GameInstance.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionInventory, Log, All);

UAIRECompanionInventoryComponent::UAIRECompanionInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIRECompanionInventoryComponent::InitializeInventory(
	const UAIRECompanionConfigDataAsset* CompanionConfig,
	UAIRECompanionEquipmentComponent* InEquipmentComponent,
	UAbilitySystemComponent* InAbilitySystem)
{
	ShutdownInventory();
	if (!IsValid(CompanionConfig)
		|| !IsValid(InEquipmentComponent)
		|| !IsValid(InAbilitySystem))
	{
		return false;
	}

	UGameInstance* GameInstance = GetWorld()
		? GetWorld()->GetGameInstance()
		: nullptr;
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
		: nullptr;
	if (!IsValid(InventorySubsystem)
		|| !InventorySubsystem->EnsureMakoInventoryInitialized(
			CompanionConfig))
	{
		return false;
	}

	GameplayInventory = InventorySubsystem;
	EquipmentComponent = InEquipmentComponent;
	AbilitySystem = InAbilitySystem;
	WeaponEquipCompletedDelegateHandle = EquipmentComponent
		->OnWeaponEquipCompleted()
		.AddUObject(
			this,
			&UAIRECompanionInventoryComponent::HandleWeaponEquipCompleted);
	GameplayInventory->OnContainerChanged.AddUniqueDynamic(
		this,
		&UAIRECompanionInventoryComponent::HandleContainerChanged);
	bIsInitialized = true;

	FAIREInventoryContainerSnapshot Snapshot;
	if (!GetInventorySnapshot(Snapshot))
	{
		ShutdownInventory();
		return false;
	}
	BoundInventorySessionId = Snapshot.SessionId;
	if (!Snapshot.Equipment.EquippedItemId.IsNone())
	{
		const bool bAccepted = RequestRuntimeEquipment(
			Snapshot.Equipment.EquippedItemId,
			EAIREEquipmentCallbackMode::RestoreCurrent,
			Snapshot.SessionId,
			FGuid());
		if (!bAccepted
			&& EquipmentCallbackMode
				== EAIREEquipmentCallbackMode::RestoreCurrent)
		{
			const FName FailedItemId =
				Snapshot.Equipment.EquippedItemId;
			GameplayInventory->CompleteMakoEquipmentRuntimeRestore(
				Snapshot.SessionId,
				FailedItemId,
				false);
			ClearActiveEquipmentRequest();
			OnWeaponEquipResult.Broadcast(FailedItemId, false);
		}
	}

	return true;
}

void UAIRECompanionInventoryComponent::ShutdownInventory()
{
	if (GameplayInventory.IsValid()
		&& ActiveEquipmentSessionId.IsValid()
		&& ActiveEquipmentMutationId.IsValid()
		&& (EquipmentCallbackMode == EAIREEquipmentCallbackMode::Equipping
			|| EquipmentCallbackMode
				== EAIREEquipmentCallbackMode::Recovering))
	{
		GameplayInventory->CancelMakoEquipmentSwap(
			ActiveEquipmentSessionId,
			ActiveEquipmentMutationId);
	}

	if (GameplayInventory.IsValid())
	{
		GameplayInventory->OnContainerChanged.RemoveDynamic(
			this,
			&UAIRECompanionInventoryComponent::HandleContainerChanged);
	}
	if (WeaponEquipCompletedDelegateHandle.IsValid()
		&& EquipmentComponent.IsValid())
	{
		EquipmentComponent->OnWeaponEquipCompleted().Remove(
			WeaponEquipCompletedDelegateHandle);
	}
	WeaponEquipCompletedDelegateHandle.Reset();

	if (EquipmentCallbackMode != EAIREEquipmentCallbackMode::None
		&& EquipmentComponent.IsValid())
	{
		EquipmentComponent->UnequipCurrentWeapon();
	}

	ClearActiveEquipmentRequest();
	BoundInventorySessionId.Invalidate();
	GameplayInventory.Reset();
	EquipmentComponent.Reset();
	AbilitySystem.Reset();
	bIsInitialized = false;
	OnInventoryChanged.Clear();
	OnWeaponEquipResult.Clear();
}

bool UAIRECompanionInventoryComponent::HasItem(
	const FName ItemId,
	const int32 Count) const
{
	return !ItemId.IsNone() && Count > 0 && GetItemCount(ItemId) >= Count;
}

int32 UAIRECompanionInventoryComponent::GetItemCount(
	const FName ItemId) const
{
	if (ItemId.IsNone())
	{
		return 0;
	}

	FAIREInventoryContainerSnapshot Snapshot;
	if (!GetInventorySnapshot(Snapshot))
	{
		return 0;
	}

	int32 TotalCount = 0;
	for (const FAIREInventoryItemStackSnapshot& Stack : Snapshot.ItemStacks)
	{
		if (Stack.ItemId == ItemId)
		{
			TotalCount += Stack.Count;
		}
	}
	if (Snapshot.Equipment.EquippedItemId == ItemId)
	{
		++TotalCount;
	}
	return TotalCount;
}

bool UAIRECompanionInventoryComponent::TryAddItem(
	const FName ItemId,
	const int32 Count)
{
	FAIREInventoryContainerSnapshot Snapshot;
	if (!bIsInitialized
		|| !GameplayInventory.IsValid()
		|| !GetInventorySnapshot(Snapshot))
	{
		return false;
	}

	FAIREInventoryMutationRequest Request;
	Request.SessionId = Snapshot.SessionId;
	Request.MutationId = FGuid::NewGuid();
	Request.ContainerId = Snapshot.ContainerId;
	Request.ExpectedRevision = Snapshot.Revision;
	Request.ItemId = ItemId;
	Request.Count = Count;
	return GameplayInventory->TryAddItem(Request).WasApplied();
}

bool UAIRECompanionInventoryComponent::TryConsumeItem(
	const FName ItemId,
	const int32 Count)
{
	FAIREInventoryContainerSnapshot Snapshot;
	if (!bIsInitialized
		|| !GameplayInventory.IsValid()
		|| !GetInventorySnapshot(Snapshot)
		|| Snapshot.Equipment.EquippedItemId == ItemId)
	{
		return false;
	}

	FAIREInventoryMutationRequest Request;
	Request.SessionId = Snapshot.SessionId;
	Request.MutationId = FGuid::NewGuid();
	Request.ContainerId = Snapshot.ContainerId;
	Request.ExpectedRevision = Snapshot.Revision;
	Request.ItemId = ItemId;
	Request.Count = Count;
	return GameplayInventory->TryRemoveItem(Request).WasApplied();
}

bool UAIRECompanionInventoryComponent::EquipWeaponItem(
	const FName ItemId)
{
	FAIREInventoryContainerSnapshot Snapshot;
	if (!bIsInitialized
		|| ItemId.IsNone()
		|| !GetInventorySnapshot(Snapshot))
	{
		return false;
	}
	if (Snapshot.Equipment.EquippedItemId == ItemId)
	{
		if (Snapshot.Equipment.TransitionState
			== EAIREEquipmentTransitionState::Idle)
		{
			return true;
		}
		if (Snapshot.Equipment.TransitionState
			== EAIREEquipmentTransitionState::RecoveryFailed)
		{
			if (EquipmentCallbackMode != EAIREEquipmentCallbackMode::None
				|| !AbilitySystem.IsValid()
				|| AbilitySystem->HasMatchingGameplayTag(
					AIRECompanionGameplayTags::StateActionAttacking))
			{
				return false;
			}
			const bool bAccepted = RequestRuntimeEquipment(
				ItemId,
				EAIREEquipmentCallbackMode::RestoreCurrent,
				Snapshot.SessionId,
				FGuid());
			if (!bAccepted
				&& EquipmentCallbackMode
					== EAIREEquipmentCallbackMode::RestoreCurrent)
			{
				GameplayInventory->CompleteMakoEquipmentRuntimeRestore(
					Snapshot.SessionId,
					ItemId,
					false);
				ClearActiveEquipmentRequest();
				OnWeaponEquipResult.Broadcast(ItemId, false);
			}
			return bAccepted;
		}
		return false;
	}

	const FAIREInventoryItemStackSnapshot* SourceStack =
		Snapshot.ItemStacks.FindByPredicate(
			[ItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == ItemId;
			});
	if (!SourceStack)
	{
		return false;
	}

	FAIREInventoryEquipRequest Request;
	Request.SessionId = Snapshot.SessionId;
	Request.MutationId = FGuid::NewGuid();
	Request.ExpectedRevision = Snapshot.Revision;
	Request.SourceSlotIndex = SourceStack->SlotIndex;
	const FAIREInventoryMutationResult Result =
		RequestEquipWeaponItem(Request);
	return Result.Code == EAIREInventoryMutationCode::Succeeded
		|| Result.Code == EAIREInventoryMutationCode::AlreadyApplied;
}

FAIREInventoryMutationResult
UAIRECompanionInventoryComponent::RequestEquipWeaponItem(
	const FAIREInventoryEquipRequest& Request)
{
	FAIREInventoryMutationResult RejectedResult;
	RejectedResult.MutationId = Request.MutationId;
	if (!bIsInitialized
		|| !GameplayInventory.IsValid()
		|| !EquipmentComponent.IsValid()
		|| !AbilitySystem.IsValid())
	{
		RejectedResult.Code = EAIREInventoryMutationCode::NotInitialized;
		return RejectedResult;
	}
	if (EquipmentCallbackMode != EAIREEquipmentCallbackMode::None)
	{
		RejectedResult.Code = EAIREInventoryMutationCode::EquipmentBusy;
		return RejectedResult;
	}
	if (AbilitySystem->HasMatchingGameplayTag(
		AIRECompanionGameplayTags::StateActionAttacking))
	{
		RejectedResult.Code = EAIREInventoryMutationCode::EquipmentBusy;
		return RejectedResult;
	}

	FAIREInventoryMutationResult Result =
		GameplayInventory->ReserveMakoEquipmentSwap(Request);
	if (Result.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Result;
	}

	FAIREInventoryContainerSnapshot Snapshot;
	if (!GetInventorySnapshot(Snapshot)
		|| Snapshot.Equipment.PendingItemId.IsNone())
	{
		GameplayInventory->CancelMakoEquipmentSwap(
			Request.SessionId,
			Request.MutationId);
		Result.Code = EAIREInventoryMutationCode::EquipmentRequestRejected;
		return Result;
	}

	const FName PendingItemId = Snapshot.Equipment.PendingItemId;
	const bool bAccepted = RequestRuntimeEquipment(
		PendingItemId,
		EAIREEquipmentCallbackMode::Equipping,
		Request.SessionId,
		Request.MutationId);
	if (!bAccepted)
	{
		if (EquipmentCallbackMode == EAIREEquipmentCallbackMode::Equipping)
		{
			GameplayInventory->CancelMakoEquipmentSwap(
				Request.SessionId,
				Request.MutationId);
			ClearActiveEquipmentRequest();
			OnWeaponEquipResult.Broadcast(PendingItemId, false);
		}
		Result.Code = EAIREInventoryMutationCode::EquipmentRequestRejected;
	}
	return Result;
}

FName UAIRECompanionInventoryComponent::GetEquippedWeaponItemId() const
{
	FAIREInventoryContainerSnapshot Snapshot;
	return GetInventorySnapshot(Snapshot)
		? Snapshot.Equipment.EquippedItemId
		: NAME_None;
}

FName UAIRECompanionInventoryComponent::GetPendingWeaponItemId() const
{
	FAIREInventoryContainerSnapshot Snapshot;
	return GetInventorySnapshot(Snapshot)
		? Snapshot.Equipment.PendingItemId
		: NAME_None;
}

bool UAIRECompanionInventoryComponent::GetInventorySnapshot(
	FAIREInventoryContainerSnapshot& OutSnapshot) const
{
	return GameplayInventory.IsValid()
		&& GameplayInventory->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetMakoContainerId(),
			OutSnapshot);
}

bool UAIRECompanionInventoryComponent::CanCompleteMakoCraftWork(
	const FAIREMakoCraftWorkRequest& Request,
	FAIREInventoryWorkResult& OutResult) const
{
	if (!bIsInitialized || !GameplayInventory.IsValid())
	{
		OutResult = FAIREInventoryWorkResult();
		OutResult.Code = EAIREInventoryMutationCode::NotInitialized;
		return false;
	}
	return GameplayInventory->CanCompleteMakoCraftWork(Request, OutResult);
}

FAIREInventoryWorkResult UAIRECompanionInventoryComponent::TryCompleteMakoCraftWork(
	const FAIREMakoCraftWorkRequest& Request)
{
	if (!bIsInitialized || !GameplayInventory.IsValid())
	{
		FAIREInventoryWorkResult Result;
		Result.Code = EAIREInventoryMutationCode::NotInitialized;
		return Result;
	}
	return GameplayInventory->TryCompleteMakoCraftWork(Request);
}

FAIREInventoryWorkResult UAIRECompanionInventoryComponent::TryStoreMakoWorkReward(
	const FAIREMakoWorkRewardRequest& Request)
{
	if (!bIsInitialized || !GameplayInventory.IsValid())
	{
		FAIREInventoryWorkResult Result;
		Result.Code = EAIREInventoryMutationCode::NotInitialized;
		return Result;
	}
	return GameplayInventory->TryStoreMakoWorkReward(Request);
}

const UAIRECompanionItemDefinitionDataAsset*
UAIRECompanionInventoryComponent::FindItemDefinition(
	const FName ItemId) const
{
	return GameplayInventory.IsValid()
		? GameplayInventory->FindCompanionItemDefinition(ItemId)
		: nullptr;
}

void UAIRECompanionInventoryComponent::HandleWeaponEquipCompleted(
	UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
	const bool bSucceeded)
{
	if (!bIsInitialized
		|| EquipmentCallbackMode == EAIREEquipmentCallbackMode::None
		|| !GameplayInventory.IsValid())
	{
		return;
	}

	FAIREInventoryContainerSnapshot Snapshot;
	if (!GetInventorySnapshot(Snapshot)
		|| Snapshot.SessionId != ActiveEquipmentSessionId)
	{
		return;
	}

	const FName ExpectedItemId =
		EquipmentCallbackMode == EAIREEquipmentCallbackMode::Equipping
		? Snapshot.Equipment.PendingItemId
		: Snapshot.Equipment.EquippedItemId;
	const UAIRECompanionItemDefinitionDataAsset* ExpectedItem =
		FindItemDefinition(ExpectedItemId);
	if (!IsValid(ExpectedItem)
		|| ExpectedItem->WeaponDefinition != WeaponDefinition)
	{
		return;
	}

	if (EquipmentCallbackMode == EAIREEquipmentCallbackMode::RestoreCurrent)
	{
		const FAIREInventoryMutationResult RestoreResult =
			GameplayInventory->CompleteMakoEquipmentRuntimeRestore(
				ActiveEquipmentSessionId,
				ExpectedItemId,
				bSucceeded);
		ClearActiveEquipmentRequest();
		OnWeaponEquipResult.Broadcast(
			ExpectedItemId,
			bSucceeded
				&& RestoreResult.Code
					== EAIREInventoryMutationCode::Succeeded);
		return;
	}

	if (EquipmentCallbackMode == EAIREEquipmentCallbackMode::Equipping)
	{
		if (bSucceeded)
		{
			const FAIREInventoryMutationResult Result =
				GameplayInventory->CommitMakoEquipmentSwap(
					ActiveEquipmentSessionId,
					ActiveEquipmentMutationId);
			ClearActiveEquipmentRequest();
			OnWeaponEquipResult.Broadcast(
				ExpectedItemId,
				Result.Code == EAIREInventoryMutationCode::Succeeded);
			return;
		}

		OnWeaponEquipResult.Broadcast(ExpectedItemId, false);
		const FName RecoveryItemId = Snapshot.Equipment.EquippedItemId;
		const FAIREInventoryMutationResult RecoveryResult =
			GameplayInventory->BeginMakoEquipmentRecovery(
				ActiveEquipmentSessionId,
				ActiveEquipmentMutationId);
		if (RecoveryResult.Code != EAIREInventoryMutationCode::Succeeded
			|| RecoveryItemId.IsNone())
		{
			ClearActiveEquipmentRequest();
			return;
		}

		const bool bRecoveryAccepted = RequestRuntimeEquipment(
			RecoveryItemId,
			EAIREEquipmentCallbackMode::Recovering,
			ActiveEquipmentSessionId,
			ActiveEquipmentMutationId);
		if (!bRecoveryAccepted
			&& EquipmentCallbackMode == EAIREEquipmentCallbackMode::Recovering)
		{
			GameplayInventory->CompleteMakoEquipmentRecovery(
				ActiveEquipmentSessionId,
				ActiveEquipmentMutationId,
				false);
			ClearActiveEquipmentRequest();
			OnWeaponEquipResult.Broadcast(RecoveryItemId, false);
		}
		return;
	}

	const FAIREInventoryMutationResult RecoveryResult =
		GameplayInventory->CompleteMakoEquipmentRecovery(
			ActiveEquipmentSessionId,
			ActiveEquipmentMutationId,
			bSucceeded);
	ClearActiveEquipmentRequest();
	OnWeaponEquipResult.Broadcast(
		ExpectedItemId,
		bSucceeded
			&& RecoveryResult.Code
				== EAIREInventoryMutationCode::Succeeded);
}

void UAIRECompanionInventoryComponent::HandleContainerChanged(
	const FName ContainerId,
	const int64 Revision)
{
	(void)Revision;
	if (ContainerId == UAIREGameplayInventorySubsystem::GetMakoContainerId())
	{
		FAIREInventoryContainerSnapshot Snapshot;
		if (GetInventorySnapshot(Snapshot)
			&& BoundInventorySessionId.IsValid()
			&& Snapshot.SessionId != BoundInventorySessionId)
		{
			if (EquipmentComponent.IsValid())
			{
				EquipmentComponent->UnequipCurrentWeapon();
			}
			ClearActiveEquipmentRequest();
			BoundInventorySessionId = Snapshot.SessionId;
		}
		OnInventoryChanged.Broadcast();
	}
}

bool UAIRECompanionInventoryComponent::RequestRuntimeEquipment(
	const FName ItemId,
	const EAIREEquipmentCallbackMode CallbackMode,
	const FGuid& SessionId,
	const FGuid& MutationId)
{
	ActiveEquipmentSessionId = SessionId;
	ActiveEquipmentMutationId = MutationId;
	EquipmentCallbackMode = CallbackMode;
	const UAIRECompanionItemDefinitionDataAsset* ItemDefinition =
		FindItemDefinition(ItemId);
	if (!EquipmentComponent.IsValid()
		|| !IsValid(ItemDefinition)
		|| !IsValid(ItemDefinition->WeaponDefinition))
	{
		return false;
	}

	return EquipmentComponent->EquipWeapon(ItemDefinition->WeaponDefinition);
}

void UAIRECompanionInventoryComponent::ClearActiveEquipmentRequest()
{
	ActiveEquipmentSessionId.Invalidate();
	ActiveEquipmentMutationId.Invalidate();
	EquipmentCallbackMode = EAIREEquipmentCallbackMode::None;
}

void UAIRECompanionInventoryComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownInventory();
	Super::EndPlay(EndPlayReason);
}

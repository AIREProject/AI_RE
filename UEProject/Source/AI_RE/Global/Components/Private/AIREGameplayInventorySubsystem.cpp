#include "AIREGameplayInventorySubsystem.h"

#include "AI_REItemDataAsset.h"
#include "AI_REItemSubsystem.h"
#include "AI_REPlayerCombatComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "AI_REWeaponItemDataAsset.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Engine/GameInstance.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"
#include "AIREGameplayInventorySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "Subsystems/SubsystemCollection.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#endif

namespace
{
	constexpr int32 FreshIronIngotCount = 3;
	constexpr int32 FreshWoodHandleCount = 1;
	const FName FreshIronIngotItemId(TEXT("IronIngot"));
	const FName FreshWoodHandleItemId(TEXT("WoodHandle"));

	FName NormalizeContainerId(const FName ContainerId)
	{
		static const FName LegacySharedStorageId(
			AIREGameplayInventory::LegacySharedStorageContainerId);
		return ContainerId == LegacySharedStorageId
			? FName(AIREGameplayInventory::SharedStorageContainerId)
			: ContainerId;
	}

#if WITH_DEV_AUTOMATION_TESTS
	int32 GInventoryAutomationBroadcastCount = 0;

	bool ResolveSyntheticItemRules(
		const FName ItemId,
		int32& OutMaxStackSize,
		bool& bOutIsCompanionItem,
		bool& bOutIsWeapon)
	{
		if (ItemId == FName(TEXT("AIRE.Test.Stack2")))
		{
			OutMaxStackSize = 2;
			bOutIsCompanionItem = true;
			bOutIsWeapon = false;
			return true;
		}
		if (ItemId == FName(TEXT("AIRE.Test.Stack4")))
		{
			OutMaxStackSize = 4;
			bOutIsCompanionItem = true;
			bOutIsWeapon = false;
			return true;
		}
		if (ItemId == FName(TEXT("AIRE.Test.GenericStack4")))
		{
			OutMaxStackSize = 4;
			bOutIsCompanionItem = false;
			bOutIsWeapon = false;
			return true;
		}
		if (ItemId == FreshIronIngotItemId
			|| ItemId == FreshWoodHandleItemId)
		{
			OutMaxStackSize = 99;
			bOutIsCompanionItem = false;
			bOutIsWeapon = false;
			return true;
		}
		if (ItemId == FName(TEXT("AIRE.Test.WeaponA"))
			|| ItemId == FName(TEXT("AIRE.Test.WeaponB")))
		{
			OutMaxStackSize = 1;
			bOutIsCompanionItem = true;
			bOutIsWeapon = true;
			return true;
		}
		if (ItemId.ToString().StartsWith(TEXT("AIRE.Test.Unique.")))
		{
			OutMaxStackSize = 1;
			bOutIsCompanionItem = true;
			bOutIsWeapon = false;
			return true;
		}
		return false;
	}
#endif

	int32 FindStackIndexBySlot(
		const TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const int32 SlotIndex)
	{
		return Stacks.IndexOfByPredicate(
			[SlotIndex](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.SlotIndex == SlotIndex;
			});
	}

	int32 FindFirstEmptySlot(
		const TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const int32 Capacity)
	{
		for (int32 SlotIndex = 0; SlotIndex < Capacity; ++SlotIndex)
		{
			if (FindStackIndexBySlot(Stacks, SlotIndex) == INDEX_NONE)
			{
				return SlotIndex;
			}
		}
		return INDEX_NONE;
	}

	void SortStacks(TArray<FAIREInventoryItemStackSnapshot>& Stacks)
	{
		Stacks.Sort(
			[](const FAIREInventoryItemStackSnapshot& Left,
				const FAIREInventoryItemStackSnapshot& Right)
			{
				return Left.SlotIndex < Right.SlotIndex;
			});
	}

	bool TryAddToStacks(
		TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const int32 Capacity,
		const FName ItemId,
		const int32 Count,
		const int32 MaxStackSize)
	{
		if (ItemId.IsNone()
			|| Count <= 0
			|| MaxStackSize < 1
			|| Capacity < 1)
		{
			return false;
		}

		int64 FreeCapacity = 0;
		for (const FAIREInventoryItemStackSnapshot& Stack : Stacks)
		{
			if (Stack.ItemId == ItemId)
			{
				FreeCapacity += FMath::Max(0, MaxStackSize - Stack.Count);
			}
		}
		FreeCapacity += static_cast<int64>(FMath::Max(0, Capacity - Stacks.Num()))
			* MaxStackSize;
		if (FreeCapacity < Count)
		{
			return false;
		}

		int32 RemainingCount = Count;
		SortStacks(Stacks);
		for (FAIREInventoryItemStackSnapshot& Stack : Stacks)
		{
			if (Stack.ItemId != ItemId || Stack.Count >= MaxStackSize)
			{
				continue;
			}

			const int32 AddedCount = FMath::Min(
				RemainingCount,
				MaxStackSize - Stack.Count);
			Stack.Count += AddedCount;
			RemainingCount -= AddedCount;
			if (RemainingCount == 0)
			{
				break;
			}
		}

		while (RemainingCount > 0)
		{
			const int32 EmptySlot = FindFirstEmptySlot(Stacks, Capacity);
			if (EmptySlot == INDEX_NONE)
			{
				return false;
			}

			FAIREInventoryItemStackSnapshot& NewStack =
				Stacks.AddDefaulted_GetRef();
			NewStack.SlotIndex = EmptySlot;
			NewStack.ItemId = ItemId;
			NewStack.Count = FMath::Min(RemainingCount, MaxStackSize);
			RemainingCount -= NewStack.Count;
		}

		SortStacks(Stacks);
		return true;
	}

	bool TryRemoveItemFromStacks(
		TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const FName ItemId,
		const int32 Count)
	{
		if (ItemId.IsNone() || Count <= 0)
		{
			return false;
		}

		int64 AvailableCount = 0;
		for (const FAIREInventoryItemStackSnapshot& Stack : Stacks)
		{
			if (Stack.ItemId == ItemId)
			{
				AvailableCount += Stack.Count;
			}
		}
		if (AvailableCount < Count)
		{
			return false;
		}

		SortStacks(Stacks);
		int32 RemainingCount = Count;
		for (int32 Index = Stacks.Num() - 1;
			Index >= 0 && RemainingCount > 0;
			--Index)
		{
			FAIREInventoryItemStackSnapshot& Stack = Stacks[Index];
			if (Stack.ItemId != ItemId)
			{
				continue;
			}

			const int32 RemovedCount = FMath::Min(Stack.Count, RemainingCount);
			Stack.Count -= RemovedCount;
			RemainingCount -= RemovedCount;
			if (Stack.Count == 0)
			{
				Stacks.RemoveAt(Index);
			}
		}
		return true;
	}

	int64 CountItemInStacks(
		const TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const FName ItemId)
	{
		int64 Count = 0;
		for (const FAIREInventoryItemStackSnapshot& Stack : Stacks)
		{
			if (Stack.ItemId == ItemId)
			{
				Count += Stack.Count;
			}
		}
		return Count;
	}

	bool TryRemoveLocalFirst(
		TArray<FAIREInventoryItemStackSnapshot>& LocalStacks,
		TArray<FAIREInventoryItemStackSnapshot>& StorageStacks,
		const FName ItemId,
		const int32 Count,
		bool& bOutLocalChanged,
		bool& bOutStorageChanged)
	{
		const int32 LocalCount = static_cast<int32>(FMath::Min<int64>(
			CountItemInStacks(LocalStacks, ItemId),
			Count));
		if (LocalCount > 0)
		{
			if (!TryRemoveItemFromStacks(LocalStacks, ItemId, LocalCount))
			{
				return false;
			}
			bOutLocalChanged = true;
		}

		const int32 StorageCount = Count - LocalCount;
		if (StorageCount > 0)
		{
			if (!TryRemoveItemFromStacks(
					StorageStacks,
					ItemId,
					StorageCount))
			{
				return false;
			}
			bOutStorageChanged = true;
		}
		return true;
	}

	void CopyPlayerItemsToSnapshots(
		const TArray<FInventoryItemStack>& PlayerItems,
		TArray<FAIREInventoryItemStackSnapshot>& OutStacks)
	{
		OutStacks.Reset(PlayerItems.Num());
		for (const FInventoryItemStack& PlayerItem : PlayerItems)
		{
			FAIREInventoryItemStackSnapshot& Stack =
				OutStacks.AddDefaulted_GetRef();
			Stack.SlotIndex = PlayerItem.SlotIndex;
			Stack.ItemId = PlayerItem.ItemId;
			Stack.Count = PlayerItem.Count;
		}
	}

	void CopySnapshotsToPlayerItems(
		const TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		TArray<FInventoryItemStack>& OutPlayerItems)
	{
		OutPlayerItems.Reset(Stacks.Num());
		for (const FAIREInventoryItemStackSnapshot& Stack : Stacks)
		{
			FInventoryItemStack& PlayerItem =
				OutPlayerItems.AddDefaulted_GetRef();
			PlayerItem.SlotIndex = Stack.SlotIndex;
			PlayerItem.ItemId = Stack.ItemId;
			PlayerItem.Count = Stack.Count;
		}
	}

	bool TryRemoveFromSlot(
		TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const int32 SlotIndex,
		const int32 Count,
		FName& OutItemId)
	{
		OutItemId = NAME_None;
		if (SlotIndex < 0 || Count <= 0)
		{
			return false;
		}

		const int32 StackIndex = FindStackIndexBySlot(Stacks, SlotIndex);
		if (StackIndex == INDEX_NONE
			|| Stacks[StackIndex].ItemId.IsNone()
			|| Stacks[StackIndex].Count < Count)
		{
			return false;
		}

		OutItemId = Stacks[StackIndex].ItemId;
		Stacks[StackIndex].Count -= Count;
		if (Stacks[StackIndex].Count == 0)
		{
			Stacks.RemoveAt(StackIndex);
		}
		return true;
	}

	bool TryMoveWithinStacks(
		TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const int32 Capacity,
		const int32 SourceSlotIndex,
		const int32 DestinationSlotIndex,
		const int32 Count,
		const int32 MaxStackSize)
	{
		if (SourceSlotIndex < 0
			|| SourceSlotIndex >= Capacity
			|| DestinationSlotIndex < 0
			|| DestinationSlotIndex >= Capacity
			|| SourceSlotIndex == DestinationSlotIndex
			|| Count <= 0)
		{
			return false;
		}

		const int32 SourceIndex = FindStackIndexBySlot(Stacks, SourceSlotIndex);
		if (SourceIndex == INDEX_NONE || Stacks[SourceIndex].Count < Count)
		{
			return false;
		}

		const int32 DestinationIndex =
			FindStackIndexBySlot(Stacks, DestinationSlotIndex);
		if (DestinationIndex == INDEX_NONE)
		{
			if (Stacks[SourceIndex].Count == Count)
			{
				Stacks[SourceIndex].SlotIndex = DestinationSlotIndex;
			}
			else
			{
				Stacks[SourceIndex].Count -= Count;
				FAIREInventoryItemStackSnapshot& NewStack =
					Stacks.AddDefaulted_GetRef();
				NewStack.SlotIndex = DestinationSlotIndex;
				NewStack.ItemId = Stacks[SourceIndex].ItemId;
				NewStack.Count = Count;
			}
			SortStacks(Stacks);
			return true;
		}

		if (Stacks[DestinationIndex].ItemId == Stacks[SourceIndex].ItemId)
		{
			if (Stacks[DestinationIndex].Count + Count > MaxStackSize)
			{
				return false;
			}
			Stacks[DestinationIndex].Count += Count;
			Stacks[SourceIndex].Count -= Count;
			if (Stacks[SourceIndex].Count == 0)
			{
				Stacks.RemoveAt(SourceIndex);
			}
			return true;
		}

		if (Stacks[SourceIndex].Count != Count)
		{
			return false;
		}
		Swap(Stacks[SourceIndex].SlotIndex, Stacks[DestinationIndex].SlotIndex);
		SortStacks(Stacks);
		return true;
	}

	bool IsStableId(const FString& Value)
	{
		if (Value.IsEmpty()
			|| Value.Len() > AIREGameplayInventory::MaxStableIdLength)
		{
			return false;
		}

		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			const bool bIsAllowed = FChar::IsAlnum(Character)
				|| Character == TEXT('.')
				|| Character == TEXT('_')
				|| Character == TEXT(':')
				|| Character == TEXT('-');
			if (!bIsAllowed || (Index == 0 && !FChar::IsAlnum(Character)))
			{
				return false;
			}
		}
		return true;
	}
}

void UAIREGameplayInventorySubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAI_REItemSubsystem>();
	SessionScope = MakeCanonicalPersistenceScope();
	InventorySessionId = FGuid::NewGuid();
	CreateEmptyContainers();
	RegisteredPlayerInventory.Reset();
	RegisteredPlayerCombat.Reset();
	CachedPlayerPersistenceState = FAIREInventoryPersistedPlayerState();
	bHasPlayerPersistenceState = false;
	bApplyingPlayerPersistenceState = false;
	bPersistenceLifecycleInitialized = true;
	bPersistenceShuttingDown = false;
	bPersistenceLoadComplete = false;
	bPersistenceReady = false;
	bPersistenceDirty = false;
	bPersistenceSaveInFlight = false;
	LastPersistenceLoadResult = MakePersistenceResult(
		EAIREInventoryPersistenceOperation::Load,
		EAIREInventoryPersistenceResultCode::InProgress);
	LastPersistenceSaveResult = MakePersistenceResult(
		EAIREInventoryPersistenceOperation::Save,
		EAIREInventoryPersistenceResultCode::NotStarted);
	BeginPersistenceLoad();
}

void UAIREGameplayInventorySubsystem::Deinitialize()
{
	if (RegisteredPlayerInventory.IsValid())
	{
		EAIREInventoryPersistenceResultCode CaptureCode =
			EAIREInventoryPersistenceResultCode::NotStarted;
		FAIREInventoryPersistedPlayerState PlayerState;
		if (CapturePlayerPersistenceState(
				*RegisteredPlayerInventory.Get(),
				PlayerState,
				CaptureCode))
		{
			CachedPlayerPersistenceState = MoveTemp(PlayerState);
			bHasPlayerPersistenceState = true;
		}
	}
	if (bPersistenceDirty && bPersistenceReady)
	{
		const int64 ShutdownGeneration =
			HighestIssuedPersistenceGeneration < MAX_int64
			? HighestIssuedPersistenceGeneration + 1
			: 0;
		FAIREInventorySaveEnvelope Envelope;
		EAIREInventoryPersistenceResultCode BuildCode =
			EAIREInventoryPersistenceResultCode::NotStarted;
		if (ShutdownGeneration > 0
			&& BuildPersistenceEnvelope(
				ShutdownGeneration,
				Envelope,
				BuildCode))
		{
			UAIREGameplayInventorySaveGame* SaveGame =
				NewObject<UAIREGameplayInventorySaveGame>();
			SaveGame->Envelope = MoveTemp(Envelope);
			UGameplayStatics::AsyncSaveGameToSlot(
				SaveGame,
				GetNextPersistenceSlotName(),
				AIREGameplayInventoryPersistence::UserIndex);
		}
	}

	bPersistenceShuttingDown = true;
	bPersistenceLifecycleInitialized = false;
	++PersistenceEpoch;
	++ActiveSaveEpoch;
	PersistenceReadyDelegate.Clear();
	PersistenceSaveCompletedDelegate.Clear();
	OnContainerChanged.Clear();
	Containers.Reset();
	AppliedMutations.Reset();
	AppliedMutationOrder.Reset();
	TransientAppliedMutations.Reset();
	TransientAppliedMutationOrder.Reset();
	AppliedWorkResults.Reset();
	AppliedWorkResultOrder.Reset();
	TransientAppliedWorkResults.Reset();
	TransientAppliedWorkResultOrder.Reset();
	AppliedImportCandidateIds.Reset();
	AppliedImportCandidateOrder.Reset();
	AppliedImportOperationIds.Reset();
	AppliedImportOperationOrder.Reset();
	AppliedOfflineTaskIds.Reset();
	AppliedOfflineTaskOrder.Reset();
	PersistenceLoadSlots.Reset();
	PendingCompanionConfig.Reset();
	RegisteredPlayerInventory.Reset();
	RegisteredPlayerCombat.Reset();
	CachedPlayerPersistenceState = FAIREInventoryPersistedPlayerState();
	bHasPlayerPersistenceState = false;
	bApplyingPlayerPersistenceState = false;
	SessionScope = FAIREInventorySessionScope();
	InventorySessionId.Invalidate();
	bPersistenceReady = false;
	bPersistenceLoadComplete = false;
	bPersistenceSaveInFlight = false;
	bMakoInventoryInitialized = false;
	bShouldSeedFreshSharedStorage = false;
	Super::Deinitialize();
}

bool UAIREGameplayInventorySubsystem::IsPersistenceReady() const
{
	return bPersistenceReady && !bPersistenceShuttingDown;
}

FAIREInventoryPersistenceResult
UAIREGameplayInventorySubsystem::GetLastPersistenceLoadResult() const
{
	return LastPersistenceLoadResult;
}

FAIREInventoryPersistenceResult
UAIREGameplayInventorySubsystem::GetLastPersistenceSaveResult() const
{
	return LastPersistenceSaveResult;
}

FAIREInventoryPersistenceResult
UAIREGameplayInventorySubsystem::RequestInventorySave()
{
	if (bPersistenceShuttingDown)
	{
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::ShuttingDown,
			LatestPersistenceGeneration);
		return LastPersistenceSaveResult;
	}
	if (!bPersistenceReady)
	{
		bPersistenceDirty = true;
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::InProgress,
			LatestPersistenceGeneration);
		return LastPersistenceSaveResult;
	}
	if (bPersistenceSaveInFlight)
	{
		bPersistenceDirty = true;
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::Coalesced,
			HighestIssuedPersistenceGeneration);
		return LastPersistenceSaveResult;
	}

	bPersistenceDirty = true;
	return TryStartPersistenceSave();
}

FAIREInventoryPersistenceReady&
UAIREGameplayInventorySubsystem::OnPersistenceReady()
{
	return PersistenceReadyDelegate;
}

FAIREInventoryPersistenceSaveCompleted&
UAIREGameplayInventorySubsystem::OnPersistenceSaveCompleted()
{
	return PersistenceSaveCompletedDelegate;
}

bool UAIREGameplayInventorySubsystem::RegisterPlayerInventory(
	UAI_REPlayerInventoryComponent* PlayerInventory,
	UAI_REPlayerCombatComponent* PlayerCombat)
{
	if (!IsValid(PlayerInventory)
		|| PlayerInventory->MaxSlots
			!= AIREGameplayInventoryPersistence::PlayerInventoryCapacity)
	{
		return false;
	}
	if (RegisteredPlayerInventory.IsValid()
		&& RegisteredPlayerInventory.Get() != PlayerInventory)
	{
		UAI_REPlayerInventoryComponent* PreviousPlayer =
			RegisteredPlayerInventory.Get();
		if (bPersistenceReady)
		{
			EAIREInventoryPersistenceResultCode CaptureCode =
				EAIREInventoryPersistenceResultCode::NotStarted;
			FAIREInventoryPersistedPlayerState PreviousState;
			if (!CapturePlayerPersistenceState(
					*PreviousPlayer,
					PreviousState,
					CaptureCode))
			{
				return false;
			}
			CachedPlayerPersistenceState = MoveTemp(PreviousState);
			bHasPlayerPersistenceState = true;
		}
		PreviousPlayer->bPersistenceReadyForGameplay = false;
	}

	RegisteredPlayerInventory = PlayerInventory;
	RegisteredPlayerCombat = PlayerCombat;
	PlayerInventory->bPersistenceReadyForGameplay = false;
	if (bPersistenceReady)
	{
		ApplyOrInitializeRegisteredPlayerState();
		if (bPersistenceDirty)
		{
			TryStartPersistenceSave();
		}
	}
	return true;
}

void UAIREGameplayInventorySubsystem::UnregisterPlayerInventory(
	UAI_REPlayerInventoryComponent* PlayerInventory)
{
	if (!IsValid(PlayerInventory)
		|| RegisteredPlayerInventory.Get() != PlayerInventory)
	{
		return;
	}

	EAIREInventoryPersistenceResultCode CaptureCode =
		EAIREInventoryPersistenceResultCode::NotStarted;
	FAIREInventoryPersistedPlayerState PlayerState;
	if (CapturePlayerPersistenceState(
			*PlayerInventory,
			PlayerState,
			CaptureCode))
	{
		CachedPlayerPersistenceState = MoveTemp(PlayerState);
		bHasPlayerPersistenceState = true;
		if (bPersistenceReady)
		{
			MarkPersistenceDirty();
		}
	}
	RegisteredPlayerInventory.Reset();
	RegisteredPlayerCombat.Reset();
}

void UAIREGameplayInventorySubsystem::NotifyPlayerInventoryChanged(
	UAI_REPlayerInventoryComponent* PlayerInventory)
{
	if (bApplyingPlayerPersistenceState
		|| !IsValid(PlayerInventory)
		|| RegisteredPlayerInventory.Get() != PlayerInventory)
	{
		return;
	}

	EAIREInventoryPersistenceResultCode CaptureCode =
		EAIREInventoryPersistenceResultCode::NotStarted;
	FAIREInventoryPersistedPlayerState PlayerState;
	if (!CapturePlayerPersistenceState(
			*PlayerInventory,
			PlayerState,
			CaptureCode))
	{
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			CaptureCode,
			LatestPersistenceGeneration);
		return;
	}

	CachedPlayerPersistenceState = MoveTemp(PlayerState);
	bHasPlayerPersistenceState = true;
	MarkPersistenceDirty();
}

FGuid UAIREGameplayInventorySubsystem::GetInventorySessionId() const
{
	return InventorySessionId;
}

FName UAIREGameplayInventorySubsystem::GetMakoContainerId()
{
	return FName(AIREGameplayInventory::MakoContainerId);
}

FName UAIREGameplayInventorySubsystem::GetSharedStorageContainerId()
{
	return FName(AIREGameplayInventory::SharedStorageContainerId);
}

bool UAIREGameplayInventorySubsystem::GetContainerSnapshot(
	const FName ContainerId,
	FAIREInventoryContainerSnapshot& OutSnapshot) const
{
	const FAIREContainerState* Container = FindContainer(ContainerId);
	if (!bPersistenceReady
		|| !Container
		|| !InventorySessionId.IsValid())
	{
		OutSnapshot = FAIREInventoryContainerSnapshot();
		return false;
	}

	OutSnapshot.SessionId = InventorySessionId;
	OutSnapshot.ContainerId = Container->ContainerId;
	OutSnapshot.Revision = Container->Revision;
	OutSnapshot.Capacity = Container->Capacity;
	OutSnapshot.ItemStacks = Container->ItemStacks;
	SortStacks(OutSnapshot.ItemStacks);
	OutSnapshot.Equipment.EquippedItemId = Container->EquippedItemId;
	OutSnapshot.Equipment.PendingItemId = Container->PendingItemId;
	OutSnapshot.Equipment.TransitionState = Container->EquipmentTransition;
	return true;
}

bool UAIREGameplayInventorySubsystem::GetPlayerPersistenceSnapshot(
	FAIREInventoryPersistedPlayerState& OutSnapshot) const
{
	OutSnapshot = FAIREInventoryPersistedPlayerState();
	if (!bPersistenceReady)
	{
		return false;
	}
	if (RegisteredPlayerInventory.IsValid())
	{
		EAIREInventoryPersistenceResultCode Code =
			EAIREInventoryPersistenceResultCode::NotStarted;
		return CapturePlayerPersistenceState(
			*RegisteredPlayerInventory.Get(),
			OutSnapshot,
			Code);
	}
	EAIREInventoryPersistenceResultCode Code =
		EAIREInventoryPersistenceResultCode::NotStarted;
	return ValidatePlayerPersistenceState(
		CachedPlayerPersistenceState,
		OutSnapshot,
		Code);
}

FAIREInventoryMutationResult UAIREGameplayInventorySubsystem::TryAddItem(
	const FAIREInventoryMutationRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}

	FAIREContainerState* Container = FindContainer(Request.ContainerId);
	FAIREInventoryMutationResult Validation = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		Container,
		Request.ExpectedRevision);
	if (Validation.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Validation;
	}
	if (Request.ItemId.IsNone() || Request.Count <= 0)
	{
		return MakeResult(
			Request.ItemId.IsNone()
				? EAIREInventoryMutationCode::InvalidItem
				: EAIREInventoryMutationCode::InvalidQuantity,
			Request.MutationId);
	}
	if (IsEquipmentTransitionActive(*Container)
		&& Container->PendingItemId == Request.ItemId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			Request.MutationId,
			Container->Revision);
	}

	FAIREItemRules Rules;
	if (!ResolveItemRules(Request.ItemId, false, Rules))
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidItem,
			Request.MutationId,
			Container->Revision);
	}

	TArray<FAIREInventoryItemStackSnapshot> NewStacks = Container->ItemStacks;
	if (!TryAddToStacks(
			NewStacks,
			Container->Capacity,
			Request.ItemId,
			Request.Count,
			Rules.MaxStackSize))
	{
		return MakeResult(
			EAIREInventoryMutationCode::CapacityExceeded,
			Request.MutationId,
			Container->Revision);
	}

	Container->ItemStacks = MoveTemp(NewStacks);
	++Container->Revision;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		Container->Revision);
	RecordAppliedMutation(Result, true);
	BroadcastContainerChanged(*Container);
	return Result;
}

FAIREInventoryMutationResult UAIREGameplayInventorySubsystem::TryRemoveItem(
	const FAIREInventoryMutationRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}

	FAIREContainerState* Container = FindContainer(Request.ContainerId);
	FAIREInventoryMutationResult Validation = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		Container,
		Request.ExpectedRevision);
	if (Validation.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Validation;
	}
	if (Request.ItemId.IsNone() || Request.Count <= 0)
	{
		return MakeResult(
			Request.ItemId.IsNone()
				? EAIREInventoryMutationCode::InvalidItem
				: EAIREInventoryMutationCode::InvalidQuantity,
			Request.MutationId);
	}
	if (IsEquipmentTransitionActive(*Container)
		&& Container->PendingItemId == Request.ItemId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			Request.MutationId,
			Container->Revision);
	}

	FAIREItemRules Rules;
	if (!ResolveItemRules(Request.ItemId, false, Rules))
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidItem,
			Request.MutationId,
			Container->Revision);
	}

	TArray<FAIREInventoryItemStackSnapshot> NewStacks = Container->ItemStacks;
	if (!TryRemoveItemFromStacks(NewStacks, Request.ItemId, Request.Count))
	{
		return MakeResult(
			EAIREInventoryMutationCode::InsufficientQuantity,
			Request.MutationId,
			Container->Revision);
	}

	Container->ItemStacks = MoveTemp(NewStacks);
	++Container->Revision;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		Container->Revision);
	RecordAppliedMutation(Result, true);
	BroadcastContainerChanged(*Container);
	return Result;
}

FAIREInventoryMutationResult UAIREGameplayInventorySubsystem::TryMoveItem(
	const FAIREInventoryMoveRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}

	FAIREContainerState* Container = FindContainer(Request.ContainerId);
	FAIREInventoryMutationResult Validation = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		Container,
		Request.ExpectedRevision);
	if (Validation.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Validation;
	}
	if (Request.Count <= 0)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidQuantity,
			Request.MutationId,
			Container->Revision);
	}
	if (IsEquipmentTransitionActive(*Container)
		&& (Container->ReservedSlotIndex == Request.SourceSlotIndex
			|| Container->ReservedSlotIndex == Request.DestinationSlotIndex))
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			Request.MutationId,
			Container->Revision);
	}

	const int32 SourceIndex = FindStackIndexBySlot(
		Container->ItemStacks,
		Request.SourceSlotIndex);
	if (SourceIndex == INDEX_NONE)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSlot,
			Request.MutationId,
			Container->Revision);
	}

	FAIREItemRules Rules;
	const FName ItemId = Container->ItemStacks[SourceIndex].ItemId;
	if (!ResolveItemRules(ItemId, false, Rules))
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidItem,
			Request.MutationId,
			Container->Revision);
	}

	TArray<FAIREInventoryItemStackSnapshot> NewStacks = Container->ItemStacks;
	if (!TryMoveWithinStacks(
			NewStacks,
			Container->Capacity,
			Request.SourceSlotIndex,
			Request.DestinationSlotIndex,
			Request.Count,
			Rules.MaxStackSize))
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSlot,
			Request.MutationId,
			Container->Revision);
	}

	Container->ItemStacks = MoveTemp(NewStacks);
	++Container->Revision;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		Container->Revision);
	RecordAppliedMutation(Result, true);
	BroadcastContainerChanged(*Container);
	return Result;
}

FAIREInventoryMutationResult UAIREGameplayInventorySubsystem::TryTransferItem(
	const FAIREInventoryTransferRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}

	FAIREContainerState* Source = FindContainer(Request.SourceContainerId);
	FAIREContainerState* Destination = FindContainer(Request.DestinationContainerId);
	FAIREInventoryMutationResult Validation = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		Source,
		Request.ExpectedSourceRevision);
	if (Validation.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Validation;
	}
	if (!Destination || Source == Destination)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidContainer,
			Request.MutationId,
			Source->Revision);
	}
	if (Destination->Revision != Request.ExpectedDestinationRevision)
	{
		return MakeResult(
			EAIREInventoryMutationCode::RevisionConflict,
			Request.MutationId,
			Source->Revision,
			Destination->Revision);
	}
	if (Request.Count <= 0)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidQuantity,
			Request.MutationId,
			Source->Revision,
			Destination->Revision);
	}
	if (IsEquipmentTransitionActive(*Source)
		&& Source->ReservedSlotIndex == Request.SourceSlotIndex)
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			Request.MutationId,
			Source->Revision,
			Destination->Revision);
	}
	if (Request.SourceSlotIndex < 0
		|| Request.SourceSlotIndex >= Source->Capacity
		|| FindStackIndexBySlot(Source->ItemStacks, Request.SourceSlotIndex)
			== INDEX_NONE)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSlot,
			Request.MutationId,
			Source->Revision,
			Destination->Revision);
	}

	TArray<FAIREInventoryItemStackSnapshot> NewSourceStacks = Source->ItemStacks;
	FName ItemId;
	if (!TryRemoveFromSlot(
			NewSourceStacks,
			Request.SourceSlotIndex,
			Request.Count,
			ItemId))
	{
		return MakeResult(
			EAIREInventoryMutationCode::InsufficientQuantity,
			Request.MutationId,
			Source->Revision,
			Destination->Revision);
	}

	FAIREItemRules Rules;
	if (!ResolveItemRules(ItemId, false, Rules))
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidItem,
			Request.MutationId,
			Source->Revision,
			Destination->Revision);
	}
	if (IsEquipmentTransitionActive(*Destination)
		&& Destination->PendingItemId == ItemId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			Request.MutationId,
			Source->Revision,
			Destination->Revision);
	}

	TArray<FAIREInventoryItemStackSnapshot> NewDestinationStacks =
		Destination->ItemStacks;
	if (!TryAddToStacks(
			NewDestinationStacks,
			Destination->Capacity,
			ItemId,
			Request.Count,
			Rules.MaxStackSize))
	{
		return MakeResult(
			EAIREInventoryMutationCode::CapacityExceeded,
			Request.MutationId,
			Source->Revision,
			Destination->Revision);
	}

	Source->ItemStacks = MoveTemp(NewSourceStacks);
	Destination->ItemStacks = MoveTemp(NewDestinationStacks);
	++Source->Revision;
	++Destination->Revision;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		Source->Revision,
		Destination->Revision);
	RecordAppliedMutation(Result, true);
	BroadcastContainerChanged(*Source);
	BroadcastContainerChanged(*Destination);
	return Result;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::TryTransferPlayerStorage(
	UAI_REPlayerInventoryComponent* PlayerInventory,
	const FAIREPlayerStorageTransferRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}

	FAIREContainerState* Storage = FindContainer(
		GetSharedStorageContainerId());
	FAIREInventoryMutationResult Validation = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		Storage,
		Request.ExpectedStorageRevision);
	if (Validation.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Validation;
	}
	if (!IsValid(PlayerInventory))
	{
		return MakeResult(
			EAIREInventoryMutationCode::NotInitialized,
			Request.MutationId,
			Storage->Revision);
	}
	if (Request.Count <= 0)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidQuantity,
			Request.MutationId,
			Storage->Revision);
	}
	if (Request.Direction
			!= EAIREPlayerStorageTransferDirection::DepositPlayerToStorage
		&& Request.Direction
			!= EAIREPlayerStorageTransferDirection::WithdrawStorageToPlayer)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidOperation,
			Request.MutationId,
			Storage->Revision);
	}

	TArray<FInventoryItemStack> NewPlayerItems;
	TArray<FAIREInventoryItemStackSnapshot> NewStorageStacks =
		Storage->ItemStacks;
	FName ItemId;
	if (Request.Direction
		== EAIREPlayerStorageTransferDirection::DepositPlayerToStorage)
	{
		const int32 PlayerStackIndex =
			PlayerInventory->FindStackIndexBySlot(Request.SourceSlotIndex);
		if (Request.SourceSlotIndex < 0
			|| Request.SourceSlotIndex >= PlayerInventory->MaxSlots
			|| PlayerStackIndex == INDEX_NONE)
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidSlot,
				Request.MutationId,
				Storage->Revision);
		}
		if (PlayerInventory->Items[PlayerStackIndex].Count < Request.Count)
		{
			return MakeResult(
				EAIREInventoryMutationCode::InsufficientQuantity,
				Request.MutationId,
				Storage->Revision);
		}
		FAIREItemRules Rules;
		if (!ResolveItemRules(
				PlayerInventory->Items[PlayerStackIndex].ItemId,
				false,
				Rules))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidItem,
				Request.MutationId,
				Storage->Revision);
		}
		if (!PlayerInventory->BuildExactRemoveFromSlotState(
				Request.SourceSlotIndex,
				Request.Count,
				NewPlayerItems,
				ItemId))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InsufficientQuantity,
				Request.MutationId,
				Storage->Revision);
		}

		if (!TryAddToStacks(
				NewStorageStacks,
				Storage->Capacity,
				ItemId,
				Request.Count,
				Rules.MaxStackSize))
		{
			return MakeResult(
				EAIREInventoryMutationCode::CapacityExceeded,
				Request.MutationId,
				Storage->Revision);
		}
	}
	else
	{
		if (Request.SourceSlotIndex < 0
			|| Request.SourceSlotIndex >= Storage->Capacity
			|| FindStackIndexBySlot(
				Storage->ItemStacks,
				Request.SourceSlotIndex) == INDEX_NONE)
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidSlot,
				Request.MutationId,
				Storage->Revision);
		}
		if (!TryRemoveFromSlot(
				NewStorageStacks,
				Request.SourceSlotIndex,
				Request.Count,
				ItemId))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InsufficientQuantity,
				Request.MutationId,
				Storage->Revision);
		}
		FAIREItemRules Rules;
		if (!ResolveItemRules(ItemId, false, Rules))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidItem,
				Request.MutationId,
				Storage->Revision);
		}
		if (!PlayerInventory->BuildExactAddState(
				ItemId,
				Request.Count,
				NewPlayerItems))
		{
			return MakeResult(
				EAIREInventoryMutationCode::CapacityExceeded,
				Request.MutationId,
				Storage->Revision);
		}
	}

	PlayerInventory->CommitExactInventoryState(MoveTemp(NewPlayerItems));
	Storage->ItemStacks = MoveTemp(NewStorageStacks);
	++Storage->Revision;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		Storage->Revision);
	RecordAppliedMutation(Result, true);
	PlayerInventory->NotifyExactInventoryMutation();
	BroadcastContainerChanged(*Storage);
	return Result;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::TryTransferPlayerMako(
	UAI_REPlayerInventoryComponent* PlayerInventory,
	const FAIREPlayerMakoTransferRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}

	FAIREContainerState* Mako = FindContainer(GetMakoContainerId());
	FAIREInventoryMutationResult Validation = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		Mako,
		Request.ExpectedMakoRevision);
	if (Validation.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Validation;
	}
	if (!IsValid(PlayerInventory)
		|| !PlayerInventory->bPersistenceReadyForGameplay)
	{
		return MakeResult(
			EAIREInventoryMutationCode::NotInitialized,
			Request.MutationId,
			Mako->Revision);
	}
	if (PlayerInventory->GetInventoryRevision()
		!= Request.ExpectedPlayerRevision)
	{
		return MakeResult(
			EAIREInventoryMutationCode::RevisionConflict,
			Request.MutationId,
			Mako->Revision,
			PlayerInventory->GetInventoryRevision());
	}
	if (Request.Count <= 0)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidQuantity,
			Request.MutationId,
			Mako->Revision,
			PlayerInventory->GetInventoryRevision());
	}
	if (Request.Direction != EAIREPlayerMakoTransferDirection::PlayerToMako
		&& Request.Direction
			!= EAIREPlayerMakoTransferDirection::MakoToPlayer)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidOperation,
			Request.MutationId,
			Mako->Revision,
			PlayerInventory->GetInventoryRevision());
	}
	if (IsEquipmentTransitionActive(*Mako))
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			Request.MutationId,
			Mako->Revision,
			PlayerInventory->GetInventoryRevision());
	}

	TArray<FInventoryItemStack> NewPlayerItems;
	TArray<FAIREInventoryItemStackSnapshot> NewMakoStacks =
		Mako->ItemStacks;
	FName ItemId;
	if (Request.Direction
		== EAIREPlayerMakoTransferDirection::PlayerToMako)
	{
		const int32 PlayerStackIndex =
			PlayerInventory->FindStackIndexBySlot(Request.SourceSlotIndex);
		if (Request.SourceSlotIndex < 0
			|| Request.SourceSlotIndex >= PlayerInventory->MaxSlots
			|| PlayerStackIndex == INDEX_NONE)
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidSlot,
				Request.MutationId,
				PlayerInventory->GetInventoryRevision(),
				Mako->Revision);
		}

		const FInventoryItemStack& SourceStack =
			PlayerInventory->Items[PlayerStackIndex];
		FAIREItemRules Rules;
		if (SourceStack.Count < Request.Count
			|| !ResolveItemRules(SourceStack.ItemId, false, Rules))
		{
			return MakeResult(
				SourceStack.Count < Request.Count
					? EAIREInventoryMutationCode::InsufficientQuantity
					: EAIREInventoryMutationCode::InvalidItem,
				Request.MutationId,
				PlayerInventory->GetInventoryRevision(),
				Mako->Revision);
		}
		if (!PlayerInventory->BuildExactRemoveFromSlotState(
				Request.SourceSlotIndex,
				Request.Count,
				NewPlayerItems,
				ItemId))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InsufficientQuantity,
				Request.MutationId,
				PlayerInventory->GetInventoryRevision(),
				Mako->Revision);
		}
		if (!TryAddToStacks(
				NewMakoStacks,
				Mako->Capacity,
				ItemId,
				Request.Count,
				Rules.MaxStackSize))
		{
			return MakeResult(
				EAIREInventoryMutationCode::CapacityExceeded,
				Request.MutationId,
				PlayerInventory->GetInventoryRevision(),
				Mako->Revision);
		}
	}
	else
	{
		if (Request.SourceSlotIndex < 0
			|| Request.SourceSlotIndex >= Mako->Capacity
			|| !TryRemoveFromSlot(
				NewMakoStacks,
				Request.SourceSlotIndex,
				Request.Count,
				ItemId))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidSlot,
				Request.MutationId,
				Mako->Revision,
				PlayerInventory->GetInventoryRevision());
		}

		FAIREItemRules Rules;
		if (!ResolveItemRules(ItemId, false, Rules))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidItem,
				Request.MutationId,
				Mako->Revision,
				PlayerInventory->GetInventoryRevision());
		}
		if (!PlayerInventory->BuildExactAddState(
				ItemId,
				Request.Count,
				NewPlayerItems))
		{
			return MakeResult(
				EAIREInventoryMutationCode::CapacityExceeded,
				Request.MutationId,
				Mako->Revision,
				PlayerInventory->GetInventoryRevision());
		}
	}

	PlayerInventory->CommitExactInventoryState(MoveTemp(NewPlayerItems));
	Mako->ItemStacks = MoveTemp(NewMakoStacks);
	++Mako->Revision;
	const bool bPlayerWasSource = Request.Direction
		== EAIREPlayerMakoTransferDirection::PlayerToMako;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		bPlayerWasSource
			? PlayerInventory->GetInventoryRevision()
			: Mako->Revision,
		bPlayerWasSource
			? Mako->Revision
			: PlayerInventory->GetInventoryRevision());
	RecordAppliedMutation(Result, true);
	PlayerInventory->NotifyExactInventoryMutation();
	BroadcastContainerChanged(*Mako);
	return Result;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::TryEquipPlayerWeapon(
	UAI_REPlayerInventoryComponent* PlayerInventory,
	UAI_REPlayerCombatComponent* PlayerCombat,
	const FAIREPlayerWeaponEquipRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}
	if (!bPersistenceReady || !InventorySessionId.IsValid())
	{
		return MakeResult(
			EAIREInventoryMutationCode::NotInitialized,
			Request.MutationId);
	}
	if (Request.SessionId != InventorySessionId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSession,
			Request.MutationId);
	}
	if (!Request.MutationId.IsValid())
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidMutationId,
			Request.MutationId);
	}
	if (!IsValid(PlayerInventory)
		|| !PlayerInventory->bPersistenceReadyForGameplay
		|| !IsValid(PlayerCombat))
	{
		return MakeResult(
			EAIREInventoryMutationCode::NotInitialized,
			Request.MutationId);
	}
	if (PlayerInventory->GetInventoryRevision()
		!= Request.ExpectedPlayerRevision)
	{
		return MakeResult(
			EAIREInventoryMutationCode::RevisionConflict,
			Request.MutationId,
			PlayerInventory->GetInventoryRevision());
	}

	const int32 StackIndex =
		PlayerInventory->FindStackIndexBySlot(Request.SourceSlotIndex);
	if (Request.SourceSlotIndex < 0
		|| Request.SourceSlotIndex >= PlayerInventory->MaxSlots
		|| StackIndex == INDEX_NONE)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSlot,
			Request.MutationId,
			PlayerInventory->GetInventoryRevision());
	}

	const FInventoryItemStack& SourceStack =
		PlayerInventory->Items[StackIndex];
	const FName NewWeaponItemId = SourceStack.ItemId;
	UGameInstance* GameInstance = GetGameInstance();
	const UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
		: nullptr;
	UAI_REWeaponItemDataAsset* WeaponItem = IsValid(ItemSubsystem)
		? Cast<UAI_REWeaponItemDataAsset>(
			ItemSubsystem->GetItemDataAsset(SourceStack.ItemId))
		: nullptr;
	if (SourceStack.Count != 1
		|| !IsValid(WeaponItem)
		|| WeaponItem->ItemId != SourceStack.ItemId
		|| WeaponItem->ItemType != EAI_REItemType::Weapon
		|| !IsValid(WeaponItem->WeaponDefinition.Get()))
	{
		PlayerInventory->NotifyWeaponEquipResult(SourceStack.ItemId, false);
		return MakeResult(
			EAIREInventoryMutationCode::InvalidItem,
			Request.MutationId,
			PlayerInventory->GetInventoryRevision());
	}
	if (PlayerInventory->EquippedWeaponItemId == NewWeaponItemId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidOperation,
			Request.MutationId,
			PlayerInventory->GetInventoryRevision());
	}

	TArray<FInventoryItemStack> NewPlayerItems = PlayerInventory->Items;
	FInventoryItemStack& NewSourceStack = NewPlayerItems[StackIndex];
	if (PlayerInventory->EquippedWeaponItemId.IsNone())
	{
		NewPlayerItems.RemoveAt(StackIndex);
	}
	else
	{
		NewSourceStack.ItemId = PlayerInventory->EquippedWeaponItemId;
		NewSourceStack.Count = 1;
	}

	if (!PlayerCombat->TryEquipWeapon(WeaponItem))
	{
		PlayerInventory->NotifyWeaponEquipResult(SourceStack.ItemId, false);
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentRequestRejected,
			Request.MutationId,
			PlayerInventory->GetInventoryRevision());
	}

	PlayerInventory->CommitExactInventoryAndEquipmentState(
		MoveTemp(NewPlayerItems),
		NewWeaponItemId);
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		PlayerInventory->GetInventoryRevision());
	RecordAppliedMutation(Result, true);
	PlayerInventory->NotifyExactInventoryMutation();
	PlayerInventory->NotifyWeaponEquipResult(NewWeaponItemId, true);
	return Result;
}

bool UAIREGameplayInventorySubsystem::CanCompletePlayerCraft(
	const UAI_REPlayerInventoryComponent* PlayerInventory,
	const FAIREPlayerCraftRequest& Request,
	FAIREInventoryMutationResult& OutResult) const
{
	OutResult = MakeResult(
		EAIREInventoryMutationCode::NotInitialized,
		Request.MutationId);
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		OutResult = PreviousResult;
		return true;
	}

	const FAIREContainerState* Storage = FindContainer(
		GetSharedStorageContainerId());
	OutResult = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		Storage,
		Request.ExpectedStorageRevision);
	if (OutResult.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return false;
	}
	if (!IsValid(PlayerInventory)
		|| !PlayerInventory->bPersistenceReadyForGameplay)
	{
		OutResult.Code = EAIREInventoryMutationCode::NotInitialized;
		return false;
	}
	if (Request.Result.ItemId.IsNone() || Request.Result.Count <= 0)
	{
		OutResult.Code = Request.Result.ItemId.IsNone()
			? EAIREInventoryMutationCode::InvalidItem
			: EAIREInventoryMutationCode::InvalidQuantity;
		return false;
	}

	TMap<FName, int32> IngredientTotals;
	if (!AggregateWorkIngredients(Request.Ingredients, IngredientTotals))
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidQuantity;
		return false;
	}
	for (const TPair<FName, int32>& Ingredient : IngredientTotals)
	{
		FAIREItemRules IngredientRules;
		if (!ResolveItemRules(Ingredient.Key, false, IngredientRules))
		{
			OutResult.Code = EAIREInventoryMutationCode::InvalidItem;
			return false;
		}
	}

	FAIREItemRules ResultRules;
	if (!ResolveItemRules(Request.Result.ItemId, false, ResultRules))
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidItem;
		return false;
	}

	TArray<FAIREInventoryItemStackSnapshot> NewPlayerStacks;
	CopyPlayerItemsToSnapshots(PlayerInventory->Items, NewPlayerStacks);
	TArray<FAIREInventoryItemStackSnapshot> NewStorageStacks =
		Storage->ItemStacks;
	bool bPlayerChanged = false;
	bool bStorageChanged = false;
	for (const TPair<FName, int32>& Ingredient : IngredientTotals)
	{
		if (!TryRemoveLocalFirst(
				NewPlayerStacks,
				NewStorageStacks,
				Ingredient.Key,
				Ingredient.Value,
				bPlayerChanged,
				bStorageChanged))
		{
			OutResult.Code = EAIREInventoryMutationCode::InsufficientQuantity;
			return false;
		}
	}
	if (!TryAddToStacks(
			NewPlayerStacks,
			PlayerInventory->MaxSlots,
			Request.Result.ItemId,
			Request.Result.Count,
			ResultRules.MaxStackSize))
	{
		OutResult.Code = EAIREInventoryMutationCode::CapacityExceeded;
		return false;
	}

	OutResult = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		Storage->Revision);
	return true;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::TryCompletePlayerCraft(
	UAI_REPlayerInventoryComponent* PlayerInventory,
	const FAIREPlayerCraftRequest& Request)
{
	FAIREInventoryMutationResult Result;
	if (!CanCompletePlayerCraft(PlayerInventory, Request, Result)
		|| Result.Code == EAIREInventoryMutationCode::AlreadyApplied)
	{
		return Result;
	}

	FAIREContainerState* Storage = FindContainer(
		GetSharedStorageContainerId());
	TMap<FName, int32> IngredientTotals;
	FAIREItemRules ResultRules;
	if (!IsValid(PlayerInventory)
		|| !Storage
		|| !AggregateWorkIngredients(Request.Ingredients, IngredientTotals)
		|| !ResolveItemRules(Request.Result.ItemId, false, ResultRules))
	{
		Result.Code = EAIREInventoryMutationCode::InvalidOperation;
		return Result;
	}

	TArray<FAIREInventoryItemStackSnapshot> NewPlayerStacks;
	CopyPlayerItemsToSnapshots(PlayerInventory->Items, NewPlayerStacks);
	TArray<FAIREInventoryItemStackSnapshot> NewStorageStacks =
		Storage->ItemStacks;
	bool bPlayerChanged = false;
	bool bStorageChanged = false;
	for (const TPair<FName, int32>& Ingredient : IngredientTotals)
	{
		if (!TryRemoveLocalFirst(
				NewPlayerStacks,
				NewStorageStacks,
				Ingredient.Key,
				Ingredient.Value,
				bPlayerChanged,
				bStorageChanged))
		{
			Result.Code = EAIREInventoryMutationCode::InvalidOperation;
			return Result;
		}
	}
	if (!TryAddToStacks(
			NewPlayerStacks,
			PlayerInventory->MaxSlots,
			Request.Result.ItemId,
			Request.Result.Count,
			ResultRules.MaxStackSize))
	{
		Result.Code = EAIREInventoryMutationCode::InvalidOperation;
		return Result;
	}
	TArray<FInventoryItemStack> NewPlayerItems;
	CopySnapshotsToPlayerItems(NewPlayerStacks, NewPlayerItems);

	PlayerInventory->CommitExactInventoryState(MoveTemp(NewPlayerItems));
	if (bStorageChanged)
	{
		Storage->ItemStacks = MoveTemp(NewStorageStacks);
		++Storage->Revision;
	}
	Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		Storage->Revision);
	RecordAppliedMutation(Result, true);
	PlayerInventory->NotifyExactInventoryMutation();
	if (bStorageChanged)
	{
		BroadcastContainerChanged(*Storage);
	}
	return Result;
}

FGuid UAIREGameplayInventorySubsystem::ResetInventorySession(
	const FAIREInventorySessionScope& NewScope)
{
	++PersistenceEpoch;
	++ActiveSaveEpoch;
	SessionScope = IsSessionScopeValid(NewScope)
		? NewScope
		: MakeCanonicalPersistenceScope();
	InventorySessionId = FGuid::NewGuid();
	AppliedMutations.Reset();
	AppliedMutationOrder.Reset();
	TransientAppliedMutations.Reset();
	TransientAppliedMutationOrder.Reset();
	AppliedWorkResults.Reset();
	AppliedWorkResultOrder.Reset();
	TransientAppliedWorkResults.Reset();
	TransientAppliedWorkResultOrder.Reset();
	AppliedImportCandidateIds.Reset();
	AppliedImportCandidateOrder.Reset();
	AppliedImportOperationIds.Reset();
	AppliedImportOperationOrder.Reset();
	AppliedOfflineTaskIds.Reset();
	AppliedOfflineTaskOrder.Reset();
	PersistenceLoadSlots.Reset();
	PendingCompanionConfig.Reset();
	CachedPlayerPersistenceState = FAIREInventoryPersistedPlayerState();
	bHasPlayerPersistenceState = false;
	if (RegisteredPlayerInventory.IsValid())
	{
		EAIREInventoryPersistenceResultCode CaptureCode =
			EAIREInventoryPersistenceResultCode::NotStarted;
		bHasPlayerPersistenceState = CapturePlayerPersistenceState(
			*RegisteredPlayerInventory.Get(),
			CachedPlayerPersistenceState,
			CaptureCode);
		RegisteredPlayerInventory->bPersistenceReadyForGameplay = true;
	}
	bPersistenceLoadComplete = true;
	bPersistenceReady = true;
	bPersistenceSaveInFlight = false;
	bPersistenceDirty = false;
	bPersistenceShuttingDown = false;
	bMakoInventoryInitialized = false;
	bShouldSeedFreshSharedStorage = false;
	CreateEmptyContainers();
	bSuppressPersistenceDirty = true;
	BroadcastContainerChanged(Containers.FindChecked(GetMakoContainerId()));
	BroadcastContainerChanged(
		Containers.FindChecked(GetSharedStorageContainerId()));
	bSuppressPersistenceDirty = false;
	if (bPersistenceLifecycleInitialized
		&& SessionScope == MakeCanonicalPersistenceScope())
	{
		MarkPersistenceDirty();
	}
	return InventorySessionId;
}

bool UAIREGameplayInventorySubsystem::CanCompleteMakoCraftWork(
	const FAIREMakoCraftWorkRequest& Request,
	FAIREInventoryWorkResult& OutResult) const
{
	OutResult = MakeWorkResult(
		EAIREInventoryMutationCode::NotInitialized,
		EAIREInventoryWorkResultDestination::None,
		Request.Result);
	FAIREInventoryWorkResult PreviousResult;
	if (FindAppliedWorkResult(Request.WorkOrderId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		PreviousResult.bAlreadyApplied = true;
		OutResult = PreviousResult;
		return true;
	}
	if (!bPersistenceReady || !InventorySessionId.IsValid())
	{
		return false;
	}
	if (Request.SessionId != InventorySessionId)
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidSession;
		return false;
	}
	if (!Request.WorkOrderId.IsValid())
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidMutationId;
		return false;
	}
	FAIREInventoryMutationResult ExistingMutation;
	if (FindAppliedMutation(Request.WorkOrderId, ExistingMutation))
	{
		OutResult.Code = EAIREInventoryMutationCode::DuplicateOperation;
		return false;
	}
	const FAIREContainerState* MakoContainer =
		FindContainer(GetMakoContainerId());
	const FAIREContainerState* StorageContainer =
		FindContainer(GetSharedStorageContainerId());
	if (!MakoContainer || !StorageContainer)
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidContainer;
		return false;
	}
	if (MakoContainer->Revision != Request.ExpectedMakoRevision
		|| StorageContainer->Revision != Request.ExpectedStorageRevision)
	{
		OutResult.Code = EAIREInventoryMutationCode::RevisionConflict;
		OutResult.MakoRevision = MakoContainer->Revision;
		OutResult.StorageRevision = StorageContainer->Revision;
		return false;
	}
	if (IsEquipmentTransitionActive(*MakoContainer))
	{
		OutResult.Code = EAIREInventoryMutationCode::EquipmentBusy;
		return false;
	}
	if (Request.Result.ItemId.IsNone() || Request.Result.Count <= 0)
	{
		OutResult.Code = Request.Result.ItemId.IsNone()
			? EAIREInventoryMutationCode::InvalidItem
			: EAIREInventoryMutationCode::InvalidQuantity;
		return false;
	}

	TMap<FName, int32> IngredientTotals;
	if (!AggregateWorkIngredients(Request.Ingredients, IngredientTotals))
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidQuantity;
		return false;
	}
	for (const TPair<FName, int32>& Ingredient : IngredientTotals)
	{
		FAIREItemRules IngredientRules;
		if (!ResolveItemRules(Ingredient.Key, false, IngredientRules))
		{
			OutResult.Code = EAIREInventoryMutationCode::InvalidItem;
			return false;
		}
		if (Ingredient.Key == MakoContainer->EquippedItemId
			|| Ingredient.Key == MakoContainer->PendingItemId)
		{
			OutResult.Code = EAIREInventoryMutationCode::EquipmentBusy;
			return false;
		}
	}

	FAIREItemRules ResultRules;
	if (!ResolveItemRules(Request.Result.ItemId, false, ResultRules))
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidItem;
		return false;
	}
	TArray<FAIREInventoryItemStackSnapshot> MakoAfterIngredients =
		MakoContainer->ItemStacks;
	TArray<FAIREInventoryItemStackSnapshot> StorageAfterIngredients =
		StorageContainer->ItemStacks;
	bool bMakoChanged = false;
	bool bStorageChanged = false;
	for (const TPair<FName, int32>& Ingredient : IngredientTotals)
	{
		if (!TryRemoveLocalFirst(
				MakoAfterIngredients,
				StorageAfterIngredients,
				Ingredient.Key,
				Ingredient.Value,
				bMakoChanged,
				bStorageChanged))
		{
			OutResult.Code = EAIREInventoryMutationCode::InsufficientQuantity;
			return false;
		}
	}
	TArray<FAIREInventoryItemStackSnapshot> MakoAfterResult =
		MakoAfterIngredients;
	if (TryAddToStacks(
			MakoAfterResult,
			MakoContainer->Capacity,
			Request.Result.ItemId,
			Request.Result.Count,
			ResultRules.MaxStackSize))
	{
		OutResult = MakeWorkResult(EAIREInventoryMutationCode::Succeeded,
			EAIREInventoryWorkResultDestination::Mako, Request.Result);
		return true;
	}
	TArray<FAIREInventoryItemStackSnapshot> StorageAfterResult =
		StorageAfterIngredients;
	if (TryAddToStacks(StorageAfterResult, StorageContainer->Capacity,
		Request.Result.ItemId, Request.Result.Count, ResultRules.MaxStackSize))
	{
		OutResult = MakeWorkResult(EAIREInventoryMutationCode::Succeeded,
			EAIREInventoryWorkResultDestination::SharedStorage, Request.Result);
		return true;
	}
	if (Request.bCanWorldDrop)
	{
		OutResult = MakeWorkResult(EAIREInventoryMutationCode::Succeeded,
			EAIREInventoryWorkResultDestination::WorldDrop, Request.Result);
		return true;
	}
	OutResult.Code = EAIREInventoryMutationCode::CapacityExceeded;
	return false;
}

FAIREInventoryWorkResult UAIREGameplayInventorySubsystem::TryCompleteMakoCraftWork(
	const FAIREMakoCraftWorkRequest& Request)
{
	FAIREInventoryWorkResult Result;
	if (!CanCompleteMakoCraftWork(Request, Result) || Result.bAlreadyApplied)
	{
		return Result;
	}
	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	FAIREContainerState* StorageContainer = FindContainer(GetSharedStorageContainerId());
	TMap<FName, int32> IngredientTotals;
	if (!MakoContainer || !StorageContainer
		|| !AggregateWorkIngredients(Request.Ingredients, IngredientTotals))
	{
		Result.Code = EAIREInventoryMutationCode::InvalidOperation;
		return Result;
	}
	TArray<FAIREInventoryItemStackSnapshot> NewMakoStacks = MakoContainer->ItemStacks;
	TArray<FAIREInventoryItemStackSnapshot> NewStorageStacks =
		StorageContainer->ItemStacks;
	bool bMakoChanged = false;
	bool bStorageChanged = false;
	for (const TPair<FName, int32>& Ingredient : IngredientTotals)
	{
		if (!TryRemoveLocalFirst(
				NewMakoStacks,
				NewStorageStacks,
				Ingredient.Key,
				Ingredient.Value,
				bMakoChanged,
				bStorageChanged))
		{
			Result.Code = EAIREInventoryMutationCode::InsufficientQuantity;
			return Result;
		}
	}
	FAIREItemRules ResultRules;
	const bool bResultInMako = Result.Destination == EAIREInventoryWorkResultDestination::Mako;
	if (Result.Destination != EAIREInventoryWorkResultDestination::WorldDrop
		&& (!ResolveItemRules(Request.Result.ItemId, false, ResultRules)
			|| !TryAddToStacks(bResultInMako ? NewMakoStacks : NewStorageStacks,
				bResultInMako ? MakoContainer->Capacity : StorageContainer->Capacity,
				Request.Result.ItemId, Request.Result.Count, ResultRules.MaxStackSize)))
	{
		Result.Code = EAIREInventoryMutationCode::InvalidOperation;
		return Result;
	}
	if (bResultInMako)
	{
		bMakoChanged = true;
	}
	else if (Result.Destination
		== EAIREInventoryWorkResultDestination::SharedStorage)
	{
		bStorageChanged = true;
	}
	if (bMakoChanged)
	{
		MakoContainer->ItemStacks = MoveTemp(NewMakoStacks);
		++MakoContainer->Revision;
	}
	if (bStorageChanged)
	{
		StorageContainer->ItemStacks = MoveTemp(NewStorageStacks);
		++StorageContainer->Revision;
	}
	Result.MakoRevision = MakoContainer->Revision;
	Result.StorageRevision = StorageContainer->Revision;
	RecordAppliedWorkResult(
		Request.WorkOrderId,
		Result,
		Result.Destination != EAIREInventoryWorkResultDestination::WorldDrop);
	if (bMakoChanged)
	{
		BroadcastContainerChanged(*MakoContainer);
	}
	if (bStorageChanged)
	{
		BroadcastContainerChanged(*StorageContainer);
	}
	return Result;
}

FAIREInventoryWorkResult UAIREGameplayInventorySubsystem::TryStoreMakoWorkReward(
	const FAIREMakoWorkRewardRequest& Request)
{
	FAIREInventoryWorkResult PreviousResult;
	if (FindAppliedWorkResult(Request.DeliveryId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		PreviousResult.bAlreadyApplied = true;
		return PreviousResult;
	}
	FAIREInventoryWorkResult Result = MakeWorkResult(EAIREInventoryMutationCode::NotInitialized,
		EAIREInventoryWorkResultDestination::None, Request.Reward);
	if (!bPersistenceReady || !InventorySessionId.IsValid()) { return Result; }
	if (Request.SessionId != InventorySessionId) { Result.Code = EAIREInventoryMutationCode::InvalidSession; return Result; }
	if (!Request.DeliveryId.IsValid()) { Result.Code = EAIREInventoryMutationCode::InvalidMutationId; return Result; }
	FAIREInventoryMutationResult ExistingMutation;
	if (FindAppliedMutation(Request.DeliveryId, ExistingMutation))
	{
		Result.Code = EAIREInventoryMutationCode::DuplicateOperation;
		return Result;
	}
	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	FAIREContainerState* StorageContainer = FindContainer(GetSharedStorageContainerId());
	if (!MakoContainer || !StorageContainer) { Result.Code = EAIREInventoryMutationCode::InvalidContainer; return Result; }
	if (MakoContainer->Revision != Request.ExpectedMakoRevision || StorageContainer->Revision != Request.ExpectedStorageRevision) { Result.Code = EAIREInventoryMutationCode::RevisionConflict; Result.MakoRevision = MakoContainer->Revision; Result.StorageRevision = StorageContainer->Revision; return Result; }
	if (Request.Reward.ItemId.IsNone() || Request.Reward.Count <= 0) { Result.Code = Request.Reward.ItemId.IsNone() ? EAIREInventoryMutationCode::InvalidItem : EAIREInventoryMutationCode::InvalidQuantity; return Result; }
	TArray<FAIREInventoryItemStackSnapshot> NewMakoStacks = MakoContainer->ItemStacks;
	FAIREItemRules MakoRules;
	if (!IsEquipmentTransitionActive(*MakoContainer)
		&& ResolveItemRules(Request.Reward.ItemId, false, MakoRules)
		&& TryAddToStacks(NewMakoStacks, MakoContainer->Capacity, Request.Reward.ItemId, Request.Reward.Count, MakoRules.MaxStackSize))
	{
		MakoContainer->ItemStacks = MoveTemp(NewMakoStacks);
		++MakoContainer->Revision;
		Result = MakeWorkResult(EAIREInventoryMutationCode::Succeeded, EAIREInventoryWorkResultDestination::Mako, Request.Reward);
		Result.MakoRevision = MakoContainer->Revision;
		Result.StorageRevision = StorageContainer->Revision;
		RecordAppliedWorkResult(Request.DeliveryId, Result);
		BroadcastContainerChanged(*MakoContainer);
		return Result;
	}
	TArray<FAIREInventoryItemStackSnapshot> NewStorageStacks = StorageContainer->ItemStacks;
	FAIREItemRules StorageRules;
	if (ResolveItemRules(Request.Reward.ItemId, false, StorageRules)
		&& TryAddToStacks(NewStorageStacks, StorageContainer->Capacity, Request.Reward.ItemId, Request.Reward.Count, StorageRules.MaxStackSize))
	{
		StorageContainer->ItemStacks = MoveTemp(NewStorageStacks);
		++StorageContainer->Revision;
		Result = MakeWorkResult(EAIREInventoryMutationCode::Succeeded, EAIREInventoryWorkResultDestination::SharedStorage, Request.Reward);
		Result.MakoRevision = MakoContainer->Revision;
		Result.StorageRevision = StorageContainer->Revision;
		RecordAppliedWorkResult(Request.DeliveryId, Result);
		BroadcastContainerChanged(*StorageContainer);
		return Result;
	}
	Result = MakeWorkResult(
		EAIREInventoryMutationCode::Succeeded,
		EAIREInventoryWorkResultDestination::WorldDrop,
		Request.Reward);
	Result.MakoRevision = MakoContainer->Revision;
	Result.StorageRevision = StorageContainer->Revision;
	return Result;
}

bool UAIREGameplayInventorySubsystem::EnsureMakoInventoryInitialized(
	const UAIRECompanionConfigDataAsset* CompanionConfig)
{
	if (bMakoInventoryInitialized)
	{
		return true;
	}
	if (!IsValid(CompanionConfig))
	{
		return false;
	}

	FText ValidationError;
	if (!CompanionConfig->IsConfigurationValid(ValidationError))
	{
		return false;
	}
	PendingCompanionConfig = CompanionConfig;
	if (!bPersistenceLoadComplete)
	{
		return true;
	}

	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	FAIREContainerState* StorageContainer = FindContainer(
		GetSharedStorageContainerId());
	if (!MakoContainer || !StorageContainer)
	{
		return false;
	}

	TArray<FAIREInventoryItemStackSnapshot> NewStacks;
	for (const FAIRECompanionInitialInventoryEntry& Entry
		: CompanionConfig->InitialInventory)
	{
		if (!IsValid(Entry.ItemDefinition) || Entry.Count <= 0)
		{
			return false;
		}

		FAIREItemRules Rules;
		if (!ResolveItemRules(
				Entry.ItemDefinition->ItemId,
				true,
				Rules)
			|| !TryAddToStacks(
				NewStacks,
				MakoContainer->Capacity,
				Entry.ItemDefinition->ItemId,
				Entry.Count,
				Rules.MaxStackSize))
		{
			return false;
		}
	}

	FName EquippedItemId;
	if (!CompanionConfig->DefaultEquippedWeaponItemId.IsNone())
	{
		FAIREItemRules WeaponRules;
		if (!ResolveItemRules(
				CompanionConfig->DefaultEquippedWeaponItemId,
				true,
				WeaponRules)
			|| !WeaponRules.bIsWeapon
			|| !TryRemoveItemFromStacks(
				NewStacks,
				CompanionConfig->DefaultEquippedWeaponItemId,
				1))
		{
			return false;
		}
		EquippedItemId = CompanionConfig->DefaultEquippedWeaponItemId;
	}

	TArray<FAIREInventoryItemStackSnapshot> NewStorageStacks;
	if (bShouldSeedFreshSharedStorage)
	{
		const TPair<FName, int32> FreshStorageItems[] =
		{
			{ FreshIronIngotItemId, FreshIronIngotCount },
			{ FreshWoodHandleItemId, FreshWoodHandleCount }
		};
		for (const TPair<FName, int32>& Item : FreshStorageItems)
		{
			FAIREItemRules Rules;
			if (!ResolveItemRules(Item.Key, false, Rules)
				|| !TryAddToStacks(
					NewStorageStacks,
					StorageContainer->Capacity,
					Item.Key,
					Item.Value,
					Rules.MaxStackSize))
			{
				return false;
			}
		}
	}

	MakoContainer->ItemStacks = MoveTemp(NewStacks);
	MakoContainer->EquippedItemId = EquippedItemId;
	if (bShouldSeedFreshSharedStorage)
	{
		StorageContainer->ItemStacks = MoveTemp(NewStorageStacks);
	}
	bMakoInventoryInitialized = true;
	if (!MakoContainer->ItemStacks.IsEmpty()
		|| !MakoContainer->EquippedItemId.IsNone())
	{
		++MakoContainer->Revision;
		BroadcastContainerChanged(*MakoContainer);
	}
	if (bShouldSeedFreshSharedStorage)
	{
		++StorageContainer->Revision;
		BroadcastContainerChanged(*StorageContainer);
	}
	bShouldSeedFreshSharedStorage = false;
	PendingCompanionConfig.Reset();
	if (!bPersistenceReady)
	{
		CompletePersistenceStartup(MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Load,
			EAIREInventoryPersistenceResultCode::FreshStateSeeded));
	}
	if (!bPersistenceSaveInFlight)
	{
		MarkPersistenceDirty();
	}
	return true;
}

FAIREOfflineTaskApplyResult
UAIREGameplayInventorySubsystem::TryApplyOfflineTaskResult(
	const FAIREOfflineTaskApplyRequest& Request)
{
	FAIREOfflineTaskApplyResult Result;
	const FAIREContainerState* CurrentMako = FindContainer(GetMakoContainerId());
	const FAIREContainerState* CurrentStorage =
		FindContainer(GetSharedStorageContainerId());
	Result.MakoRevision = CurrentMako ? CurrentMako->Revision : INDEX_NONE;
	Result.StorageRevision = CurrentStorage
		? CurrentStorage->Revision
		: INDEX_NONE;
	if (!bPersistenceReady || !InventorySessionId.IsValid())
	{
		return Result;
	}
	if (Request.SessionId != InventorySessionId)
	{
		Result.Code = EAIREInventoryMutationCode::InvalidSession;
		return Result;
	}
	if (!IsStableId(Request.TaskId))
	{
		Result.Code = EAIREInventoryMutationCode::InvalidOperation;
		return Result;
	}
	if (AppliedOfflineTaskIds.Contains(Request.TaskId))
	{
		Result.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return Result;
	}
	FAIREContainerState* Mako = FindContainer(GetMakoContainerId());
	FAIREContainerState* Storage = FindContainer(GetSharedStorageContainerId());
	if (!Mako || !Storage)
	{
		Result.Code = EAIREInventoryMutationCode::InvalidContainer;
		return Result;
	}
	if (Mako->Revision != Request.ExpectedMakoRevision
		|| Storage->Revision != Request.ExpectedStorageRevision)
	{
		Result.Code = EAIREInventoryMutationCode::RevisionConflict;
		return Result;
	}
	if (IsEquipmentTransitionActive(*Mako))
	{
		Result.Code = EAIREInventoryMutationCode::EquipmentBusy;
		return Result;
	}

	TMap<FName, int32> CostTotals;
	TMap<FName, int32> RewardTotals;
	auto Aggregate = [](const TArray<FAIREInventoryItemQuantity>& Items,
		TMap<FName, int32>& OutTotals,
		const bool bAllowEmpty)
	{
		OutTotals.Reset();
		for (const FAIREInventoryItemQuantity& Item : Items)
		{
			const int64 Total = static_cast<int64>(OutTotals.FindRef(Item.ItemId))
				+ Item.Count;
			if (Item.ItemId.IsNone() || Item.Count <= 0 || Total > MAX_int32)
			{
				return false;
			}
			OutTotals.Add(Item.ItemId, static_cast<int32>(Total));
		}
		return bAllowEmpty || !OutTotals.IsEmpty();
	};
	if (!Aggregate(Request.Costs, CostTotals, true)
		|| !Aggregate(Request.Rewards, RewardTotals, false))
	{
		Result.Code = EAIREInventoryMutationCode::InvalidQuantity;
		return Result;
	}
	TMap<FName, FAIREItemRules> RulesByItem;
	for (const TMap<FName, int32>* Totals : { &CostTotals, &RewardTotals })
	{
		for (const TPair<FName, int32>& Item : *Totals)
		{
			FAIREItemRules Rules;
			if (!ResolveItemRules(Item.Key, false, Rules))
			{
				Result.Code = EAIREInventoryMutationCode::InvalidItem;
				return Result;
			}
			RulesByItem.Add(Item.Key, Rules);
		}
	}

	TArray<FAIREInventoryItemStackSnapshot> NewMako = Mako->ItemStacks;
	TArray<FAIREInventoryItemStackSnapshot> NewStorage = Storage->ItemStacks;
	bool bMakoChanged = false;
	bool bStorageChanged = false;
	for (const TPair<FName, int32>& Cost : CostTotals)
	{
		if (!TryRemoveLocalFirst(
				NewMako,
				NewStorage,
				Cost.Key,
				Cost.Value,
				bMakoChanged,
				bStorageChanged))
		{
			Result.Code = EAIREInventoryMutationCode::InsufficientQuantity;
			return Result;
		}
	}
	for (const TPair<FName, int32>& Reward : RewardTotals)
	{
		const FAIREItemRules& Rules = RulesByItem.FindChecked(Reward.Key);
		TArray<FAIREInventoryItemStackSnapshot> MakoCandidate = NewMako;
		if (TryAddToStacks(
				MakoCandidate,
				Mako->Capacity,
				Reward.Key,
				Reward.Value,
				Rules.MaxStackSize))
		{
			NewMako = MoveTemp(MakoCandidate);
			bMakoChanged = true;
			continue;
		}
		if (!TryAddToStacks(
				NewStorage,
				Storage->Capacity,
				Reward.Key,
				Reward.Value,
				Rules.MaxStackSize))
		{
			Result.Code = EAIREInventoryMutationCode::CapacityExceeded;
			return Result;
		}
		bStorageChanged = true;
	}

	if (bMakoChanged)
	{
		Mako->ItemStacks = MoveTemp(NewMako);
		++Mako->Revision;
	}
	if (bStorageChanged)
	{
		Storage->ItemStacks = MoveTemp(NewStorage);
		++Storage->Revision;
	}
	RecordAppliedOfflineTaskId(Request.TaskId);
	Result.Code = EAIREInventoryMutationCode::Succeeded;
	Result.MakoRevision = Mako->Revision;
	Result.StorageRevision = Storage->Revision;
	if (bMakoChanged)
	{
		BroadcastContainerChanged(*Mako);
	}
	if (bStorageChanged)
	{
		BroadcastContainerChanged(*Storage);
	}
	return Result;
}

const UAIRECompanionItemDefinitionDataAsset*
UAIREGameplayInventorySubsystem::FindCompanionItemDefinition(
	const FName ItemId) const
{
	UGameInstance* GameInstance = GetGameInstance();
	const UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
		: nullptr;
	return IsValid(ItemSubsystem)
		? Cast<UAIRECompanionItemDefinitionDataAsset>(
			ItemSubsystem->GetItemDataAsset(ItemId))
		: nullptr;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::TryApplyStartupImportCandidate(
	const FAIREInventoryStartupImportCandidate& Candidate)
{
	if (!bPersistenceReady)
	{
		return MakeResult(
			EAIREInventoryMutationCode::NotInitialized,
			FGuid());
	}
	if (Candidate.LocalFormatVersion
		!= AIREGameplayInventory::LocalImportFormatVersion)
	{
		return MakeResult(
			EAIREInventoryMutationCode::UnsupportedImportFormat,
			FGuid());
	}
	if (!InventorySessionId.IsValid()
		|| Candidate.SessionId != InventorySessionId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSession,
			FGuid());
	}
	if (!IsSessionScopeValid(SessionScope)
		|| !(Candidate.Scope == SessionScope))
	{
		return MakeResult(
			EAIREInventoryMutationCode::ScopeMismatch,
			FGuid());
	}
	if (!IsStableId(Candidate.CandidateId))
	{
		return MakeResult(
			EAIREInventoryMutationCode::DuplicateOperation,
			FGuid());
	}
	if (AppliedImportCandidateIds.Contains(Candidate.CandidateId))
	{
		return MakeResult(
			EAIREInventoryMutationCode::AlreadyApplied,
			FGuid());
	}

	FAIREContainerState* Container = FindContainer(Candidate.ContainerId);
	if (!Container)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidContainer,
			FGuid());
	}
	if (Container->Revision != Candidate.BaseRevision)
	{
		return MakeResult(
			EAIREInventoryMutationCode::RevisionConflict,
			FGuid(),
			Container->Revision);
	}
	if (IsEquipmentTransitionActive(*Container))
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			FGuid(),
			Container->Revision);
	}
	if (Candidate.Operations.IsEmpty())
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidQuantity,
			FGuid(),
			Container->Revision);
	}

	TSet<FString> CandidateOperationIds;
	TArray<FAIREInventoryItemStackSnapshot> NewStacks = Container->ItemStacks;
	for (const FAIREInventoryImportOperation& Operation : Candidate.Operations)
	{
		if (Operation.Type != EAIREInventoryImportOperationType::Add
			&& Operation.Type != EAIREInventoryImportOperationType::Remove)
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidOperation,
				FGuid(),
				Container->Revision);
		}
		if (!IsStableId(Operation.OperationId)
			|| CandidateOperationIds.Contains(Operation.OperationId)
			|| AppliedImportOperationIds.Contains(Operation.OperationId))
		{
			return MakeResult(
				EAIREInventoryMutationCode::DuplicateOperation,
				FGuid(),
				Container->Revision);
		}
		CandidateOperationIds.Add(Operation.OperationId);

		FAIREItemRules Rules;
		if (Operation.Count <= 0
			|| !ResolveItemRules(
				Operation.ItemId,
				false,
				Rules))
		{
			return MakeResult(
				Operation.Count <= 0
					? EAIREInventoryMutationCode::InvalidQuantity
					: EAIREInventoryMutationCode::InvalidItem,
				FGuid(),
				Container->Revision);
		}

		const bool bApplied = Operation.Type
			== EAIREInventoryImportOperationType::Add
			? TryAddToStacks(
				NewStacks,
				Container->Capacity,
				Operation.ItemId,
				Operation.Count,
				Rules.MaxStackSize)
			: TryRemoveItemFromStacks(
				NewStacks,
				Operation.ItemId,
				Operation.Count);
		if (!bApplied)
		{
			return MakeResult(
				Operation.Type == EAIREInventoryImportOperationType::Add
					? EAIREInventoryMutationCode::CapacityExceeded
					: EAIREInventoryMutationCode::InsufficientQuantity,
				FGuid(),
				Container->Revision);
		}
	}

	Container->ItemStacks = MoveTemp(NewStacks);
	++Container->Revision;
	RecordAppliedImportCandidateId(Candidate.CandidateId);
	TArray<FString> OrderedOperationIds = CandidateOperationIds.Array();
	OrderedOperationIds.Sort();
	RecordAppliedImportOperationIds(OrderedOperationIds);
	BroadcastContainerChanged(*Container);
	return MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		FGuid(),
		Container->Revision);
}

void UAIREGameplayInventorySubsystem::CreateEmptyContainers()
{
	Containers.Reset();

	FAIREContainerState MakoContainer;
	MakoContainer.ContainerId = GetMakoContainerId();
	MakoContainer.Capacity = AIREGameplayInventory::MakoItemSlotCapacity;
	Containers.Add(MakoContainer.ContainerId, MoveTemp(MakoContainer));

	FAIREContainerState Storage;
	Storage.ContainerId = GetSharedStorageContainerId();
	Storage.Capacity =
		AIREGameplayInventory::SharedStorageSlotCapacity;
	Containers.Add(Storage.ContainerId, MoveTemp(Storage));
}

UAIREGameplayInventorySubsystem::FAIREContainerState*
UAIREGameplayInventorySubsystem::FindContainer(const FName ContainerId)
{
	return Containers.Find(NormalizeContainerId(ContainerId));
}

const UAIREGameplayInventorySubsystem::FAIREContainerState*
UAIREGameplayInventorySubsystem::FindContainer(const FName ContainerId) const
{
	return Containers.Find(NormalizeContainerId(ContainerId));
}

bool UAIREGameplayInventorySubsystem::ResolveItemRules(
	const FName ItemId,
	const bool bRequireCompanionItem,
	FAIREItemRules& OutRules) const
{
	OutRules = FAIREItemRules();
#if WITH_DEV_AUTOMATION_TESTS
	if (GIsAutomationTesting
		&& ResolveSyntheticItemRules(
			ItemId,
			OutRules.MaxStackSize,
			OutRules.bIsCompanionItem,
			OutRules.bIsWeapon))
	{
		return !bRequireCompanionItem || OutRules.bIsCompanionItem;
	}
#endif

	UGameInstance* GameInstance = GetGameInstance();
	const UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
		: nullptr;
	const UAI_REItemDataAsset* ItemDefinition = IsValid(ItemSubsystem)
		? ItemSubsystem->GetItemDataAsset(ItemId)
		: nullptr;
	if (!IsValid(ItemDefinition)
		|| ItemDefinition->ItemId != ItemId
		|| ItemDefinition->MaxStackSize < 1)
	{
		return false;
	}

	const UAIRECompanionItemDefinitionDataAsset* CompanionItem =
		Cast<UAIRECompanionItemDefinitionDataAsset>(ItemDefinition);
	if (bRequireCompanionItem)
	{
		FText ValidationError;
		if (!IsValid(CompanionItem)
			|| !CompanionItem->IsCompanionItemDefinitionValid(
				ValidationError))
		{
			return false;
		}
	}

	OutRules.MaxStackSize = ItemDefinition->MaxStackSize;
	OutRules.bIsCompanionItem = IsValid(CompanionItem);
	OutRules.bIsWeapon = ItemDefinition->ItemType == EAI_REItemType::Weapon;
	return true;
}

bool UAIREGameplayInventorySubsystem::IsEquipmentTransitionActive(
	const FAIREContainerState& State) const
{
	return State.ContainerId == GetMakoContainerId()
		&& (State.EquipmentTransition
				== EAIREEquipmentTransitionState::Equipping
			|| State.EquipmentTransition
				== EAIREEquipmentTransitionState::Recovering);
}

bool UAIREGameplayInventorySubsystem::IsSessionScopeValid(
	const FAIREInventorySessionScope& Scope) const
{
	return IsStableId(Scope.ProfileId)
		&& IsStableId(Scope.SaveSlotId)
		&& IsStableId(Scope.CompanionId);
}

FAIREInventorySessionScope
UAIREGameplayInventorySubsystem::MakeCanonicalPersistenceScope()
{
	FAIREInventorySessionScope Scope;
	Scope.ProfileId = AIREGameplayInventoryPersistence::CanonicalProfileId;
	Scope.SaveSlotId = AIREGameplayInventoryPersistence::CanonicalSaveSlotId;
	Scope.CompanionId = AIREGameplayInventoryPersistence::CanonicalCompanionId;
	return Scope;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::ValidateMutation(
	const FGuid& RequestSessionId,
	const FGuid& MutationId,
	const FAIREContainerState* Container,
	const int64 ExpectedRevision) const
{
	if (!bPersistenceReady || !InventorySessionId.IsValid())
	{
		return MakeResult(
			EAIREInventoryMutationCode::NotInitialized,
			MutationId);
	}
	if (RequestSessionId != InventorySessionId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSession,
			MutationId);
	}
	if (!MutationId.IsValid())
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidMutationId,
			MutationId);
	}
	if (!Container)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidContainer,
			MutationId);
	}
	if (Container->Revision != ExpectedRevision)
	{
		return MakeResult(
			EAIREInventoryMutationCode::RevisionConflict,
			MutationId,
			Container->Revision);
	}
	return MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		MutationId,
		Container->Revision);
}

FAIREInventoryMutationResult UAIREGameplayInventorySubsystem::MakeResult(
	const EAIREInventoryMutationCode Code,
	const FGuid& MutationId,
	const int64 SourceRevision,
	const int64 DestinationRevision) const
{
	FAIREInventoryMutationResult Result;
	Result.Code = Code;
	Result.MutationId = MutationId;
	Result.SourceRevision = SourceRevision;
	Result.DestinationRevision = DestinationRevision;
	return Result;
}

bool UAIREGameplayInventorySubsystem::FindAppliedMutation(
	const FGuid& MutationId,
	FAIREInventoryMutationResult& OutResult) const
{
	if (!MutationId.IsValid())
	{
		return false;
	}
	if (const FAIREInventoryMutationResult* Found =
		AppliedMutations.Find(MutationId))
	{
		OutResult = *Found;
		return true;
	}
	if (const FAIREInventoryMutationResult* Found =
		TransientAppliedMutations.Find(MutationId))
	{
		OutResult = *Found;
		return true;
	}
	if (const FAIREInventoryWorkResult* WorkResult =
		AppliedWorkResults.Find(MutationId))
	{
		OutResult = MakeResult(
			EAIREInventoryMutationCode::Succeeded,
			MutationId,
			WorkResult->MakoRevision,
			WorkResult->StorageRevision);
		return true;
	}
	return false;
}

void UAIREGameplayInventorySubsystem::RecordAppliedMutation(
	const FAIREInventoryMutationResult& Result,
	const bool bPersistent)
{
	if (!Result.MutationId.IsValid())
	{
		return;
	}

	if (bPersistent)
	{
		TransientAppliedMutations.Remove(Result.MutationId);
		TransientAppliedMutationOrder.RemoveSingle(Result.MutationId);
		if (!AppliedMutations.Contains(Result.MutationId))
		{
			AppliedMutationOrder.Add(Result.MutationId);
		}
		AppliedMutations.Add(Result.MutationId, Result);
		while (AppliedMutationOrder.Num()
			> AIREGameplayInventoryPersistence::MaxLedgerEntries)
		{
			const FGuid EvictedId = AppliedMutationOrder[0];
			AppliedMutationOrder.RemoveAt(0);
			AppliedMutations.Remove(EvictedId);
		}
		return;
	}

	if (AppliedMutations.Contains(Result.MutationId))
	{
		return;
	}
	if (!TransientAppliedMutations.Contains(Result.MutationId))
	{
		TransientAppliedMutationOrder.Add(Result.MutationId);
	}
	TransientAppliedMutations.Add(Result.MutationId, Result);
	while (TransientAppliedMutationOrder.Num()
		> AIREGameplayInventoryPersistence::MaxLedgerEntries)
	{
		const FGuid EvictedId = TransientAppliedMutationOrder[0];
		TransientAppliedMutationOrder.RemoveAt(0);
		TransientAppliedMutations.Remove(EvictedId);
	}
}

void UAIREGameplayInventorySubsystem::RemoveAppliedMutation(
	const FGuid& MutationId)
{
	TransientAppliedMutations.Remove(MutationId);
	TransientAppliedMutationOrder.RemoveSingle(MutationId);
}

void UAIREGameplayInventorySubsystem::RecordAppliedWorkResult(
	const FGuid& OperationId,
	const FAIREInventoryWorkResult& Result,
	const bool bPersistent)
{
	if (!OperationId.IsValid())
	{
		return;
	}
	if (!bPersistent)
	{
		if (!TransientAppliedWorkResults.Contains(OperationId))
		{
			TransientAppliedWorkResultOrder.Add(OperationId);
		}
		TransientAppliedWorkResults.Add(OperationId, Result);
		while (TransientAppliedWorkResultOrder.Num()
			> AIREGameplayInventoryPersistence::MaxLedgerEntries)
		{
			const FGuid EvictedId = TransientAppliedWorkResultOrder[0];
			TransientAppliedWorkResultOrder.RemoveAt(0);
			TransientAppliedWorkResults.Remove(EvictedId);
		}
		return;
	}
	TransientAppliedWorkResults.Remove(OperationId);
	TransientAppliedWorkResultOrder.RemoveSingle(OperationId);
	if (!AppliedWorkResults.Contains(OperationId))
	{
		AppliedWorkResultOrder.Add(OperationId);
	}
	AppliedWorkResults.Add(OperationId, Result);
	while (AppliedWorkResultOrder.Num()
		> AIREGameplayInventoryPersistence::MaxLedgerEntries)
	{
		const FGuid EvictedId = AppliedWorkResultOrder[0];
		AppliedWorkResultOrder.RemoveAt(0);
		AppliedWorkResults.Remove(EvictedId);
	}
}

void UAIREGameplayInventorySubsystem::RecordAppliedImportCandidateId(
	const FString& CandidateId)
{
	if (AppliedImportCandidateIds.Contains(CandidateId))
	{
		return;
	}
	AppliedImportCandidateIds.Add(CandidateId);
	AppliedImportCandidateOrder.Add(CandidateId);
	while (AppliedImportCandidateOrder.Num()
		> AIREGameplayInventoryPersistence::MaxLedgerEntries)
	{
		const FString EvictedId = AppliedImportCandidateOrder[0];
		AppliedImportCandidateOrder.RemoveAt(0);
		AppliedImportCandidateIds.Remove(EvictedId);
	}
}

void UAIREGameplayInventorySubsystem::RecordAppliedImportOperationIds(
	const TArray<FString>& OperationIds)
{
	for (const FString& OperationId : OperationIds)
	{
		if (AppliedImportOperationIds.Contains(OperationId))
		{
			continue;
		}
		AppliedImportOperationIds.Add(OperationId);
		AppliedImportOperationOrder.Add(OperationId);
		while (AppliedImportOperationOrder.Num()
			> AIREGameplayInventoryPersistence::MaxLedgerEntries)
		{
			const FString EvictedId = AppliedImportOperationOrder[0];
			AppliedImportOperationOrder.RemoveAt(0);
			AppliedImportOperationIds.Remove(EvictedId);
		}
	}
}

void UAIREGameplayInventorySubsystem::RecordAppliedOfflineTaskId(
	const FString& TaskId)
{
	if (!AppliedOfflineTaskIds.Contains(TaskId))
	{
		AppliedOfflineTaskOrder.Add(TaskId);
	}
	AppliedOfflineTaskIds.Add(TaskId);
	while (AppliedOfflineTaskOrder.Num()
		> AIREGameplayInventoryPersistence::MaxLedgerEntries)
	{
		const FString EvictedId = AppliedOfflineTaskOrder[0];
		AppliedOfflineTaskOrder.RemoveAt(0);
		AppliedOfflineTaskIds.Remove(EvictedId);
	}
}

void UAIREGameplayInventorySubsystem::BroadcastContainerChanged(
	const FAIREContainerState& Container)
{
#if WITH_DEV_AUTOMATION_TESTS
	++GInventoryAutomationBroadcastCount;
#endif
	OnContainerChanged.Broadcast(Container.ContainerId, Container.Revision);
	if (!bSuppressPersistenceDirty
		&& bPersistenceReady
		&& SessionScope == MakeCanonicalPersistenceScope())
	{
		MarkPersistenceDirty();
	}
}

bool UAIREGameplayInventorySubsystem::AggregateWorkIngredients(
	const TArray<FAIREInventoryItemQuantity>& Ingredients,
	TMap<FName, int32>& OutIngredients) const
{
	OutIngredients.Reset();
	for (const FAIREInventoryItemQuantity& Ingredient : Ingredients)
	{
		if (Ingredient.ItemId.IsNone() || Ingredient.Count <= 0)
		{
			return false;
		}
		const int32 ExistingCount = OutIngredients.FindRef(Ingredient.ItemId);
		const int64 TotalCount = static_cast<int64>(ExistingCount)
			+ static_cast<int64>(Ingredient.Count);
		if (TotalCount > MAX_int32)
		{
			return false;
		}
		OutIngredients.Add(Ingredient.ItemId, static_cast<int32>(TotalCount));
	}
	return !OutIngredients.IsEmpty();
}

FAIREInventoryWorkResult UAIREGameplayInventorySubsystem::MakeWorkResult(
	const EAIREInventoryMutationCode Code,
	const EAIREInventoryWorkResultDestination Destination,
	const FAIREInventoryItemQuantity& DeliveredItem) const
{
	FAIREInventoryWorkResult Result;
	Result.Code = Code;
	Result.Destination = Destination;
	Result.DeliveredItem = DeliveredItem;
	const FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	const FAIREContainerState* StorageContainer =
		FindContainer(GetSharedStorageContainerId());
	Result.MakoRevision = MakoContainer ? MakoContainer->Revision : INDEX_NONE;
	Result.StorageRevision = StorageContainer
		? StorageContainer->Revision
		: INDEX_NONE;
	return Result;
}

bool UAIREGameplayInventorySubsystem::FindAppliedWorkResult(
	const FGuid& MutationId,
	FAIREInventoryWorkResult& OutResult) const
{
	if (!MutationId.IsValid())
	{
		return false;
	}
	const FAIREInventoryWorkResult* Found = AppliedWorkResults.Find(MutationId);
	if (!Found)
	{
		Found = TransientAppliedWorkResults.Find(MutationId);
	}
	if (!Found)
	{
		return false;
	}
	OutResult = *Found;
	return true;
}

void UAIREGameplayInventorySubsystem::BeginPersistenceLoad()
{
	const uint64 LoadEpoch = ++PersistenceEpoch;
	PersistenceLoadSlots.SetNum(2);
	PersistenceLoadSlots[0].SlotName =
		AIREGameplayInventoryPersistence::PrimarySlotName;
	PersistenceLoadSlots[1].SlotName =
		AIREGameplayInventoryPersistence::PreviousSlotName;
	BeginLoadForSlot(0, LoadEpoch);
	BeginLoadForSlot(1, LoadEpoch);
}

void UAIREGameplayInventorySubsystem::BeginLoadForSlot(
	const int32 SlotIndex,
	const uint64 LoadEpoch)
{
	if (!PersistenceLoadSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	ISaveGameSystem* SaveSystem =
		IPlatformFeaturesModule::Get().GetSaveGameSystem();
	if (!SaveSystem)
	{
		HandleSlotExistenceResult(SlotIndex, LoadEpoch, false, true);
		return;
	}

	const FString SlotName = PersistenceLoadSlots[SlotIndex].SlotName;
	const FPlatformUserId PlatformUserId =
		FPlatformMisc::GetPlatformUserForUserIndex(
			AIREGameplayInventoryPersistence::UserIndex);
	const TWeakObjectPtr<UAIREGameplayInventorySubsystem> WeakThis(this);
	SaveSystem->DoesSaveGameExistAsync(
		*SlotName,
		PlatformUserId,
		[WeakThis, SlotIndex, LoadEpoch](
			const FString&,
			FPlatformUserId,
			const ISaveGameSystem::ESaveExistsResult ExistsResult)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			const bool bExists =
				ExistsResult == ISaveGameSystem::ESaveExistsResult::OK
				|| ExistsResult
					== ISaveGameSystem::ESaveExistsResult::Corrupt;
			const bool bIoFailure = ExistsResult
				== ISaveGameSystem::ESaveExistsResult::UnspecifiedError;
			WeakThis->HandleSlotExistenceResult(
				SlotIndex,
				LoadEpoch,
				bExists,
				bIoFailure);
		});
}

void UAIREGameplayInventorySubsystem::HandleSlotExistenceResult(
	const int32 SlotIndex,
	const uint64 LoadEpoch,
	const bool bExists,
	const bool bIoFailure)
{
	if (LoadEpoch != PersistenceEpoch
		|| bPersistenceShuttingDown
		|| !PersistenceLoadSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FAIREPersistenceLoadSlotState& Slot =
		PersistenceLoadSlots[SlotIndex];
	Slot.bExists = bExists;
	Slot.bIoFailure = bIoFailure;
	if (!bExists || bIoFailure)
	{
		Slot.bCompleted = true;
		FinalizePersistenceLoad(LoadEpoch);
		return;
	}

	const TWeakObjectPtr<UAIREGameplayInventorySubsystem> WeakThis(this);
	UGameplayStatics::AsyncLoadGameFromSlot(
		Slot.SlotName,
		AIREGameplayInventoryPersistence::UserIndex,
		FAsyncLoadGameFromSlotDelegate::CreateWeakLambda(
			this,
			[WeakThis, SlotIndex, LoadEpoch](
				const FString&,
				const int32,
				USaveGame* LoadedGame)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleSlotLoaded(
						SlotIndex,
						LoadEpoch,
						LoadedGame);
				}
			}));
}

void UAIREGameplayInventorySubsystem::HandleSlotLoaded(
	const int32 SlotIndex,
	const uint64 LoadEpoch,
	USaveGame* LoadedGame)
{
	if (LoadEpoch != PersistenceEpoch
		|| bPersistenceShuttingDown
		|| !PersistenceLoadSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FAIREPersistenceLoadSlotState& Slot =
		PersistenceLoadSlots[SlotIndex];
	if (const UAIREGameplayInventorySaveGame* InventorySave =
		Cast<UAIREGameplayInventorySaveGame>(LoadedGame))
	{
		Slot.LoadedEnvelope = InventorySave->Envelope;
	}
	Slot.bCompleted = true;
	FinalizePersistenceLoad(LoadEpoch);
}

void UAIREGameplayInventorySubsystem::FinalizePersistenceLoad(
	const uint64 LoadEpoch)
{
	if (LoadEpoch != PersistenceEpoch
		|| bPersistenceShuttingDown
		|| PersistenceLoadSlots.Num() != 2
		|| PersistenceLoadSlots.ContainsByPredicate(
			[](const FAIREPersistenceLoadSlotState& Slot)
			{
				return !Slot.bCompleted;
			}))
	{
		return;
	}

	int32 SelectedIndex = INDEX_NONE;
	FAIREInventorySaveEnvelope SelectedEnvelope;
	bool bAnyInvalid = false;
	bool bAnyIoFailure = false;
	bool bAnyExisting = false;
	for (int32 Index = 0; Index < PersistenceLoadSlots.Num(); ++Index)
	{
		FAIREPersistenceLoadSlotState& Slot = PersistenceLoadSlots[Index];
		bAnyExisting |= Slot.bExists;
		bAnyIoFailure |= Slot.bIoFailure;
		if (!Slot.LoadedEnvelope.IsSet())
		{
			bAnyInvalid |= Slot.bExists && !Slot.bIoFailure;
			continue;
		}

		FAIREInventorySaveEnvelope NormalizedEnvelope;
		EAIREInventoryPersistenceResultCode ValidationCode =
			EAIREInventoryPersistenceResultCode::NotStarted;
		if (!ValidatePersistenceEnvelope(
				Slot.LoadedEnvelope.GetValue(),
				NormalizedEnvelope,
				ValidationCode))
		{
			Slot.ValidationCode = ValidationCode;
			bAnyInvalid = true;
			continue;
		}
		Slot.ValidationCode = EAIREInventoryPersistenceResultCode::Succeeded;
		if (SelectedIndex == INDEX_NONE
			|| NormalizedEnvelope.Generation > SelectedEnvelope.Generation)
		{
			SelectedIndex = Index;
			SelectedEnvelope = MoveTemp(NormalizedEnvelope);
		}
	}

	bPersistenceLoadComplete = true;
	if (SelectedIndex != INDEX_NONE
		&& CommitPersistenceEnvelope(SelectedEnvelope))
	{
		LatestPersistenceGeneration = SelectedEnvelope.Generation;
		HighestIssuedPersistenceGeneration = SelectedEnvelope.Generation;
		LatestPersistenceSlotName =
			PersistenceLoadSlots[SelectedIndex].SlotName;
		LastIssuedPersistenceSlotName = LatestPersistenceSlotName;
		const bool bUsedFallback = bAnyInvalid || bAnyIoFailure;
		CompletePersistenceStartup(MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Load,
			bUsedFallback
				? EAIREInventoryPersistenceResultCode::SucceededWithFallback
				: EAIREInventoryPersistenceResultCode::Succeeded,
			SelectedEnvelope.Generation,
			bUsedFallback));
		return;
	}

	if (!bAnyExisting && !bAnyIoFailure)
	{
		bShouldSeedFreshSharedStorage = true;
		FinalizeFreshPersistenceStateIfPossible();
		return;
	}

	bSuppressPersistenceDirty = true;
	CreateEmptyContainers();
	AppliedMutations.Reset();
	AppliedMutationOrder.Reset();
	TransientAppliedMutations.Reset();
	TransientAppliedMutationOrder.Reset();
	AppliedWorkResults.Reset();
	AppliedWorkResultOrder.Reset();
	TransientAppliedWorkResults.Reset();
	TransientAppliedWorkResultOrder.Reset();
	AppliedImportCandidateIds.Reset();
	AppliedImportCandidateOrder.Reset();
	AppliedImportOperationIds.Reset();
	AppliedImportOperationOrder.Reset();
	CachedPlayerPersistenceState = FAIREInventoryPersistedPlayerState();
	CachedPlayerPersistenceState.InventoryCapacity =
		AIREGameplayInventoryPersistence::PlayerInventoryCapacity;
	bHasPlayerPersistenceState = true;
	PendingCompanionConfig.Reset();
	bMakoInventoryInitialized = true;
	bShouldSeedFreshSharedStorage = false;
	BroadcastContainerChanged(Containers.FindChecked(GetMakoContainerId()));
	BroadcastContainerChanged(
		Containers.FindChecked(GetSharedStorageContainerId()));
	bSuppressPersistenceDirty = false;
	CompletePersistenceStartup(MakePersistenceResult(
		EAIREInventoryPersistenceOperation::Load,
		bAnyIoFailure
			? EAIREInventoryPersistenceResultCode::IoFailure
			: EAIREInventoryPersistenceResultCode::SafeEmptyNoValidSave));
}

void UAIREGameplayInventorySubsystem::FinalizeFreshPersistenceStateIfPossible()
{
	if (bPersistenceReady || !bPersistenceLoadComplete)
	{
		return;
	}
	if (PendingCompanionConfig.IsValid())
	{
		EnsureMakoInventoryInitialized(PendingCompanionConfig.Get());
	}
}

void UAIREGameplayInventorySubsystem::CompletePersistenceStartup(
	const FAIREInventoryPersistenceResult& Result)
{
	if (bPersistenceReady || bPersistenceShuttingDown)
	{
		return;
	}
	ApplyOrInitializeRegisteredPlayerState();
	bPersistenceReady = true;
	LastPersistenceLoadResult = Result;
	PersistenceReadyDelegate.Broadcast(Result);
	if (bPersistenceDirty)
	{
		TryStartPersistenceSave();
	}
}

bool UAIREGameplayInventorySubsystem::CapturePlayerPersistenceState(
	const UAI_REPlayerInventoryComponent& PlayerInventory,
	FAIREInventoryPersistedPlayerState& OutState,
	EAIREInventoryPersistenceResultCode& OutCode) const
{
	FAIREInventoryPersistedPlayerState Candidate;
	Candidate.InventoryCapacity = PlayerInventory.MaxSlots;
	Candidate.Revision = PlayerInventory.Revision;
	Candidate.Equipment.EquippedItemId =
		PlayerInventory.EquippedWeaponItemId;
	for (const FInventoryItemStack& Item : PlayerInventory.Items)
	{
		FAIREInventoryPersistedStack& Stack =
			Candidate.ItemStacks.AddDefaulted_GetRef();
		Stack.SlotIndex = Item.SlotIndex;
		Stack.ItemId = Item.ItemId;
		Stack.Count = Item.Count;
	}
	return ValidatePlayerPersistenceState(
		Candidate,
		OutState,
		OutCode);
}

bool UAIREGameplayInventorySubsystem::ValidatePlayerPersistenceState(
	const FAIREInventoryPersistedPlayerState& State,
	FAIREInventoryPersistedPlayerState& OutNormalizedState,
	EAIREInventoryPersistenceResultCode& OutCode) const
{
	OutNormalizedState = FAIREInventoryPersistedPlayerState();
	const int32 MaxPlayerStacks =
		AIREGameplayInventoryPersistence::PlayerInventoryCapacity
		+ AIREGameplayInventoryPersistence::PlayerQuickSlotCount;
	if (State.InventoryCapacity
			!= AIREGameplayInventoryPersistence::PlayerInventoryCapacity
		|| State.Revision < 0
		|| State.ItemStacks.Num() > MaxPlayerStacks)
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidPayload;
		return false;
	}

	FAIREInventoryPersistedPlayerState Normalized = State;
	TSet<int32> SeenSlots;
	for (const FAIREInventoryPersistedStack& Stack : State.ItemStacks)
	{
		const bool bNormalSlot = Stack.SlotIndex >= 0
			&& Stack.SlotIndex < State.InventoryCapacity;
		const bool bQuickSlot =
			Stack.SlotIndex
				>= AIREGameplayInventoryPersistence::PlayerQuickSlotStart
			&& Stack.SlotIndex
				< AIREGameplayInventoryPersistence::PlayerQuickSlotStart
					+ AIREGameplayInventoryPersistence::PlayerQuickSlotCount;
		if ((!bNormalSlot && !bQuickSlot)
			|| SeenSlots.Contains(Stack.SlotIndex)
			|| Stack.Count <= 0)
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidPayload;
			return false;
		}

		FAIREItemRules Rules;
		if (Stack.ItemId.IsNone()
			|| !IsStableId(Stack.ItemId.ToString())
			|| !ResolveItemRules(Stack.ItemId, false, Rules)
			|| Stack.Count > Rules.MaxStackSize)
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidItem;
			return false;
		}
		SeenSlots.Add(Stack.SlotIndex);
	}
	Normalized.ItemStacks.Sort(
		[](const FAIREInventoryPersistedStack& Left,
			const FAIREInventoryPersistedStack& Right)
		{
			return Left.SlotIndex < Right.SlotIndex;
		});

	if (!State.Equipment.EquippedItemId.IsNone())
	{
		FAIREItemRules EquipmentRules;
		bool bIsPlayerWeapon = false;
#if WITH_DEV_AUTOMATION_TESTS
		bIsPlayerWeapon = GIsAutomationTesting
			&& (State.Equipment.EquippedItemId
					== FName(TEXT("AIRE.Test.WeaponA"))
				|| State.Equipment.EquippedItemId
					== FName(TEXT("AIRE.Test.WeaponB")));
#endif
		if (!bIsPlayerWeapon)
		{
			UGameInstance* GameInstance = GetGameInstance();
			const UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
				? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
				: nullptr;
			bIsPlayerWeapon = IsValid(ItemSubsystem)
				&& IsValid(Cast<UAI_REWeaponItemDataAsset>(
					ItemSubsystem->GetItemDataAsset(
						State.Equipment.EquippedItemId)));
		}
		if (!IsStableId(State.Equipment.EquippedItemId.ToString())
			|| !ResolveItemRules(
				State.Equipment.EquippedItemId,
				false,
				EquipmentRules)
			|| !EquipmentRules.bIsWeapon
			|| !bIsPlayerWeapon)
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidItem;
			return false;
		}
	}

	OutNormalizedState = MoveTemp(Normalized);
	OutCode = EAIREInventoryPersistenceResultCode::Succeeded;
	return true;
}

void UAIREGameplayInventorySubsystem::ApplyOrInitializeRegisteredPlayerState()
{
	UAI_REPlayerInventoryComponent* PlayerInventory =
		RegisteredPlayerInventory.Get();
	if (!IsValid(PlayerInventory))
	{
		return;
	}

	if (!bHasPlayerPersistenceState)
	{
		EAIREInventoryPersistenceResultCode CaptureCode =
			EAIREInventoryPersistenceResultCode::NotStarted;
		if (!CapturePlayerPersistenceState(
				*PlayerInventory,
				CachedPlayerPersistenceState,
				CaptureCode))
		{
			PlayerInventory->bPersistenceReadyForGameplay = true;
			return;
		}
		bHasPlayerPersistenceState = true;
		bPersistenceDirty = true;
		PlayerInventory->bPersistenceReadyForGameplay = true;
		return;
	}

	bApplyingPlayerPersistenceState = true;
	TArray<FInventoryItemStack> NewItems;
	NewItems.Reserve(CachedPlayerPersistenceState.ItemStacks.Num());
	for (const FAIREInventoryPersistedStack& PersistedStack
		: CachedPlayerPersistenceState.ItemStacks)
	{
		FInventoryItemStack& Item = NewItems.AddDefaulted_GetRef();
		Item.SlotIndex = PersistedStack.SlotIndex;
		Item.ItemId = PersistedStack.ItemId;
		Item.Count = PersistedStack.Count;
	}
	PlayerInventory->Items = MoveTemp(NewItems);
	PlayerInventory->MaxSlots =
		CachedPlayerPersistenceState.InventoryCapacity;
	PlayerInventory->Revision = CachedPlayerPersistenceState.Revision;
	PlayerInventory->EquippedWeaponItemId =
		CachedPlayerPersistenceState.Equipment.EquippedItemId;
	PlayerInventory->bPersistenceReadyForGameplay = true;
	PlayerInventory->OnInventoryChanged.Broadcast();

	const FName EquippedItemId = PlayerInventory->EquippedWeaponItemId;
	bool bEquipmentRestored = EquippedItemId.IsNone();
	if (!EquippedItemId.IsNone() && RegisteredPlayerCombat.IsValid())
	{
		UGameInstance* GameInstance = GetGameInstance();
		UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
			? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
			: nullptr;
		UAI_REWeaponItemDataAsset* WeaponItem = IsValid(ItemSubsystem)
			? Cast<UAI_REWeaponItemDataAsset>(
				ItemSubsystem->GetItemDataAsset(EquippedItemId))
			: nullptr;
		bEquipmentRestored = IsValid(WeaponItem)
			&& RegisteredPlayerCombat->TryEquipWeapon(WeaponItem);
	}
	if (!EquippedItemId.IsNone())
	{
		PlayerInventory->OnWeaponEquipResult.Broadcast(
			EquippedItemId,
			bEquipmentRestored);
	}
	bApplyingPlayerPersistenceState = false;
}

bool UAIREGameplayInventorySubsystem::ValidatePersistenceEnvelope(
	const FAIREInventorySaveEnvelope& Envelope,
	FAIREInventorySaveEnvelope& OutNormalizedEnvelope,
	EAIREInventoryPersistenceResultCode& OutCode) const
{
	OutNormalizedEnvelope = FAIREInventorySaveEnvelope();
	if (Envelope.FormatVersion
		!= AIREGameplayInventoryPersistence::SaveFormatVersion)
	{
		OutCode =
			EAIREInventoryPersistenceResultCode::UnsupportedFormatVersion;
		return false;
	}
	if (Envelope.ContentVersion
		!= AIREGameplayInventoryPersistence::ItemContentVersion)
	{
		OutCode =
			EAIREInventoryPersistenceResultCode::UnsupportedContentVersion;
		return false;
	}
	if (Envelope.Generation <= 0)
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidGeneration;
		return false;
	}
	if (!Envelope.SourceSessionId.IsValid())
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidSession;
		return false;
	}

	const FAIREInventorySessionScope CanonicalScope =
		MakeCanonicalPersistenceScope();
	if (Envelope.ProfileId != CanonicalScope.ProfileId
		|| Envelope.SaveSlotId != CanonicalScope.SaveSlotId
		|| Envelope.CompanionId != CanonicalScope.CompanionId)
	{
		OutCode = EAIREInventoryPersistenceResultCode::ScopeMismatch;
		return false;
	}
	if (Envelope.Containers.Num() != 2)
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidContainer;
		return false;
	}

	FAIREInventorySaveEnvelope Normalized = Envelope;
	TSet<FName> SeenContainerIds;
	for (FAIREInventoryPersistedContainer& Container : Normalized.Containers)
	{
		Container.ContainerId = NormalizeContainerId(Container.ContainerId);
		const bool bIsMako = Container.ContainerId == GetMakoContainerId();
		const bool bIsStorage =
			Container.ContainerId == GetSharedStorageContainerId();
		const int32 ExpectedCapacity = bIsMako
			? AIREGameplayInventory::MakoItemSlotCapacity
			: AIREGameplayInventory::SharedStorageSlotCapacity;
		if ((!bIsMako && !bIsStorage)
			|| SeenContainerIds.Contains(Container.ContainerId)
			|| Container.Capacity != ExpectedCapacity
			|| Container.Revision < 0
			|| Container.ItemStacks.Num() > Container.Capacity
			|| (bIsStorage
				&& !Container.Equipment.EquippedItemId.IsNone()))
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidContainer;
			return false;
		}
		SeenContainerIds.Add(Container.ContainerId);

		TSet<int32> SeenSlots;
		for (const FAIREInventoryPersistedStack& Stack
			: Container.ItemStacks)
		{
			if (Stack.SlotIndex < 0
				|| Stack.SlotIndex >= Container.Capacity
				|| SeenSlots.Contains(Stack.SlotIndex)
				|| Stack.Count <= 0)
			{
				OutCode = EAIREInventoryPersistenceResultCode::InvalidPayload;
				return false;
			}
			FAIREItemRules Rules;
			if (Stack.ItemId.IsNone()
				|| !IsStableId(Stack.ItemId.ToString())
				|| !ResolveItemRules(Stack.ItemId, false, Rules)
				|| Stack.Count > Rules.MaxStackSize)
			{
				OutCode = EAIREInventoryPersistenceResultCode::InvalidItem;
				return false;
			}
			SeenSlots.Add(Stack.SlotIndex);
		}
		Container.ItemStacks.Sort(
			[](const FAIREInventoryPersistedStack& Left,
				const FAIREInventoryPersistedStack& Right)
			{
				return Left.SlotIndex < Right.SlotIndex;
			});

		if (!Container.Equipment.EquippedItemId.IsNone())
		{
			FAIREItemRules EquipmentRules;
			if (!bIsMako
				|| !IsStableId(
					Container.Equipment.EquippedItemId.ToString())
				|| !ResolveItemRules(
					Container.Equipment.EquippedItemId,
					true,
					EquipmentRules)
				|| !EquipmentRules.bIsWeapon)
			{
				OutCode = EAIREInventoryPersistenceResultCode::InvalidItem;
				return false;
			}
		}
	}
	if (!SeenContainerIds.Contains(GetMakoContainerId())
		|| !SeenContainerIds.Contains(GetSharedStorageContainerId()))
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidContainer;
		return false;
	}
	Normalized.Containers.Sort(
		[](const FAIREInventoryPersistedContainer& Left,
			const FAIREInventoryPersistedContainer& Right)
		{
			return Left.ContainerId.ToString() < Right.ContainerId.ToString();
		});

	FAIREInventoryPersistedPlayerState NormalizedPlayer;
	if (!ValidatePlayerPersistenceState(
			Normalized.Player,
			NormalizedPlayer,
			OutCode))
	{
		return false;
	}
	Normalized.Player = MoveTemp(NormalizedPlayer);

	if (Normalized.Mutations.Num()
			> AIREGameplayInventoryPersistence::MaxLedgerEntries
		|| Normalized.WorkResults.Num()
			> AIREGameplayInventoryPersistence::MaxLedgerEntries
		|| Normalized.ImportCandidateIds.Num()
			> AIREGameplayInventoryPersistence::MaxLedgerEntries
		|| Normalized.ImportOperationIds.Num()
			> AIREGameplayInventoryPersistence::MaxLedgerEntries
		|| Normalized.OfflineTasks.Num()
			> AIREGameplayInventoryPersistence::MaxLedgerEntries)
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
		return false;
	}

	TSet<FGuid> SeenMutationIds;
	for (const FAIREInventoryPersistedMutationEntry& Entry
		: Normalized.Mutations)
	{
		if (!Entry.MutationId.IsValid()
			|| SeenMutationIds.Contains(Entry.MutationId)
			|| Entry.Code != EAIREInventoryMutationCode::Succeeded
			|| Entry.SourceRevision < INDEX_NONE
			|| Entry.DestinationRevision < INDEX_NONE)
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
			return false;
		}
		SeenMutationIds.Add(Entry.MutationId);
	}

	TSet<FGuid> SeenWorkIds;
	for (const FAIREInventoryPersistedWorkEntry& Entry
		: Normalized.WorkResults)
	{
		FAIREItemRules DeliveredRules;
		const bool bMakoDestination = Entry.Destination
			== EAIREInventoryWorkResultDestination::Mako;
		const bool bStorageDestination = Entry.Destination
			== EAIREInventoryWorkResultDestination::SharedStorage;
		if (!Entry.OperationId.IsValid()
			|| SeenWorkIds.Contains(Entry.OperationId)
			|| SeenMutationIds.Contains(Entry.OperationId)
			|| Entry.Code != EAIREInventoryMutationCode::Succeeded
			|| (!bMakoDestination && !bStorageDestination)
			|| Entry.DeliveredItemId.IsNone()
			|| Entry.DeliveredItemCount <= 0
			|| !ResolveItemRules(
				Entry.DeliveredItemId,
				bMakoDestination,
				DeliveredRules)
			|| Entry.MakoRevision < 0
			|| Entry.StorageRevision < 0)
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
			return false;
		}
		SeenWorkIds.Add(Entry.OperationId);
	}

	TSet<FString> SeenCandidateIds;
	for (const FString& CandidateId : Normalized.ImportCandidateIds)
	{
		if (!IsStableId(CandidateId)
			|| SeenCandidateIds.Contains(CandidateId))
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
			return false;
		}
		SeenCandidateIds.Add(CandidateId);
	}
	TSet<FString> SeenOperationIds;
	for (const FString& OperationId : Normalized.ImportOperationIds)
	{
		if (!IsStableId(OperationId)
			|| SeenOperationIds.Contains(OperationId))
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
			return false;
		}
		SeenOperationIds.Add(OperationId);
	}
	TSet<FString> SeenOfflineTaskIds;
	for (const FAIREInventoryPersistedOfflineTaskEntry& Task
		: Normalized.OfflineTasks)
	{
		if (!IsStableId(Task.TaskId)
			|| SeenOfflineTaskIds.Contains(Task.TaskId))
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
			return false;
		}
		SeenOfflineTaskIds.Add(Task.TaskId);
	}

	OutNormalizedEnvelope = MoveTemp(Normalized);
	OutCode = EAIREInventoryPersistenceResultCode::Succeeded;
	return true;
}

bool UAIREGameplayInventorySubsystem::CommitPersistenceEnvelope(
	const FAIREInventorySaveEnvelope& Envelope)
{
	const bool bSaveRequestedBeforeCommit = bPersistenceDirty;
	FAIREInventorySaveEnvelope Normalized;
	EAIREInventoryPersistenceResultCode ValidationCode =
		EAIREInventoryPersistenceResultCode::NotStarted;
	if (!ValidatePersistenceEnvelope(
			Envelope,
			Normalized,
			ValidationCode))
	{
		return false;
	}

	TMap<FName, FAIREContainerState> NewContainers;
	for (const FAIREInventoryPersistedContainer& Persisted
		: Normalized.Containers)
	{
		FAIREContainerState State;
		State.ContainerId = Persisted.ContainerId;
		State.Capacity = Persisted.Capacity;
		State.Revision = Persisted.Revision;
		for (const FAIREInventoryPersistedStack& PersistedStack
			: Persisted.ItemStacks)
		{
			FAIREInventoryItemStackSnapshot& Stack =
				State.ItemStacks.AddDefaulted_GetRef();
			Stack.SlotIndex = PersistedStack.SlotIndex;
			Stack.ItemId = PersistedStack.ItemId;
			Stack.Count = PersistedStack.Count;
		}
		State.EquippedItemId = Persisted.Equipment.EquippedItemId;
		State.EquipmentTransition = EAIREEquipmentTransitionState::Idle;
		NewContainers.Add(State.ContainerId, MoveTemp(State));
	}

	TMap<FGuid, FAIREInventoryMutationResult> NewMutations;
	TArray<FGuid> NewMutationOrder;
	for (const FAIREInventoryPersistedMutationEntry& Entry
		: Normalized.Mutations)
	{
		FAIREInventoryMutationResult Result;
		Result.MutationId = Entry.MutationId;
		Result.Code = Entry.Code;
		Result.SourceRevision = Entry.SourceRevision;
		Result.DestinationRevision = Entry.DestinationRevision;
		NewMutationOrder.Add(Entry.MutationId);
		NewMutations.Add(Entry.MutationId, Result);
	}
	TMap<FGuid, FAIREInventoryWorkResult> NewWorkResults;
	TArray<FGuid> NewWorkOrder;
	for (const FAIREInventoryPersistedWorkEntry& Entry
		: Normalized.WorkResults)
	{
		FAIREInventoryWorkResult Result;
		Result.Code = Entry.Code;
		Result.Destination = Entry.Destination;
		Result.DeliveredItem.ItemId = Entry.DeliveredItemId;
		Result.DeliveredItem.Count = Entry.DeliveredItemCount;
		Result.MakoRevision = Entry.MakoRevision;
		Result.StorageRevision = Entry.StorageRevision;
		NewWorkOrder.Add(Entry.OperationId);
		NewWorkResults.Add(Entry.OperationId, Result);
	}

	bSuppressPersistenceDirty = true;
	Containers = MoveTemp(NewContainers);
	AppliedMutations = MoveTemp(NewMutations);
	AppliedMutationOrder = MoveTemp(NewMutationOrder);
	TransientAppliedMutations.Reset();
	TransientAppliedMutationOrder.Reset();
	AppliedWorkResults = MoveTemp(NewWorkResults);
	AppliedWorkResultOrder = MoveTemp(NewWorkOrder);
	TransientAppliedWorkResults.Reset();
	TransientAppliedWorkResultOrder.Reset();
	AppliedImportCandidateIds.Reset();
	for (const FString& CandidateId : Normalized.ImportCandidateIds)
	{
		AppliedImportCandidateIds.Add(CandidateId);
	}
	AppliedImportCandidateOrder = Normalized.ImportCandidateIds;
	AppliedImportOperationIds.Reset();
	for (const FString& OperationId : Normalized.ImportOperationIds)
	{
		AppliedImportOperationIds.Add(OperationId);
	}
	AppliedImportOperationOrder = Normalized.ImportOperationIds;
	AppliedOfflineTaskIds.Reset();
	AppliedOfflineTaskOrder.Reset();
	for (const FAIREInventoryPersistedOfflineTaskEntry& Task
		: Normalized.OfflineTasks)
	{
		AppliedOfflineTaskIds.Add(Task.TaskId);
		AppliedOfflineTaskOrder.Add(Task.TaskId);
	}
	CachedPlayerPersistenceState = Normalized.Player;
	bHasPlayerPersistenceState = true;
	PendingCompanionConfig.Reset();
	bMakoInventoryInitialized = true;
	bShouldSeedFreshSharedStorage = false;
	bPersistenceDirty = bSaveRequestedBeforeCommit;
	ApplyOrInitializeRegisteredPlayerState();
	BroadcastContainerChanged(Containers.FindChecked(GetMakoContainerId()));
	BroadcastContainerChanged(
		Containers.FindChecked(GetSharedStorageContainerId()));
	bSuppressPersistenceDirty = false;
	return true;
}

bool UAIREGameplayInventorySubsystem::BuildPersistenceEnvelope(
	const int64 Generation,
	FAIREInventorySaveEnvelope& OutEnvelope,
	EAIREInventoryPersistenceResultCode& OutCode) const
{
	OutEnvelope = FAIREInventorySaveEnvelope();
	if (Generation <= 0)
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidGeneration;
		return false;
	}
	if (!InventorySessionId.IsValid())
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidSession;
		return false;
	}
	if (!(SessionScope == MakeCanonicalPersistenceScope()))
	{
		OutCode = EAIREInventoryPersistenceResultCode::ScopeMismatch;
		return false;
	}

	const FAIREContainerState* Mako = FindContainer(GetMakoContainerId());
	const FAIREContainerState* Storage =
		FindContainer(GetSharedStorageContainerId());
	if (!Mako || !Storage)
	{
		OutCode = EAIREInventoryPersistenceResultCode::InvalidContainer;
		return false;
	}
	if (IsEquipmentTransitionActive(*Mako))
	{
		OutCode =
			EAIREInventoryPersistenceResultCode::DeferredEquipmentTransition;
		return false;
	}

	FAIREInventorySaveEnvelope Candidate;
	Candidate.Generation = Generation;
	Candidate.ProfileId = SessionScope.ProfileId;
	Candidate.SaveSlotId = SessionScope.SaveSlotId;
	Candidate.CompanionId = SessionScope.CompanionId;
	Candidate.SourceSessionId = InventorySessionId;
	for (const FAIREContainerState* State : { Mako, Storage })
	{
		FAIREInventoryPersistedContainer& Container =
			Candidate.Containers.AddDefaulted_GetRef();
		Container.ContainerId = State->ContainerId;
		Container.Capacity = State->Capacity;
		Container.Revision = State->Revision;
		TArray<FAIREInventoryItemStackSnapshot> SortedStacks =
			State->ItemStacks;
		SortStacks(SortedStacks);
		for (const FAIREInventoryItemStackSnapshot& Stack : SortedStacks)
		{
			FAIREInventoryPersistedStack& PersistedStack =
				Container.ItemStacks.AddDefaulted_GetRef();
			PersistedStack.SlotIndex = Stack.SlotIndex;
			PersistedStack.ItemId = Stack.ItemId;
			PersistedStack.Count = Stack.Count;
		}
		Container.Equipment.EquippedItemId =
			State->ContainerId == GetMakoContainerId()
			? State->EquippedItemId
			: NAME_None;
	}
	for (const FGuid& MutationId : AppliedMutationOrder)
	{
		const FAIREInventoryMutationResult* Result =
			AppliedMutations.Find(MutationId);
		if (!Result)
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
			return false;
		}
		FAIREInventoryPersistedMutationEntry& Entry =
			Candidate.Mutations.AddDefaulted_GetRef();
		Entry.MutationId = Result->MutationId;
		Entry.Code = Result->Code;
		Entry.SourceRevision = Result->SourceRevision;
		Entry.DestinationRevision = Result->DestinationRevision;
	}
	for (const FGuid& OperationId : AppliedWorkResultOrder)
	{
		const FAIREInventoryWorkResult* Result =
			AppliedWorkResults.Find(OperationId);
		if (!Result)
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
			return false;
		}
		FAIREInventoryPersistedWorkEntry& Entry =
			Candidate.WorkResults.AddDefaulted_GetRef();
		Entry.OperationId = OperationId;
		Entry.Code = Result->Code;
		Entry.Destination = Result->Destination;
		Entry.DeliveredItemId = Result->DeliveredItem.ItemId;
		Entry.DeliveredItemCount = Result->DeliveredItem.Count;
		Entry.MakoRevision = Result->MakoRevision;
		Entry.StorageRevision = Result->StorageRevision;
	}
	Candidate.ImportCandidateIds = AppliedImportCandidateOrder;
	Candidate.ImportOperationIds = AppliedImportOperationOrder;
	for (const FString& TaskId : AppliedOfflineTaskOrder)
	{
		if (!AppliedOfflineTaskIds.Contains(TaskId))
		{
			OutCode = EAIREInventoryPersistenceResultCode::InvalidLedger;
			return false;
		}
		FAIREInventoryPersistedOfflineTaskEntry& Entry =
			Candidate.OfflineTasks.AddDefaulted_GetRef();
		Entry.TaskId = TaskId;
	}
	if (!bHasPlayerPersistenceState)
	{
		OutCode =
			EAIREInventoryPersistenceResultCode::DeferredPlayerRegistration;
		return false;
	}
	Candidate.Player = CachedPlayerPersistenceState;

	FAIREInventorySaveEnvelope Normalized;
	if (!ValidatePersistenceEnvelope(Candidate, Normalized, OutCode))
	{
		return false;
	}
	OutEnvelope = MoveTemp(Normalized);
	return true;
}

void UAIREGameplayInventorySubsystem::MarkPersistenceDirty()
{
	bPersistenceDirty = true;
	if (!bPersistenceLifecycleInitialized
		|| !bPersistenceReady
		|| bPersistenceShuttingDown)
	{
		return;
	}
	TryStartPersistenceSave();
}

FAIREInventoryPersistenceResult
UAIREGameplayInventorySubsystem::TryStartPersistenceSave()
{
	if (bPersistenceShuttingDown)
	{
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::ShuttingDown,
			LatestPersistenceGeneration);
		return LastPersistenceSaveResult;
	}
	if (!bPersistenceReady || !bPersistenceLifecycleInitialized)
	{
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::InProgress,
			LatestPersistenceGeneration);
		return LastPersistenceSaveResult;
	}
	if (bPersistenceSaveInFlight)
	{
		bPersistenceDirty = true;
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::Coalesced,
			HighestIssuedPersistenceGeneration);
		return LastPersistenceSaveResult;
	}
	if (!bPersistenceDirty)
	{
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::NoChanges,
			LatestPersistenceGeneration);
		return LastPersistenceSaveResult;
	}
	const int64 GenerationBase = FMath::Max(
		LatestPersistenceGeneration,
		HighestIssuedPersistenceGeneration);
	if (GenerationBase == MAX_int64)
	{
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::InvalidGeneration,
			LatestPersistenceGeneration);
		return LastPersistenceSaveResult;
	}

	const int64 Generation = GenerationBase + 1;
	FAIREInventorySaveEnvelope Envelope;
	EAIREInventoryPersistenceResultCode BuildCode =
		EAIREInventoryPersistenceResultCode::NotStarted;
	if (!BuildPersistenceEnvelope(Generation, Envelope, BuildCode))
	{
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			BuildCode,
			LatestPersistenceGeneration);
		return LastPersistenceSaveResult;
	}

	UAIREGameplayInventorySaveGame* SaveGame =
		NewObject<UAIREGameplayInventorySaveGame>();
	SaveGame->Envelope = MoveTemp(Envelope);
	const FString TargetSlotName = GetNextPersistenceSlotName();
	const uint64 SaveEpoch = ++ActiveSaveEpoch;
	const FGuid SaveSessionId = InventorySessionId;
	LastIssuedPersistenceSlotName = TargetSlotName;
	HighestIssuedPersistenceGeneration = Generation;
	bPersistenceSaveInFlight = true;
	bPersistenceDirty = false;
	LastPersistenceSaveResult = MakePersistenceResult(
		EAIREInventoryPersistenceOperation::Save,
		EAIREInventoryPersistenceResultCode::InProgress,
		Generation);

	const TWeakObjectPtr<UAIREGameplayInventorySubsystem> WeakThis(this);
	UGameplayStatics::AsyncSaveGameToSlot(
		SaveGame,
		TargetSlotName,
		AIREGameplayInventoryPersistence::UserIndex,
		FAsyncSaveGameToSlotDelegate::CreateWeakLambda(
			this,
			[WeakThis, SaveEpoch, SaveSessionId, Generation](
				const FString& SlotName,
				const int32,
				const bool bSucceeded)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandlePersistenceSaveCompleted(
						SlotName,
						SaveEpoch,
						SaveSessionId,
						Generation,
						bSucceeded);
				}
			}));
	return LastPersistenceSaveResult;
}

void UAIREGameplayInventorySubsystem::HandlePersistenceSaveCompleted(
	const FString& SlotName,
	const uint64 SaveEpoch,
	const FGuid& SaveSessionId,
	const int64 Generation,
	const bool bSucceeded)
{
	if (bPersistenceShuttingDown
		|| SaveEpoch != ActiveSaveEpoch
		|| SaveSessionId != InventorySessionId
		|| Generation != HighestIssuedPersistenceGeneration
		|| SlotName != LastIssuedPersistenceSlotName)
	{
		return;
	}

	bPersistenceSaveInFlight = false;
	if (!bSucceeded)
	{
		bPersistenceDirty = true;
		LastIssuedPersistenceSlotName = LatestPersistenceSlotName;
		LastPersistenceSaveResult = MakePersistenceResult(
			EAIREInventoryPersistenceOperation::Save,
			EAIREInventoryPersistenceResultCode::IoFailure,
			Generation);
		PersistenceSaveCompletedDelegate.Broadcast(LastPersistenceSaveResult);
		return;
	}

	LatestPersistenceGeneration = Generation;
	HighestIssuedPersistenceGeneration = Generation;
	LatestPersistenceSlotName = SlotName;
	LastIssuedPersistenceSlotName = SlotName;
	LastPersistenceSaveResult = MakePersistenceResult(
		EAIREInventoryPersistenceOperation::Save,
		EAIREInventoryPersistenceResultCode::Succeeded,
		Generation);
	PersistenceSaveCompletedDelegate.Broadcast(LastPersistenceSaveResult);
	if (bPersistenceDirty)
	{
		TryStartPersistenceSave();
	}
}

FString UAIREGameplayInventorySubsystem::GetNextPersistenceSlotName() const
{
	const bool bUnresolvedIssuedSlot =
		HighestIssuedPersistenceGeneration > LatestPersistenceGeneration
		&& LastIssuedPersistenceSlotName != LatestPersistenceSlotName;
	const FString& ReferenceSlotName =
		bPersistenceSaveInFlight || bUnresolvedIssuedSlot
		? LastIssuedPersistenceSlotName
		: LatestPersistenceSlotName;
	if (ReferenceSlotName
		== AIREGameplayInventoryPersistence::PrimarySlotName)
	{
		return AIREGameplayInventoryPersistence::PreviousSlotName;
	}
	return AIREGameplayInventoryPersistence::PrimarySlotName;
}

FAIREInventoryPersistenceResult
UAIREGameplayInventorySubsystem::MakePersistenceResult(
	const EAIREInventoryPersistenceOperation Operation,
	const EAIREInventoryPersistenceResultCode Code,
	const int64 Generation,
	const bool bUsedFallback) const
{
	FAIREInventoryPersistenceResult Result;
	Result.Operation = Operation;
	Result.Code = Code;
	Result.Generation = Generation;
	Result.bUsedFallback = bUsedFallback;
	return Result;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::ReserveMakoEquipmentSwap(
	const FAIREInventoryEquipRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}

	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	FAIREInventoryMutationResult Validation = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		MakoContainer,
		Request.ExpectedRevision);
	if (Validation.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Validation;
	}
	if (IsEquipmentTransitionActive(*MakoContainer))
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			Request.MutationId,
			MakoContainer->Revision);
	}

	const int32 StackIndex = FindStackIndexBySlot(
		MakoContainer->ItemStacks,
		Request.SourceSlotIndex);
	if (StackIndex == INDEX_NONE)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSlot,
			Request.MutationId,
			MakoContainer->Revision);
	}

	const FName PendingItemId = MakoContainer->ItemStacks[StackIndex].ItemId;
	FAIREItemRules Rules;
	if (!ResolveItemRules(PendingItemId, true, Rules)
		|| !Rules.bIsWeapon
		|| MakoContainer->ItemStacks[StackIndex].Count != 1)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidItem,
			Request.MutationId,
			MakoContainer->Revision);
	}

	MakoContainer->PendingItemId = PendingItemId;
	MakoContainer->PreviousItemId = MakoContainer->EquippedItemId;
	MakoContainer->EquipmentTransition =
		EAIREEquipmentTransitionState::Equipping;
	MakoContainer->EquipmentMutationId = Request.MutationId;
	MakoContainer->ReservedSlotIndex = Request.SourceSlotIndex;
	++MakoContainer->Revision;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		MakoContainer->Revision);
	RecordAppliedMutation(Result, false);
	BroadcastContainerChanged(*MakoContainer);
	return Result;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::CommitMakoEquipmentSwap(
	const FGuid& RequestSessionId,
	const FGuid& MutationId)
{
	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	if (RequestSessionId != InventorySessionId
		|| !MakoContainer
		|| MakoContainer->EquipmentMutationId != MutationId
		|| MakoContainer->EquipmentTransition
			!= EAIREEquipmentTransitionState::Equipping)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSession,
			MutationId);
	}

	const int32 StackIndex = FindStackIndexBySlot(
		MakoContainer->ItemStacks,
		MakoContainer->ReservedSlotIndex);
	if (StackIndex == INDEX_NONE
		|| MakoContainer->ItemStacks[StackIndex].ItemId
			!= MakoContainer->PendingItemId
		|| MakoContainer->ItemStacks[StackIndex].Count != 1)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSlot,
			MutationId,
			MakoContainer->Revision);
	}

	const FName NewEquippedItemId = MakoContainer->PendingItemId;
	if (MakoContainer->PreviousItemId.IsNone())
	{
		MakoContainer->ItemStacks.RemoveAt(StackIndex);
	}
	else
	{
		MakoContainer->ItemStacks[StackIndex].ItemId =
			MakoContainer->PreviousItemId;
		MakoContainer->ItemStacks[StackIndex].Count = 1;
	}
	MakoContainer->EquippedItemId = NewEquippedItemId;
	MakoContainer->PendingItemId = NAME_None;
	MakoContainer->PreviousItemId = NAME_None;
	MakoContainer->EquipmentTransition =
		EAIREEquipmentTransitionState::Idle;
	MakoContainer->EquipmentMutationId.Invalidate();
	MakoContainer->ReservedSlotIndex = INDEX_NONE;
	++MakoContainer->Revision;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		MutationId,
		MakoContainer->Revision);
	RecordAppliedMutation(Result, true);
	BroadcastContainerChanged(*MakoContainer);
	return Result;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::BeginMakoEquipmentRecovery(
	const FGuid& RequestSessionId,
	const FGuid& MutationId)
{
	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	if (RequestSessionId != InventorySessionId
		|| !MakoContainer
		|| MakoContainer->EquipmentMutationId != MutationId
		|| MakoContainer->EquipmentTransition
			!= EAIREEquipmentTransitionState::Equipping)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSession,
			MutationId);
	}

	MakoContainer->EquipmentTransition =
		MakoContainer->PreviousItemId.IsNone()
		? EAIREEquipmentTransitionState::Idle
		: EAIREEquipmentTransitionState::Recovering;
	if (MakoContainer->PreviousItemId.IsNone())
	{
		MakoContainer->PendingItemId = NAME_None;
		MakoContainer->EquipmentMutationId.Invalidate();
		MakoContainer->ReservedSlotIndex = INDEX_NONE;
		RemoveAppliedMutation(MutationId);
	}
	++MakoContainer->Revision;
	BroadcastContainerChanged(*MakoContainer);
	return MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		MutationId,
		MakoContainer->Revision);
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::CompleteMakoEquipmentRecovery(
	const FGuid& RequestSessionId,
	const FGuid& MutationId,
	const bool bSucceeded)
{
	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	if (RequestSessionId != InventorySessionId
		|| !MakoContainer
		|| MakoContainer->EquipmentMutationId != MutationId
		|| MakoContainer->EquipmentTransition
			!= EAIREEquipmentTransitionState::Recovering)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSession,
			MutationId);
	}

	MakoContainer->PendingItemId = NAME_None;
	MakoContainer->PreviousItemId = NAME_None;
	MakoContainer->EquipmentTransition = bSucceeded
		? EAIREEquipmentTransitionState::Idle
		: EAIREEquipmentTransitionState::RecoveryFailed;
	MakoContainer->EquipmentMutationId.Invalidate();
	MakoContainer->ReservedSlotIndex = INDEX_NONE;
	RemoveAppliedMutation(MutationId);
	++MakoContainer->Revision;
	BroadcastContainerChanged(*MakoContainer);
	return MakeResult(
		bSucceeded
			? EAIREInventoryMutationCode::Succeeded
			: EAIREInventoryMutationCode::RecoveryFailed,
		MutationId,
		MakoContainer->Revision);
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::CompleteMakoEquipmentRuntimeRestore(
	const FGuid& RequestSessionId,
	const FName ItemId,
	const bool bSucceeded)
{
	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	if (RequestSessionId != InventorySessionId || !MakoContainer)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSession,
			FGuid());
	}
	if (ItemId.IsNone() || MakoContainer->EquippedItemId != ItemId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidItem,
			FGuid(),
			MakoContainer->Revision);
	}
	if (IsEquipmentTransitionActive(*MakoContainer))
	{
		return MakeResult(
			EAIREInventoryMutationCode::EquipmentBusy,
			FGuid(),
			MakoContainer->Revision);
	}

	const EAIREEquipmentTransitionState NewState = bSucceeded
		? EAIREEquipmentTransitionState::Idle
		: EAIREEquipmentTransitionState::RecoveryFailed;
	if (MakoContainer->EquipmentTransition != NewState)
	{
		MakoContainer->EquipmentTransition = NewState;
		++MakoContainer->Revision;
		BroadcastContainerChanged(*MakoContainer);
	}
	return MakeResult(
		bSucceeded
			? EAIREInventoryMutationCode::Succeeded
			: EAIREInventoryMutationCode::RecoveryFailed,
		FGuid(),
		MakoContainer->Revision);
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::CancelMakoEquipmentSwap(
	const FGuid& RequestSessionId,
	const FGuid& MutationId)
{
	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	if (RequestSessionId != InventorySessionId
		|| !MakoContainer
		|| MakoContainer->EquipmentMutationId != MutationId)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidSession,
			MutationId);
	}

	MakoContainer->PendingItemId = NAME_None;
	MakoContainer->PreviousItemId = NAME_None;
	MakoContainer->EquipmentTransition =
		EAIREEquipmentTransitionState::Idle;
	MakoContainer->EquipmentMutationId.Invalidate();
	MakoContainer->ReservedSlotIndex = INDEX_NONE;
	RemoveAppliedMutation(MutationId);
	++MakoContainer->Revision;
	BroadcastContainerChanged(*MakoContainer);
	return MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		MutationId,
		MakoContainer->Revision);
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREInventoryStackAtomicityTest,
	"AIRE.Inventory.Domain.StackAtomicity",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREInventoryStackAtomicityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<FAIREInventoryItemStackSnapshot> Stacks;
	TestTrue(
		TEXT("Four items fit exactly into two stacks"),
		TryAddToStacks(Stacks, 2, FName(TEXT("test.item")), 4, 2));
	TestEqual(TEXT("Both slots are occupied"), Stacks.Num(), 2);
	TestEqual(TEXT("First stack is full"), Stacks[0].Count, 2);
	TestEqual(TEXT("Second stack is full"), Stacks[1].Count, 2);

	const TArray<FAIREInventoryItemStackSnapshot> FullSnapshot = Stacks;
	TestFalse(
		TEXT("An over-capacity add is rejected"),
		TryAddToStacks(Stacks, 2, FName(TEXT("test.item")), 1, 2));
	TestEqual(
		TEXT("Rejected add preserves stack count"),
		Stacks.Num(),
		FullSnapshot.Num());
	TestEqual(
		TEXT("Rejected add preserves first stack"),
		Stacks[0].Count,
		FullSnapshot[0].Count);
	TestEqual(
		TEXT("Rejected add preserves second stack"),
		Stacks[1].Count,
		FullSnapshot[1].Count);

	TestFalse(
		TEXT("An over-removal is rejected"),
		TryRemoveItemFromStacks(Stacks, FName(TEXT("test.item")), 5));
	TestEqual(TEXT("Rejected removal preserves slots"), Stacks.Num(), 2);
	TestTrue(
		TEXT("An exact removal succeeds"),
		TryRemoveItemFromStacks(Stacks, FName(TEXT("test.item")), 3));
	TestEqual(TEXT("One item remains"), Stacks.Num(), 1);
	TestEqual(TEXT("Remaining quantity is exact"), Stacks[0].Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREInventorySlotMoveTest,
	"AIRE.Inventory.Domain.SlotMove",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREInventorySlotMoveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<FAIREInventoryItemStackSnapshot> Stacks;
	TestTrue(
		TEXT("Initial stack is added"),
		TryAddToStacks(Stacks, 3, FName(TEXT("test.item")), 2, 4));
	TestTrue(
		TEXT("A partial stack moves to an empty slot"),
		TryMoveWithinStacks(Stacks, 3, 0, 2, 1, 4));
	TestEqual(TEXT("Move creates a second stack"), Stacks.Num(), 2);
	TestEqual(TEXT("Source keeps one item"), Stacks[0].Count, 1);
	TestEqual(TEXT("Destination receives one item"), Stacks[1].Count, 1);
	TestEqual(TEXT("Destination slot is stable"), Stacks[1].SlotIndex, 2);

	const TArray<FAIREInventoryItemStackSnapshot> BeforeInvalidMove = Stacks;
	TestFalse(
		TEXT("Same-slot move is rejected"),
		TryMoveWithinStacks(Stacks, 3, 0, 0, 1, 4));
	TestEqual(
		TEXT("Rejected move preserves stack count"),
		Stacks.Num(),
		BeforeInvalidMove.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREGameplayInventoryCapacityTest,
	"AIRE.Inventory.Subsystem.FixedCapacity",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREGameplayInventoryCapacityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UGameInstance> TestGameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
		NewObject<UAIREGameplayInventorySubsystem>(TestGameInstance.Get()));
	const FGuid SessionId = Inventory->ResetInventorySession();

	auto FillContainer = [this, &Inventory, &SessionId](
		const FName ContainerId,
		const int32 Capacity,
		const FString& ItemPrefix)
	{
		for (int32 Index = 0; Index < Capacity; ++Index)
		{
			FAIREInventoryContainerSnapshot Snapshot;
			Inventory->GetContainerSnapshot(ContainerId, Snapshot);
			FAIREInventoryMutationRequest Request;
			Request.SessionId = SessionId;
			Request.MutationId = FGuid::NewGuid();
			Request.ContainerId = ContainerId;
			Request.ExpectedRevision = Snapshot.Revision;
			Request.ItemId = FName(
				*FString::Printf(TEXT("%s.%d"), *ItemPrefix, Index));
			Request.Count = 1;
			if (!TestTrue(
				TEXT("Each fixed-capacity slot accepts one unique item"),
				Inventory->TryAddItem(Request).Code
					== EAIREInventoryMutationCode::Succeeded))
			{
				return false;
			}
		}

		FAIREInventoryContainerSnapshot FullSnapshot;
		Inventory->GetContainerSnapshot(ContainerId, FullSnapshot);
		TestEqual(
			TEXT("All fixed-capacity slots are occupied"),
			FullSnapshot.ItemStacks.Num(),
			Capacity);
		FAIREInventoryMutationRequest OverflowRequest;
		OverflowRequest.SessionId = SessionId;
		OverflowRequest.MutationId = FGuid::NewGuid();
		OverflowRequest.ContainerId = ContainerId;
		OverflowRequest.ExpectedRevision = FullSnapshot.Revision;
		OverflowRequest.ItemId = FName(
			*FString::Printf(TEXT("%s.overflow"), *ItemPrefix));
		OverflowRequest.Count = 1;
		TestTrue(
			TEXT("The next unique stack is rejected atomically"),
			Inventory->TryAddItem(OverflowRequest).Code
				== EAIREInventoryMutationCode::CapacityExceeded);
		FAIREInventoryContainerSnapshot AfterOverflow;
		Inventory->GetContainerSnapshot(ContainerId, AfterOverflow);
		TestEqual(
			TEXT("Rejected overflow preserves revision"),
			AfterOverflow.Revision,
			FullSnapshot.Revision);
		return true;
	};

	TestTrue(
		TEXT("MAKO fixed 20-slot capacity is enforced"),
		FillContainer(
			UAIREGameplayInventorySubsystem::GetMakoContainerId(),
			AIREGameplayInventory::MakoItemSlotCapacity,
			TEXT("AIRE.Test.Unique.Mako")));
	TestTrue(
		TEXT("Storage fixed 50-slot capacity is enforced"),
		FillContainer(
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			AIREGameplayInventory::SharedStorageSlotCapacity,
			TEXT("AIRE.Test.Unique.Storage")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREGameplayInventorySubsystemContractTest,
	"AIRE.Inventory.Subsystem.Contract",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREGameplayInventorySubsystemContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const FName MakoContainerId =
		UAIREGameplayInventorySubsystem::GetMakoContainerId();
	const FName StorageContainerId =
		UAIREGameplayInventorySubsystem::GetSharedStorageContainerId();
	const FName Stack2ItemId(TEXT("AIRE.Test.Stack2"));
	const FName Stack4ItemId(TEXT("AIRE.Test.Stack4"));
	const FName WeaponAItemId(TEXT("AIRE.Test.WeaponA"));
	const FName WeaponBItemId(TEXT("AIRE.Test.WeaponB"));

	TStrongObjectPtr<UGameInstance> TestGameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
		NewObject<UAIREGameplayInventorySubsystem>(TestGameInstance.Get()));
	if (!TestNotNull(TEXT("Inventory subsystem is created"), Inventory.Get()))
	{
		return false;
	}

	FAIREInventorySessionScope Scope;
	Scope.ProfileId = TEXT("profile.test");
	Scope.SaveSlotId = TEXT("save.test");
	Scope.CompanionId = TEXT("companion.test");
	const FGuid SessionId = Inventory->ResetInventorySession(Scope);
	GInventoryAutomationBroadcastCount = 0;

	FAIREInventoryContainerSnapshot MakoSnapshot;
	FAIREInventoryContainerSnapshot StorageSnapshot;
	TestTrue(
		TEXT("MAKO snapshot is available"),
		Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot));
	TestTrue(
		TEXT("Storage snapshot is available"),
		Inventory->GetContainerSnapshot(
			StorageContainerId,
			StorageSnapshot));
	TestEqual(TEXT("MAKO has 20 general slots"), MakoSnapshot.Capacity, 20);
	TestEqual(
		TEXT("Storage has 50 slots"),
		StorageSnapshot.Capacity,
		50);
	TestTrue(
		TEXT("Equipment slot starts independently empty"),
		MakoSnapshot.Equipment.EquippedItemId.IsNone());

	FAIREInventoryMutationRequest AddRequest;
	AddRequest.SessionId = SessionId;
	AddRequest.MutationId = FGuid::NewGuid();
	AddRequest.ContainerId = MakoContainerId;
	AddRequest.ExpectedRevision = MakoSnapshot.Revision;
	AddRequest.ItemId = Stack2ItemId;
	AddRequest.Count = 3;
	const FAIREInventoryMutationResult AddResult =
		Inventory->TryAddItem(AddRequest);
	TestTrue(
		TEXT("Synthetic stack add succeeds"),
		AddResult.Code == EAIREInventoryMutationCode::Succeeded);
	TestEqual(TEXT("Single-container add broadcasts once"), GInventoryAutomationBroadcastCount, 1);

	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	TestEqual(TEXT("Three stack-2 items split across two slots"), MakoSnapshot.ItemStacks.Num(), 2);
	FAIREInventoryContainerSnapshot MutatedCopy = MakoSnapshot;
	MutatedCopy.ItemStacks[0].Count = 99;
	FAIREInventoryContainerSnapshot FreshSnapshot;
	Inventory->GetContainerSnapshot(MakoContainerId, FreshSnapshot);
	TestEqual(
		TEXT("Mutating a snapshot copy does not mutate subsystem state"),
		FreshSnapshot.ItemStacks[0].Count,
		2);

	const int32 BroadcastsBeforeReplay = GInventoryAutomationBroadcastCount;
	const FAIREInventoryMutationResult ReplayResult =
		Inventory->TryAddItem(AddRequest);
	TestTrue(
		TEXT("Successful mutation replay returns AlreadyApplied"),
		ReplayResult.Code == EAIREInventoryMutationCode::AlreadyApplied);
	TestEqual(
		TEXT("Mutation replay does not broadcast"),
		GInventoryAutomationBroadcastCount,
		BroadcastsBeforeReplay);

	FAIREInventoryMutationRequest InvalidRequest = AddRequest;
	InvalidRequest.MutationId = FGuid::NewGuid();
	InvalidRequest.SessionId = FGuid::NewGuid();
	TestTrue(
		TEXT("Foreign session is rejected"),
		Inventory->TryAddItem(InvalidRequest).Code
			== EAIREInventoryMutationCode::InvalidSession);
	InvalidRequest.SessionId = SessionId;
	InvalidRequest.ExpectedRevision = FreshSnapshot.Revision - 1;
	TestTrue(
		TEXT("Stale revision is rejected"),
		Inventory->TryAddItem(InvalidRequest).Code
			== EAIREInventoryMutationCode::RevisionConflict);
	InvalidRequest.ExpectedRevision = FreshSnapshot.Revision;
	InvalidRequest.Count = 0;
	TestTrue(
		TEXT("Zero quantity is rejected"),
		Inventory->TryAddItem(InvalidRequest).Code
			== EAIREInventoryMutationCode::InvalidQuantity);
	InvalidRequest.Count = 1;
	InvalidRequest.ItemId = FName(TEXT("AIRE.Test.Unknown"));
	TestTrue(
		TEXT("Unknown item is rejected"),
		Inventory->TryAddItem(InvalidRequest).Code
			== EAIREInventoryMutationCode::InvalidItem);
	InvalidRequest.ItemId = Stack2ItemId;
	InvalidRequest.MutationId.Invalidate();
	TestTrue(
		TEXT("Invalid mutation GUID is rejected"),
		Inventory->TryAddItem(InvalidRequest).Code
			== EAIREInventoryMutationCode::InvalidMutationId);

	FAIREInventoryMoveRequest InvalidMove;
	InvalidMove.SessionId = SessionId;
	InvalidMove.MutationId = FGuid::NewGuid();
	InvalidMove.ContainerId = MakoContainerId;
	InvalidMove.ExpectedRevision = FreshSnapshot.Revision;
	InvalidMove.SourceSlotIndex = 99;
	InvalidMove.DestinationSlotIndex = 0;
	InvalidMove.Count = 1;
	TestTrue(
		TEXT("Invalid source slot is rejected"),
		Inventory->TryMoveItem(InvalidMove).Code
			== EAIREInventoryMutationCode::InvalidSlot);

	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	FAIREInventoryTransferRequest TransferRequest;
	TransferRequest.SessionId = SessionId;
	TransferRequest.MutationId = FGuid::NewGuid();
	TransferRequest.SourceContainerId = MakoContainerId;
	TransferRequest.DestinationContainerId = StorageContainerId;
	TransferRequest.ExpectedSourceRevision = MakoSnapshot.Revision;
	TransferRequest.ExpectedDestinationRevision = StorageSnapshot.Revision + 1;
	TransferRequest.SourceSlotIndex = MakoSnapshot.ItemStacks[0].SlotIndex;
	TransferRequest.Count = 1;
	const int32 BroadcastsBeforeConflict = GInventoryAutomationBroadcastCount;
	TestTrue(
		TEXT("Transfer destination conflict is rejected"),
		Inventory->TryTransferItem(TransferRequest).Code
			== EAIREInventoryMutationCode::RevisionConflict);
	FAIREInventoryContainerSnapshot MakoAfterConflict;
	FAIREInventoryContainerSnapshot StorageAfterConflict;
	Inventory->GetContainerSnapshot(MakoContainerId, MakoAfterConflict);
	Inventory->GetContainerSnapshot(StorageContainerId, StorageAfterConflict);
	TestEqual(
		TEXT("Rejected transfer preserves source revision"),
		MakoAfterConflict.Revision,
		MakoSnapshot.Revision);
	TestEqual(
		TEXT("Rejected transfer preserves destination revision"),
		StorageAfterConflict.Revision,
		StorageSnapshot.Revision);
	TestEqual(
		TEXT("Rejected transfer does not broadcast"),
		GInventoryAutomationBroadcastCount,
		BroadcastsBeforeConflict);

	TransferRequest.MutationId = FGuid::NewGuid();
	TransferRequest.ExpectedDestinationRevision = StorageSnapshot.Revision;
	const int32 BroadcastsBeforeTransfer = GInventoryAutomationBroadcastCount;
	TestTrue(
		TEXT("MAKO to storage transfer succeeds"),
		Inventory->TryTransferItem(TransferRequest).Code
			== EAIREInventoryMutationCode::Succeeded);
	TestEqual(
		TEXT("Two-container transfer broadcasts each container once"),
		GInventoryAutomationBroadcastCount,
		BroadcastsBeforeTransfer + 2);

	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	FAIREInventoryTransferRequest ReverseTransfer;
	ReverseTransfer.SessionId = SessionId;
	ReverseTransfer.MutationId = FGuid::NewGuid();
	ReverseTransfer.SourceContainerId = StorageContainerId;
	ReverseTransfer.DestinationContainerId = MakoContainerId;
	ReverseTransfer.ExpectedSourceRevision = StorageSnapshot.Revision;
	ReverseTransfer.ExpectedDestinationRevision = MakoSnapshot.Revision;
	ReverseTransfer.SourceSlotIndex = StorageSnapshot.ItemStacks[0].SlotIndex;
	ReverseTransfer.Count = 1;
	TestTrue(
		TEXT("Storage to MAKO transfer succeeds"),
		Inventory->TryTransferItem(ReverseTransfer).Code
			== EAIREInventoryMutationCode::Succeeded);

	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	FAIREInventoryStartupImportCandidate Candidate;
	Candidate.CandidateId = TEXT("candidate.good");
	Candidate.Scope = Scope;
	Candidate.SessionId = SessionId;
	Candidate.ContainerId = StorageContainerId;
	Candidate.BaseRevision = StorageSnapshot.Revision;
	FAIREInventoryImportOperation ImportAdd;
	ImportAdd.OperationId = TEXT("operation.good.add");
	ImportAdd.Type = EAIREInventoryImportOperationType::Add;
	ImportAdd.ItemId = Stack4ItemId;
	ImportAdd.Count = 2;
	Candidate.Operations.Add(ImportAdd);
	TestTrue(
		TEXT("Scoped startup import succeeds"),
		Inventory->TryApplyStartupImportCandidate(Candidate).Code
			== EAIREInventoryMutationCode::Succeeded);
	TestTrue(
		TEXT("Startup candidate replay is idempotent"),
		Inventory->TryApplyStartupImportCandidate(Candidate).Code
			== EAIREInventoryMutationCode::AlreadyApplied);

	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	FAIREInventoryStartupImportCandidate LegacyStorageCandidate = Candidate;
	LegacyStorageCandidate.CandidateId = TEXT("candidate.legacy-storage-id");
	LegacyStorageCandidate.ContainerId = FName(
		AIREGameplayInventory::LegacySharedStorageContainerId);
	LegacyStorageCandidate.BaseRevision = StorageSnapshot.Revision;
	LegacyStorageCandidate.Operations[0].OperationId =
		TEXT("operation.legacy-storage-id.add");
	LegacyStorageCandidate.Operations[0].Count = 1;
	TestTrue(
		TEXT("Legacy shared storage ID is accepted"),
		Inventory->TryApplyStartupImportCandidate(
			LegacyStorageCandidate).Code
			== EAIREInventoryMutationCode::Succeeded);
	FAIREInventoryContainerSnapshot LegacyStorageSnapshot;
	TestTrue(
		TEXT("Legacy shared storage ID resolves to a snapshot"),
		Inventory->GetContainerSnapshot(
			FName(AIREGameplayInventory::LegacySharedStorageContainerId),
			LegacyStorageSnapshot));
	TestEqual(
		TEXT("Legacy shared storage lookup emits the canonical ID"),
		LegacyStorageSnapshot.ContainerId,
		StorageContainerId);

	FAIREInventoryStartupImportCandidate InvalidLegacyStorageCandidate =
		LegacyStorageCandidate;
	InvalidLegacyStorageCandidate.CandidateId =
		TEXT("candidate.invalid-legacy-storage-id");
	InvalidLegacyStorageCandidate.ContainerId =
		FName(TEXT("AIRE.Inventory.SharedWarehouse.Invalid"));
	InvalidLegacyStorageCandidate.BaseRevision =
		LegacyStorageSnapshot.Revision;
	InvalidLegacyStorageCandidate.Operations[0].OperationId =
		TEXT("operation.invalid-legacy-storage-id.add");
	TestTrue(
		TEXT("Near-match legacy storage ID is rejected"),
		Inventory->TryApplyStartupImportCandidate(
			InvalidLegacyStorageCandidate).Code
			== EAIREInventoryMutationCode::InvalidContainer);

	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	FAIREInventoryStartupImportCandidate ImportConflictCandidate = Candidate;
	ImportConflictCandidate.CandidateId = TEXT("candidate.conflict");
	ImportConflictCandidate.BaseRevision = StorageSnapshot.Revision - 1;
	ImportConflictCandidate.Operations[0].OperationId =
		TEXT("operation.conflict.add");
	TestTrue(
		TEXT("Startup import base revision conflict is rejected"),
		Inventory->TryApplyStartupImportCandidate(
			ImportConflictCandidate).Code
			== EAIREInventoryMutationCode::RevisionConflict);

	FAIREInventoryStartupImportCandidate DuplicateOperationCandidate = Candidate;
	DuplicateOperationCandidate.CandidateId = TEXT("candidate.duplicate");
	DuplicateOperationCandidate.BaseRevision = StorageSnapshot.Revision;
	DuplicateOperationCandidate.Operations[0].OperationId =
		TEXT("operation.duplicate");
	DuplicateOperationCandidate.Operations.Add(
		DuplicateOperationCandidate.Operations[0]);
	TestTrue(
		TEXT("Duplicate operation IDs reject the entire import batch"),
		Inventory->TryApplyStartupImportCandidate(
			DuplicateOperationCandidate).Code
			== EAIREInventoryMutationCode::DuplicateOperation);

	FAIREInventoryStartupImportCandidate RollbackCandidate = Candidate;
	RollbackCandidate.CandidateId = TEXT("candidate.rollback");
	RollbackCandidate.BaseRevision = StorageSnapshot.Revision;
	RollbackCandidate.Operations.Reset();
	FAIREInventoryImportOperation RollbackAdd = ImportAdd;
	RollbackAdd.OperationId = TEXT("operation.rollback.add");
	RollbackAdd.Count = 1;
	RollbackCandidate.Operations.Add(RollbackAdd);
	FAIREInventoryImportOperation InvalidRemove;
	InvalidRemove.OperationId = TEXT("operation.rollback.remove");
	InvalidRemove.Type = EAIREInventoryImportOperationType::Remove;
	InvalidRemove.ItemId = Stack2ItemId;
	InvalidRemove.Count = 99;
	RollbackCandidate.Operations.Add(InvalidRemove);
	const int64 StorageRevisionBeforeRollback = StorageSnapshot.Revision;
	TestTrue(
		TEXT("Invalid import batch is rejected"),
		Inventory->TryApplyStartupImportCandidate(RollbackCandidate).Code
			== EAIREInventoryMutationCode::InsufficientQuantity);
	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	TestEqual(
		TEXT("Rejected import batch rolls back revision"),
		StorageSnapshot.Revision,
		StorageRevisionBeforeRollback);
	FAIREInventoryStartupImportCandidate WrongScopeCandidate = RollbackCandidate;
	WrongScopeCandidate.CandidateId = TEXT("candidate.scope");
	WrongScopeCandidate.Scope.ProfileId = TEXT("profile.other");
	TestTrue(
		TEXT("Mismatched startup scope is rejected"),
		Inventory->TryApplyStartupImportCandidate(WrongScopeCandidate).Code
			== EAIREInventoryMutationCode::ScopeMismatch);

	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	FAIREInventoryMutationRequest AddWeapon;
	AddWeapon.SessionId = SessionId;
	AddWeapon.MutationId = FGuid::NewGuid();
	AddWeapon.ContainerId = MakoContainerId;
	AddWeapon.ExpectedRevision = MakoSnapshot.Revision;
	AddWeapon.ItemId = WeaponAItemId;
	AddWeapon.Count = 1;
	TestTrue(
		TEXT("First weapon is added to a general slot"),
		Inventory->TryAddItem(AddWeapon).Code
			== EAIREInventoryMutationCode::Succeeded);
	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	const FAIREInventoryItemStackSnapshot* WeaponAStack =
		MakoSnapshot.ItemStacks.FindByPredicate(
			[WeaponAItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == WeaponAItemId;
			});
	if (!TestNotNull(TEXT("Weapon A stack exists"), WeaponAStack))
	{
		return false;
	}

	FAIREInventoryEquipRequest EquipA;
	EquipA.SessionId = SessionId;
	EquipA.MutationId = FGuid::NewGuid();
	EquipA.ExpectedRevision = MakoSnapshot.Revision;
	EquipA.SourceSlotIndex = WeaponAStack->SlotIndex;
	TestTrue(
		TEXT("Weapon slot reservation succeeds"),
		Inventory->ReserveMakoEquipmentSwap(EquipA).Code
			== EAIREInventoryMutationCode::Succeeded);
	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	TestTrue(
		TEXT("Reserved weapon remains in the general slot"),
		MakoSnapshot.ItemStacks.ContainsByPredicate(
			[WeaponAItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == WeaponAItemId && Stack.Count == 1;
			}));
	TestTrue(
		TEXT("Reservation exposes pending equipment state"),
		MakoSnapshot.Equipment.PendingItemId == WeaponAItemId
			&& MakoSnapshot.Equipment.TransitionState
				== EAIREEquipmentTransitionState::Equipping);
	TestTrue(
		TEXT("Committing equipment moves the weapon to the equipment slot"),
		Inventory->CommitMakoEquipmentSwap(SessionId, EquipA.MutationId).Code
			== EAIREInventoryMutationCode::Succeeded);

	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	AddWeapon.MutationId = FGuid::NewGuid();
	AddWeapon.ExpectedRevision = MakoSnapshot.Revision;
	AddWeapon.ItemId = WeaponBItemId;
	TestTrue(
		TEXT("Second weapon is added for recovery testing"),
		Inventory->TryAddItem(AddWeapon).Code
			== EAIREInventoryMutationCode::Succeeded);
	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	const FAIREInventoryItemStackSnapshot* WeaponBStack =
		MakoSnapshot.ItemStacks.FindByPredicate(
			[WeaponBItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == WeaponBItemId;
			});
	if (!TestNotNull(TEXT("Weapon B stack exists"), WeaponBStack))
	{
		return false;
	}

	FAIREInventoryEquipRequest EquipB;
	EquipB.SessionId = SessionId;
	EquipB.MutationId = FGuid::NewGuid();
	EquipB.ExpectedRevision = MakoSnapshot.Revision;
	EquipB.SourceSlotIndex = WeaponBStack->SlotIndex;
	TestTrue(
		TEXT("Second weapon reservation succeeds"),
		Inventory->ReserveMakoEquipmentSwap(EquipB).Code
			== EAIREInventoryMutationCode::Succeeded);
	TestTrue(
		TEXT("Failed async equip begins recovery"),
		Inventory->BeginMakoEquipmentRecovery(
			SessionId,
			EquipB.MutationId).Code
			== EAIREInventoryMutationCode::Succeeded);
	TestTrue(
		TEXT("Failed runtime recovery is surfaced"),
		Inventory->CompleteMakoEquipmentRecovery(
			SessionId,
			EquipB.MutationId,
			false).Code == EAIREInventoryMutationCode::RecoveryFailed);
	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	TestTrue(
		TEXT("Recovery failure preserves last valid equipped ownership"),
		MakoSnapshot.Equipment.EquippedItemId == WeaponAItemId);
	TestTrue(
		TEXT("Recovery failure preserves pending weapon quantity"),
		MakoSnapshot.ItemStacks.ContainsByPredicate(
			[WeaponBItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == WeaponBItemId && Stack.Count == 1;
			}));
	TestTrue(
		TEXT("Recovery failure is visible in the snapshot"),
		MakoSnapshot.Equipment.TransitionState
			== EAIREEquipmentTransitionState::RecoveryFailed);

	const FGuid OldSessionId = SessionId;
	const FGuid OldEquipmentMutationId = EquipB.MutationId;
	const FGuid NewSessionId = Inventory->ResetInventorySession(Scope);
	TestTrue(TEXT("Session reset creates a new GUID"), NewSessionId != OldSessionId);
	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	TestEqual(TEXT("Session reset clears MAKO revision"), MakoSnapshot.Revision, int64(0));
	TestTrue(TEXT("Session reset clears MAKO stacks"), MakoSnapshot.ItemStacks.IsEmpty());
	TestTrue(
		TEXT("Late equipment callback cannot mutate a reset session"),
		Inventory->CommitMakoEquipmentSwap(
			OldSessionId,
			OldEquipmentMutationId).Code
			== EAIREInventoryMutationCode::InvalidSession);

	TStrongObjectPtr<UAI_REPlayerInventoryComponent> PlayerInventory(
		NewObject<UAI_REPlayerInventoryComponent>());
	if (!TestNotNull(TEXT("Player inventory is created"), PlayerInventory.Get()))
	{
		return false;
	}
	PlayerInventory->MaxSlots = 1;
	FInventoryItemStack& PlayerStack = PlayerInventory->Items.AddDefaulted_GetRef();
	PlayerStack.SlotIndex = 0;
	PlayerStack.ItemId = Stack4ItemId;
	PlayerStack.Count = 2;
	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	FAIREPlayerStorageTransferRequest PlayerDeposit;
	PlayerDeposit.SessionId = NewSessionId;
	PlayerDeposit.MutationId = FGuid::NewGuid();
	PlayerDeposit.Direction =
		EAIREPlayerStorageTransferDirection::DepositPlayerToStorage;
	PlayerDeposit.ExpectedStorageRevision = StorageSnapshot.Revision;
	PlayerDeposit.SourceSlotIndex = 0;
	PlayerDeposit.Count = 1;
	TestTrue(
		TEXT("Player to storage exact transfer succeeds"),
		Inventory->TryTransferPlayerStorage(
			PlayerInventory.Get(),
			PlayerDeposit).Code == EAIREInventoryMutationCode::Succeeded);
	TestEqual(TEXT("Player deposit removes exactly one"), PlayerInventory->Items[0].Count, 1);
	FAIREInventoryContainerSnapshot StorageAfterDeposit;
	Inventory->GetContainerSnapshot(
		StorageContainerId,
		StorageAfterDeposit);
	TestTrue(
		TEXT("Player transfer replay is idempotent"),
		Inventory->TryTransferPlayerStorage(
			PlayerInventory.Get(),
			PlayerDeposit).Code == EAIREInventoryMutationCode::AlreadyApplied);
	TestEqual(
		TEXT("Player transfer replay preserves player quantity"),
		PlayerInventory->Items[0].Count,
		1);
	FAIREInventoryContainerSnapshot StorageAfterDepositReplay;
	Inventory->GetContainerSnapshot(
		StorageContainerId,
		StorageAfterDepositReplay);
	TestEqual(
		TEXT("Player transfer replay preserves storage revision"),
		StorageAfterDepositReplay.Revision,
		StorageAfterDeposit.Revision);

	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	FAIREPlayerStorageTransferRequest PlayerWithdraw;
	PlayerWithdraw.SessionId = NewSessionId;
	PlayerWithdraw.MutationId = FGuid::NewGuid();
	PlayerWithdraw.Direction =
		EAIREPlayerStorageTransferDirection::WithdrawStorageToPlayer;
	PlayerWithdraw.ExpectedStorageRevision = StorageSnapshot.Revision;
	PlayerWithdraw.SourceSlotIndex = StorageSnapshot.ItemStacks[0].SlotIndex;
	PlayerWithdraw.Count = 1;
	TestTrue(
		TEXT("Storage to player exact transfer succeeds"),
		Inventory->TryTransferPlayerStorage(
			PlayerInventory.Get(),
			PlayerWithdraw).Code == EAIREInventoryMutationCode::Succeeded);
	TestEqual(TEXT("Player withdrawal restores exact count"), PlayerInventory->Items[0].Count, 2);

	FAIREInventoryMutationRequest StorageAdd;
	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	StorageAdd.SessionId = NewSessionId;
	StorageAdd.MutationId = FGuid::NewGuid();
	StorageAdd.ContainerId = StorageContainerId;
	StorageAdd.ExpectedRevision = StorageSnapshot.Revision;
	StorageAdd.ItemId = Stack2ItemId;
	StorageAdd.Count = 1;
	TestTrue(
		TEXT("Storage source is prepared for atomic rejection"),
		Inventory->TryAddItem(StorageAdd).Code
			== EAIREInventoryMutationCode::Succeeded);
	PlayerInventory->Items[0].Count = 4;
	Inventory->GetContainerSnapshot(StorageContainerId, StorageSnapshot);
	const FAIREInventoryItemStackSnapshot* StorageStack2 =
		StorageSnapshot.ItemStacks.FindByPredicate(
			[Stack2ItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == Stack2ItemId;
			});
	if (!TestNotNull(TEXT("Storage stack-2 item exists"), StorageStack2))
	{
		return false;
	}
	PlayerWithdraw.MutationId = FGuid::NewGuid();
	PlayerWithdraw.ExpectedStorageRevision = StorageSnapshot.Revision;
	PlayerWithdraw.SourceSlotIndex = StorageStack2->SlotIndex;
	TestTrue(
		TEXT("Full player inventory rejects the entire withdrawal"),
		Inventory->TryTransferPlayerStorage(
			PlayerInventory.Get(),
			PlayerWithdraw).Code
			== EAIREInventoryMutationCode::CapacityExceeded);
	FAIREInventoryContainerSnapshot StorageAfterRejectedWithdraw;
	Inventory->GetContainerSnapshot(
		StorageContainerId,
		StorageAfterRejectedWithdraw);
	TestEqual(
		TEXT("Rejected player withdrawal preserves storage revision"),
		StorageAfterRejectedWithdraw.Revision,
		StorageSnapshot.Revision);
	TestEqual(
		TEXT("Rejected player withdrawal preserves player quantity"),
		PlayerInventory->Items[0].Count,
		4);

	const FGuid DirectTransferSessionId =
		Inventory->ResetInventorySession(Scope);
	PlayerInventory->Items[0].Count = 4;
	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	FAIREPlayerMakoTransferRequest PlayerToMako;
	PlayerToMako.SessionId = DirectTransferSessionId;
	PlayerToMako.MutationId = FGuid::NewGuid();
	PlayerToMako.Direction =
		EAIREPlayerMakoTransferDirection::PlayerToMako;
	PlayerToMako.ExpectedPlayerRevision =
		PlayerInventory->GetInventoryRevision();
	PlayerToMako.ExpectedMakoRevision = MakoSnapshot.Revision;
	PlayerToMako.SourceSlotIndex = 0;
	PlayerToMako.Count = 4;
	const FAIREInventoryMutationResult PlayerToMakoResult =
		Inventory->TryTransferPlayerMako(
			PlayerInventory.Get(),
			PlayerToMako);
	TestTrue(
		TEXT("Player to MAKO direct transfer succeeds atomically"),
		PlayerToMakoResult.Code
			== EAIREInventoryMutationCode::Succeeded);
	TestTrue(
		TEXT("Player source slot is emptied after direct transfer"),
		PlayerInventory->Items.IsEmpty());
	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	TestTrue(
		TEXT("MAKO receives the complete direct-transfer stack"),
		MakoSnapshot.ItemStacks.ContainsByPredicate(
			[Stack4ItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == Stack4ItemId && Stack.Count == 4;
			}));
	const int64 MakoRevisionAfterDirectTransfer = MakoSnapshot.Revision;
	TestTrue(
		TEXT("Direct transfer replay is idempotent"),
		Inventory->TryTransferPlayerMako(
			PlayerInventory.Get(),
			PlayerToMako).Code
			== EAIREInventoryMutationCode::AlreadyApplied);
	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	TestEqual(
		TEXT("Direct transfer replay preserves MAKO revision"),
		MakoSnapshot.Revision,
		MakoRevisionAfterDirectTransfer);

	FInventoryItemStack& FullPlayerStack =
		PlayerInventory->Items.AddDefaulted_GetRef();
	FullPlayerStack.SlotIndex = 0;
	FullPlayerStack.ItemId = Stack4ItemId;
	FullPlayerStack.Count = 4;
	FAIREPlayerMakoTransferRequest MakoToFullPlayer;
	MakoToFullPlayer.SessionId = DirectTransferSessionId;
	MakoToFullPlayer.MutationId = FGuid::NewGuid();
	MakoToFullPlayer.Direction =
		EAIREPlayerMakoTransferDirection::MakoToPlayer;
	MakoToFullPlayer.ExpectedPlayerRevision =
		PlayerInventory->GetInventoryRevision();
	MakoToFullPlayer.ExpectedMakoRevision = MakoSnapshot.Revision;
	MakoToFullPlayer.SourceSlotIndex =
		MakoSnapshot.ItemStacks[0].SlotIndex;
	MakoToFullPlayer.Count = 1;
	TestTrue(
		TEXT("Full player inventory rejects MAKO transfer atomically"),
		Inventory->TryTransferPlayerMako(
			PlayerInventory.Get(),
			MakoToFullPlayer).Code
			== EAIREInventoryMutationCode::CapacityExceeded);
	FAIREInventoryContainerSnapshot MakoAfterRejectedDirectTransfer;
	Inventory->GetContainerSnapshot(
		MakoContainerId,
		MakoAfterRejectedDirectTransfer);
	TestEqual(
		TEXT("Rejected direct transfer preserves MAKO revision"),
		MakoAfterRejectedDirectTransfer.Revision,
		MakoSnapshot.Revision);
	TestEqual(
		TEXT("Rejected direct transfer preserves player quantity"),
		PlayerInventory->Items[0].Count,
		4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREGameplayInventoryMakoCraftWorkTest,
	"AIRE.Inventory.Subsystem.MakoCraftWork",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREGameplayInventoryMakoCraftWorkTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const FName MakoContainerId =
		UAIREGameplayInventorySubsystem::GetMakoContainerId();
	const FName StorageContainerId =
		UAIREGameplayInventorySubsystem::GetSharedStorageContainerId();
	const FName IngredientItemId(TEXT("AIRE.Test.Stack4"));
	const FName ResultItemId(TEXT("AIRE.Test.Stack2"));

	TStrongObjectPtr<UGameInstance> TestGameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
		NewObject<UAIREGameplayInventorySubsystem>(TestGameInstance.Get()));
	const FGuid SessionId = Inventory->ResetInventorySession();

	auto GetSnapshots = [&Inventory, &MakoContainerId, &StorageContainerId](
		FAIREInventoryContainerSnapshot& OutMako,
		FAIREInventoryContainerSnapshot& OutStorage)
	{
		return Inventory->GetContainerSnapshot(MakoContainerId, OutMako)
			&& Inventory->GetContainerSnapshot(
				StorageContainerId,
				OutStorage);
	};
	auto CountItem = [](const TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const FName ItemId)
	{
		int32 TotalCount = 0;
		for (const FAIREInventoryItemStackSnapshot& Stack : Stacks)
		{
			if (Stack.ItemId == ItemId)
			{
				TotalCount += Stack.Count;
			}
		}
		return TotalCount;
	};
	auto AddItem = [this, &Inventory, &SessionId](
		const FName ContainerId,
		const FName ItemId,
		const int32 Count)
	{
		FAIREInventoryContainerSnapshot Snapshot;
		if (!Inventory->GetContainerSnapshot(ContainerId, Snapshot))
		{
			return false;
		}
		FAIREInventoryMutationRequest AddRequest;
		AddRequest.SessionId = SessionId;
		AddRequest.MutationId = FGuid::NewGuid();
		AddRequest.ContainerId = ContainerId;
		AddRequest.ExpectedRevision = Snapshot.Revision;
		AddRequest.ItemId = ItemId;
		AddRequest.Count = Count;
		return TestTrue(
			TEXT("Craft work test item is added"),
			Inventory->TryAddItem(AddRequest).Code
				== EAIREInventoryMutationCode::Succeeded);
	};

	TestTrue(
		TEXT("Craft ingredient is prepared"),
		AddItem(MakoContainerId, IngredientItemId, 4));
	FAIREInventoryContainerSnapshot MakoBefore;
	FAIREInventoryContainerSnapshot StorageBefore;
	if (!TestTrue(
			TEXT("Craft work snapshots are available"),
			GetSnapshots(MakoBefore, StorageBefore)))
	{
		return false;
	}

	FAIREMakoCraftWorkRequest CraftRequest;
	CraftRequest.SessionId = SessionId;
	CraftRequest.WorkOrderId = FGuid::NewGuid();
	CraftRequest.ExpectedMakoRevision = MakoBefore.Revision;
	CraftRequest.ExpectedStorageRevision = StorageBefore.Revision;
	FAIREInventoryItemQuantity& FirstIngredient =
		CraftRequest.Ingredients.AddDefaulted_GetRef();
	FirstIngredient.ItemId = IngredientItemId;
	FirstIngredient.Count = 1;
	FAIREInventoryItemQuantity& DuplicateIngredient =
		CraftRequest.Ingredients.AddDefaulted_GetRef();
	DuplicateIngredient.ItemId = IngredientItemId;
	DuplicateIngredient.Count = 2;
	CraftRequest.Result.ItemId = ResultItemId;
	CraftRequest.Result.Count = 1;
	FAIREInventoryWorkResult PreflightResult;
	TestTrue(
		TEXT("Aggregated craft ingredients pass preflight"),
		Inventory->CanCompleteMakoCraftWork(
			CraftRequest,
			PreflightResult));
	TestTrue(
		TEXT("Craft result prefers MAKO inventory"),
		PreflightResult.Destination
			== EAIREInventoryWorkResultDestination::Mako);
	const FAIREInventoryWorkResult CompletionResult =
		Inventory->TryCompleteMakoCraftWork(CraftRequest);
	TestTrue(
		TEXT("Craft completion succeeds"),
		CompletionResult.Code == EAIREInventoryMutationCode::Succeeded);

	FAIREInventoryContainerSnapshot MakoAfter;
	FAIREInventoryContainerSnapshot StorageAfter;
	GetSnapshots(MakoAfter, StorageAfter);
	TestEqual(
		TEXT("Craft consumes the aggregated ingredient quantity"),
		CountItem(MakoAfter.ItemStacks, IngredientItemId),
		1);
	TestEqual(
		TEXT("Craft stores the result exactly once"),
		CountItem(MakoAfter.ItemStacks, ResultItemId),
		1);
	TestEqual(
		TEXT("Craft changes MAKO revision once"),
		MakoAfter.Revision,
		MakoBefore.Revision + 1);
	TestEqual(
		TEXT("MAKO craft does not change storage revision"),
		StorageAfter.Revision,
		StorageBefore.Revision);

	const FAIREInventoryWorkResult ReplayResult =
		Inventory->TryCompleteMakoCraftWork(CraftRequest);
	TestTrue(
		TEXT("Craft replay is idempotent"),
		ReplayResult.Code == EAIREInventoryMutationCode::AlreadyApplied
			&& ReplayResult.bAlreadyApplied);
	FAIREInventoryContainerSnapshot MakoAfterReplay;
	FAIREInventoryContainerSnapshot StorageAfterReplay;
	GetSnapshots(MakoAfterReplay, StorageAfterReplay);
	TestEqual(
		TEXT("Craft replay preserves MAKO revision"),
		MakoAfterReplay.Revision,
		MakoAfter.Revision);
	TestEqual(
		TEXT("Craft replay preserves storage revision"),
		StorageAfterReplay.Revision,
		StorageAfter.Revision);

	FAIREMakoCraftWorkRequest StaleRequest = CraftRequest;
	StaleRequest.WorkOrderId = FGuid::NewGuid();
	TestTrue(
		TEXT("Stale craft revision is rejected"),
		Inventory->TryCompleteMakoCraftWork(StaleRequest).Code
			== EAIREInventoryMutationCode::RevisionConflict);
	FAIREMakoCraftWorkRequest InsufficientRequest = CraftRequest;
	InsufficientRequest.WorkOrderId = FGuid::NewGuid();
	InsufficientRequest.ExpectedMakoRevision = MakoAfter.Revision;
	InsufficientRequest.ExpectedStorageRevision = StorageAfter.Revision;
	InsufficientRequest.Ingredients.Reset();
	FAIREInventoryItemQuantity& InsufficientIngredient =
		InsufficientRequest.Ingredients.AddDefaulted_GetRef();
	InsufficientIngredient.ItemId = IngredientItemId;
	InsufficientIngredient.Count = 2;
	TestTrue(
		TEXT("Insufficient craft ingredients are rejected"),
		Inventory->TryCompleteMakoCraftWork(InsufficientRequest).Code
			== EAIREInventoryMutationCode::InsufficientQuantity);
	FAIREInventoryContainerSnapshot MakoAfterRejectedRequests;
	FAIREInventoryContainerSnapshot StorageAfterRejectedRequests;
	GetSnapshots(MakoAfterRejectedRequests, StorageAfterRejectedRequests);
	TestEqual(
		TEXT("Rejected craft requests preserve MAKO revision"),
		MakoAfterRejectedRequests.Revision,
		MakoAfter.Revision);
	TestEqual(
		TEXT("Rejected craft requests preserve storage revision"),
		StorageAfterRejectedRequests.Revision,
		StorageAfter.Revision);
	FAIREMakoWorkRewardRequest RewardRequest;
	RewardRequest.SessionId = SessionId;
	RewardRequest.DeliveryId = FGuid::NewGuid();
	RewardRequest.ExpectedMakoRevision =
		MakoAfterRejectedRequests.Revision;
	RewardRequest.ExpectedStorageRevision =
		StorageAfterRejectedRequests.Revision;
	RewardRequest.Reward.ItemId = ResultItemId;
	RewardRequest.Reward.Count = 1;
	const FAIREInventoryWorkResult RewardResult =
		Inventory->TryStoreMakoWorkReward(RewardRequest);
	TestTrue(
		TEXT("Harvest reward prefers MAKO inventory"),
		RewardResult.Code == EAIREInventoryMutationCode::Succeeded
			&& RewardResult.Destination
				== EAIREInventoryWorkResultDestination::Mako);
	TestTrue(
		TEXT("Harvest reward replay is idempotent"),
		Inventory->TryStoreMakoWorkReward(RewardRequest).Code
			== EAIREInventoryMutationCode::AlreadyApplied);

	const FName WeaponItemId(TEXT("AIRE.Test.WeaponA"));
	TestTrue(
		TEXT("Equipment transition test weapon is prepared"),
		AddItem(MakoContainerId, WeaponItemId, 1));
	FAIREInventoryContainerSnapshot MakoBeforeEquipment;
	Inventory->GetContainerSnapshot(MakoContainerId, MakoBeforeEquipment);
	const FAIREInventoryItemStackSnapshot* WeaponStack =
		MakoBeforeEquipment.ItemStacks.FindByPredicate(
			[WeaponItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == WeaponItemId;
			});
	if (!TestNotNull(
			TEXT("Equipment transition test weapon stack exists"),
			WeaponStack))
	{
		return false;
	}
	FAIREInventoryEquipRequest EquipRequest;
	EquipRequest.SessionId = SessionId;
	EquipRequest.MutationId = FGuid::NewGuid();
	EquipRequest.ExpectedRevision = MakoBeforeEquipment.Revision;
	EquipRequest.SourceSlotIndex = WeaponStack->SlotIndex;
	TestTrue(
		TEXT("Equipment transition is reserved"),
		Inventory->ReserveMakoEquipmentSwap(EquipRequest).Code
			== EAIREInventoryMutationCode::Succeeded);
	FAIREInventoryContainerSnapshot MakoDuringEquipment;
	FAIREInventoryContainerSnapshot StorageDuringEquipment;
	GetSnapshots(MakoDuringEquipment, StorageDuringEquipment);
	FAIREMakoCraftWorkRequest EquipmentBusyRequest = CraftRequest;
	EquipmentBusyRequest.WorkOrderId = FGuid::NewGuid();
	EquipmentBusyRequest.ExpectedMakoRevision = MakoDuringEquipment.Revision;
	EquipmentBusyRequest.ExpectedStorageRevision =
		StorageDuringEquipment.Revision;
	EquipmentBusyRequest.Ingredients[0].Count = 1;
	EquipmentBusyRequest.Ingredients.SetNum(1);
	TestTrue(
		TEXT("Active equipment transition rejects craft mutation"),
		Inventory->TryCompleteMakoCraftWork(EquipmentBusyRequest).Code
			== EAIREInventoryMutationCode::EquipmentBusy);

	TStrongObjectPtr<UGameInstance> FullGameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> FullInventory(
		NewObject<UAIREGameplayInventorySubsystem>(FullGameInstance.Get()));
	const FGuid FullSessionId = FullInventory->ResetInventorySession();
	FAIREInventoryContainerSnapshot InitialFullMako;
	FullInventory->GetContainerSnapshot(MakoContainerId, InitialFullMako);
	FAIREInventoryMutationRequest FullIngredientAdd;
	FullIngredientAdd.SessionId = FullSessionId;
	FullIngredientAdd.MutationId = FGuid::NewGuid();
	FullIngredientAdd.ContainerId = MakoContainerId;
	FullIngredientAdd.ExpectedRevision = InitialFullMako.Revision;
	FullIngredientAdd.ItemId = IngredientItemId;
	FullIngredientAdd.Count = 2;
	TestTrue(
		TEXT("World-drop route ingredient stack is prepared"),
		FullInventory->TryAddItem(FullIngredientAdd).Code
			== EAIREInventoryMutationCode::Succeeded);
	auto FillContainer = [this, &FullInventory, &FullSessionId](
		const FName ContainerId,
		const int32 Capacity,
		const FString& ItemPrefix)
	{
		for (int32 Index = 0; Index < Capacity; ++Index)
		{
			FAIREInventoryContainerSnapshot Snapshot;
			FullInventory->GetContainerSnapshot(ContainerId, Snapshot);
			FAIREInventoryMutationRequest AddRequest;
			AddRequest.SessionId = FullSessionId;
			AddRequest.MutationId = FGuid::NewGuid();
			AddRequest.ContainerId = ContainerId;
			AddRequest.ExpectedRevision = Snapshot.Revision;
			AddRequest.ItemId = FName(*FString::Printf(
				TEXT("%s.%d"),
				*ItemPrefix,
				Index));
			AddRequest.Count = 1;
			if (!TestTrue(
					TEXT("World-drop route container slot is filled"),
					FullInventory->TryAddItem(AddRequest).Code
						== EAIREInventoryMutationCode::Succeeded))
			{
				return false;
			}
		}
		return true;
	};
	TestTrue(
		TEXT("Full MAKO inventory is prepared"),
		FillContainer(
			MakoContainerId,
			AIREGameplayInventory::MakoItemSlotCapacity - 1,
			TEXT("AIRE.Test.Unique.FullMako")));
	TestTrue(
		TEXT("Full storage is prepared"),
		FillContainer(
			StorageContainerId,
			AIREGameplayInventory::SharedStorageSlotCapacity,
			TEXT("AIRE.Test.Unique.FullStorage")));
	FAIREInventoryContainerSnapshot FullMako;
	FAIREInventoryContainerSnapshot FullStorage;
	FullInventory->GetContainerSnapshot(MakoContainerId, FullMako);
	FullInventory->GetContainerSnapshot(StorageContainerId, FullStorage);
	FAIREMakoCraftWorkRequest WorldDropRequest;
	WorldDropRequest.SessionId = FullSessionId;
	WorldDropRequest.WorkOrderId = FGuid::NewGuid();
	WorldDropRequest.ExpectedMakoRevision = FullMako.Revision;
	WorldDropRequest.ExpectedStorageRevision = FullStorage.Revision;
	FAIREInventoryItemQuantity& WorldDropIngredient =
		WorldDropRequest.Ingredients.AddDefaulted_GetRef();
	WorldDropIngredient.ItemId = IngredientItemId;
	WorldDropIngredient.Count = 1;
	WorldDropRequest.Result.ItemId =
		FName(TEXT("AIRE.Test.Unique.CraftResult"));
	WorldDropRequest.Result.Count = 1;
	WorldDropRequest.bCanWorldDrop = true;
	const FAIREInventoryWorkResult WorldDropResult =
		FullInventory->TryCompleteMakoCraftWork(WorldDropRequest);
	TestTrue(
		TEXT("Full inventories route craft result to world drop"),
		WorldDropResult.Code == EAIREInventoryMutationCode::Succeeded
			&& WorldDropResult.Destination
				== EAIREInventoryWorkResultDestination::WorldDrop);
	TestTrue(
		TEXT("World-drop craft replay is idempotent"),
		FullInventory->TryCompleteMakoCraftWork(WorldDropRequest).Code
			== EAIREInventoryMutationCode::AlreadyApplied);

	FAIREInventoryContainerSnapshot FullMakoAfterCraft;
	FAIREInventoryContainerSnapshot FullStorageAfterCraft;
	FullInventory->GetContainerSnapshot(
		MakoContainerId,
		FullMakoAfterCraft);
	FullInventory->GetContainerSnapshot(
		StorageContainerId,
		FullStorageAfterCraft);
	FAIREMakoWorkRewardRequest RetriableHarvestReward;
	RetriableHarvestReward.SessionId = FullSessionId;
	RetriableHarvestReward.DeliveryId = FGuid::NewGuid();
	RetriableHarvestReward.ExpectedMakoRevision =
		FullMakoAfterCraft.Revision;
	RetriableHarvestReward.ExpectedStorageRevision =
		FullStorageAfterCraft.Revision;
	RetriableHarvestReward.Reward.ItemId =
		FName(TEXT("AIRE.Test.Unique.HarvestReward"));
	RetriableHarvestReward.Reward.Count = 1;
	const FAIREInventoryWorkResult InitialHarvestWorldDrop =
		FullInventory->TryStoreMakoWorkReward(RetriableHarvestReward);
	TestTrue(
		TEXT("Full inventories leave harvest reward in the world"),
		InitialHarvestWorldDrop.Code
				== EAIREInventoryMutationCode::Succeeded
			&& InitialHarvestWorldDrop.Destination
				== EAIREInventoryWorkResultDestination::WorldDrop
			&& !InitialHarvestWorldDrop.bAlreadyApplied);
	const FAIREInventoryWorkResult RepeatedHarvestWorldDrop =
		FullInventory->TryStoreMakoWorkReward(RetriableHarvestReward);
	TestTrue(
		TEXT("World harvest reward remains retriable while inventories are full"),
		RepeatedHarvestWorldDrop.Code
				== EAIREInventoryMutationCode::Succeeded
			&& RepeatedHarvestWorldDrop.Destination
				== EAIREInventoryWorkResultDestination::WorldDrop
			&& !RepeatedHarvestWorldDrop.bAlreadyApplied);

	const FAIREInventoryItemStackSnapshot* RemovableFullStack =
		FullMakoAfterCraft.ItemStacks.FindByPredicate(
			[](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId.ToString().StartsWith(
					TEXT("AIRE.Test.Unique.FullMako."));
			});
	if (!TestNotNull(
			TEXT("A full MAKO stack can be removed for harvest retry"),
			RemovableFullStack))
	{
		return false;
	}
	FAIREInventoryMutationRequest FreeMakoSlotRequest;
	FreeMakoSlotRequest.SessionId = FullSessionId;
	FreeMakoSlotRequest.MutationId = FGuid::NewGuid();
	FreeMakoSlotRequest.ContainerId = MakoContainerId;
	FreeMakoSlotRequest.ExpectedRevision = FullMakoAfterCraft.Revision;
	FreeMakoSlotRequest.ItemId = RemovableFullStack->ItemId;
	FreeMakoSlotRequest.Count = RemovableFullStack->Count;
	TestTrue(
		TEXT("A MAKO slot is freed for harvest retry"),
		FullInventory->TryRemoveItem(FreeMakoSlotRequest).Code
			== EAIREInventoryMutationCode::Succeeded);
	FAIREInventoryContainerSnapshot MakoAfterSlotFreed;
	FAIREInventoryContainerSnapshot StorageAfterSlotFreed;
	FullInventory->GetContainerSnapshot(
		MakoContainerId,
		MakoAfterSlotFreed);
	FullInventory->GetContainerSnapshot(
		StorageContainerId,
		StorageAfterSlotFreed);
	RetriableHarvestReward.ExpectedMakoRevision =
		MakoAfterSlotFreed.Revision;
	RetriableHarvestReward.ExpectedStorageRevision =
		StorageAfterSlotFreed.Revision;
	const FAIREInventoryWorkResult CollectedHarvestReward =
		FullInventory->TryStoreMakoWorkReward(RetriableHarvestReward);
	TestTrue(
		TEXT("The same world harvest reward is collected after space is available"),
		CollectedHarvestReward.Code
				== EAIREInventoryMutationCode::Succeeded
			&& CollectedHarvestReward.Destination
				== EAIREInventoryWorkResultDestination::Mako);
	TestTrue(
		TEXT("Collected world harvest reward replay is idempotent"),
		FullInventory->TryStoreMakoWorkReward(RetriableHarvestReward).Code
			== EAIREInventoryMutationCode::AlreadyApplied);
	return true;
}

#endif

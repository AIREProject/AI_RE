#include "AIREGameplayInventorySubsystem.h"

#include "AI_REItemDataAsset.h"
#include "AI_REItemSubsystem.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Engine/GameInstance.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"
#include "Subsystems/SubsystemCollection.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#endif

namespace
{
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
	InventorySessionId = FGuid::NewGuid();
	CreateEmptyContainers();
}

void UAIREGameplayInventorySubsystem::Deinitialize()
{
	OnContainerChanged.Clear();
	Containers.Reset();
	AppliedMutations.Reset();
	AppliedWorkResults.Reset();
	AppliedImportCandidateIds.Reset();
	AppliedImportOperationIds.Reset();
	SessionScope = FAIREInventorySessionScope();
	InventorySessionId.Invalidate();
	bMakoInventoryInitialized = false;
	Super::Deinitialize();
}

FGuid UAIREGameplayInventorySubsystem::GetInventorySessionId() const
{
	return InventorySessionId;
}

FName UAIREGameplayInventorySubsystem::GetMakoContainerId()
{
	return FName(AIREGameplayInventory::MakoContainerId);
}

FName UAIREGameplayInventorySubsystem::GetSharedWarehouseContainerId()
{
	return FName(AIREGameplayInventory::SharedWarehouseContainerId);
}

bool UAIREGameplayInventorySubsystem::GetContainerSnapshot(
	const FName ContainerId,
	FAIREInventoryContainerSnapshot& OutSnapshot) const
{
	const FAIREContainerState* Container = FindContainer(ContainerId);
	if (!Container || !InventorySessionId.IsValid())
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
	const bool bRequireCompanionItem =
		Container->ContainerId == GetMakoContainerId();
	if (!ResolveItemRules(Request.ItemId, bRequireCompanionItem, Rules))
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
	RecordAppliedMutation(Result);
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
	const bool bRequireCompanionItem =
		Container->ContainerId == GetMakoContainerId();
	if (!ResolveItemRules(Request.ItemId, bRequireCompanionItem, Rules))
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
	RecordAppliedMutation(Result);
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
	const bool bRequireCompanionItem =
		Container->ContainerId == GetMakoContainerId();
	if (!ResolveItemRules(ItemId, bRequireCompanionItem, Rules))
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
	RecordAppliedMutation(Result);
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
	const bool bRequireCompanionItem =
		Destination->ContainerId == GetMakoContainerId();
	if (!ResolveItemRules(ItemId, bRequireCompanionItem, Rules))
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
	RecordAppliedMutation(Result);
	BroadcastContainerChanged(*Source);
	BroadcastContainerChanged(*Destination);
	return Result;
}

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::TryTransferPlayerWarehouse(
	UAI_REPlayerInventoryComponent* PlayerInventory,
	const FAIREPlayerWarehouseTransferRequest& Request)
{
	FAIREInventoryMutationResult PreviousResult;
	if (FindAppliedMutation(Request.MutationId, PreviousResult))
	{
		PreviousResult.Code = EAIREInventoryMutationCode::AlreadyApplied;
		return PreviousResult;
	}

	FAIREContainerState* Warehouse = FindContainer(
		GetSharedWarehouseContainerId());
	FAIREInventoryMutationResult Validation = ValidateMutation(
		Request.SessionId,
		Request.MutationId,
		Warehouse,
		Request.ExpectedWarehouseRevision);
	if (Validation.Code != EAIREInventoryMutationCode::Succeeded)
	{
		return Validation;
	}
	if (!IsValid(PlayerInventory))
	{
		return MakeResult(
			EAIREInventoryMutationCode::NotInitialized,
			Request.MutationId,
			Warehouse->Revision);
	}
	if (Request.Count <= 0)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidQuantity,
			Request.MutationId,
			Warehouse->Revision);
	}
	if (Request.Direction
			!= EAIREPlayerWarehouseTransferDirection::DepositPlayerToWarehouse
		&& Request.Direction
			!= EAIREPlayerWarehouseTransferDirection::WithdrawWarehouseToPlayer)
	{
		return MakeResult(
			EAIREInventoryMutationCode::InvalidOperation,
			Request.MutationId,
			Warehouse->Revision);
	}

	TArray<FInventoryItemStack> NewPlayerItems;
	TArray<FAIREInventoryItemStackSnapshot> NewWarehouseStacks =
		Warehouse->ItemStacks;
	FName ItemId;
	if (Request.Direction
		== EAIREPlayerWarehouseTransferDirection::DepositPlayerToWarehouse)
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
				Warehouse->Revision);
		}
		if (PlayerInventory->Items[PlayerStackIndex].Count < Request.Count)
		{
			return MakeResult(
				EAIREInventoryMutationCode::InsufficientQuantity,
				Request.MutationId,
				Warehouse->Revision);
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
				Warehouse->Revision);
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
				Warehouse->Revision);
		}

		if (!TryAddToStacks(
				NewWarehouseStacks,
				Warehouse->Capacity,
				ItemId,
				Request.Count,
				Rules.MaxStackSize))
		{
			return MakeResult(
				EAIREInventoryMutationCode::CapacityExceeded,
				Request.MutationId,
				Warehouse->Revision);
		}
	}
	else
	{
		if (Request.SourceSlotIndex < 0
			|| Request.SourceSlotIndex >= Warehouse->Capacity
			|| FindStackIndexBySlot(
				Warehouse->ItemStacks,
				Request.SourceSlotIndex) == INDEX_NONE)
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidSlot,
				Request.MutationId,
				Warehouse->Revision);
		}
		if (!TryRemoveFromSlot(
				NewWarehouseStacks,
				Request.SourceSlotIndex,
				Request.Count,
				ItemId))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InsufficientQuantity,
				Request.MutationId,
				Warehouse->Revision);
		}
		FAIREItemRules Rules;
		if (!ResolveItemRules(ItemId, false, Rules))
		{
			return MakeResult(
				EAIREInventoryMutationCode::InvalidItem,
				Request.MutationId,
				Warehouse->Revision);
		}
		if (!PlayerInventory->BuildExactAddState(
				ItemId,
				Request.Count,
				NewPlayerItems))
		{
			return MakeResult(
				EAIREInventoryMutationCode::CapacityExceeded,
				Request.MutationId,
				Warehouse->Revision);
		}
	}

	PlayerInventory->CommitExactInventoryState(MoveTemp(NewPlayerItems));
	Warehouse->ItemStacks = MoveTemp(NewWarehouseStacks);
	++Warehouse->Revision;
	FAIREInventoryMutationResult Result = MakeResult(
		EAIREInventoryMutationCode::Succeeded,
		Request.MutationId,
		Warehouse->Revision);
	RecordAppliedMutation(Result);
	PlayerInventory->NotifyExactInventoryMutation();
	BroadcastContainerChanged(*Warehouse);
	return Result;
}

FGuid UAIREGameplayInventorySubsystem::ResetInventorySession(
	const FAIREInventorySessionScope& NewScope)
{
	SessionScope = NewScope;
	InventorySessionId = FGuid::NewGuid();
	AppliedMutations.Reset();
	AppliedWorkResults.Reset();
	AppliedImportCandidateIds.Reset();
	AppliedImportOperationIds.Reset();
	bMakoInventoryInitialized = false;
	CreateEmptyContainers();
	BroadcastContainerChanged(Containers.FindChecked(GetMakoContainerId()));
	BroadcastContainerChanged(
		Containers.FindChecked(GetSharedWarehouseContainerId()));
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
	if (!InventorySessionId.IsValid())
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
	const FAIREContainerState* WarehouseContainer =
		FindContainer(GetSharedWarehouseContainerId());
	if (!MakoContainer || !WarehouseContainer)
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidContainer;
		return false;
	}
	if (MakoContainer->Revision != Request.ExpectedMakoRevision
		|| WarehouseContainer->Revision != Request.ExpectedWarehouseRevision)
	{
		OutResult.Code = EAIREInventoryMutationCode::RevisionConflict;
		OutResult.MakoRevision = MakoContainer->Revision;
		OutResult.WarehouseRevision = WarehouseContainer->Revision;
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
		int64 AvailableCount = 0;
		for (const FAIREInventoryItemStackSnapshot& Stack : MakoContainer->ItemStacks)
		{
			if (Stack.ItemId == Ingredient.Key)
			{
				AvailableCount += Stack.Count;
			}
		}
		if (AvailableCount < Ingredient.Value)
		{
			OutResult.Code = EAIREInventoryMutationCode::InsufficientQuantity;
			return false;
		}
	}

	FAIREItemRules WarehouseResultRules;
	if (!ResolveItemRules(Request.Result.ItemId, false, WarehouseResultRules))
	{
		OutResult.Code = EAIREInventoryMutationCode::InvalidItem;
		return false;
	}
	TArray<FAIREInventoryItemStackSnapshot> MakoAfterIngredients =
		MakoContainer->ItemStacks;
	for (const TPair<FName, int32>& Ingredient : IngredientTotals)
	{
		if (!TryRemoveItemFromStacks(
				MakoAfterIngredients,
				Ingredient.Key,
				Ingredient.Value))
		{
			OutResult.Code = EAIREInventoryMutationCode::InsufficientQuantity;
			return false;
		}
	}
	FAIREItemRules MakoResultRules;
	if (ResolveItemRules(Request.Result.ItemId, false, MakoResultRules)
		&& TryAddToStacks(MakoAfterIngredients, MakoContainer->Capacity,
			Request.Result.ItemId, Request.Result.Count, MakoResultRules.MaxStackSize))
	{
		OutResult = MakeWorkResult(EAIREInventoryMutationCode::Succeeded,
			EAIREInventoryWorkResultDestination::Mako, Request.Result);
		return true;
	}
	TArray<FAIREInventoryItemStackSnapshot> WarehouseAfterResult =
		WarehouseContainer->ItemStacks;
	if (TryAddToStacks(WarehouseAfterResult, WarehouseContainer->Capacity,
		Request.Result.ItemId, Request.Result.Count, WarehouseResultRules.MaxStackSize))
	{
		OutResult = MakeWorkResult(EAIREInventoryMutationCode::Succeeded,
			EAIREInventoryWorkResultDestination::SharedWarehouse, Request.Result);
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
	FAIREContainerState* WarehouseContainer = FindContainer(GetSharedWarehouseContainerId());
	TMap<FName, int32> IngredientTotals;
	if (!MakoContainer || !WarehouseContainer
		|| !AggregateWorkIngredients(Request.Ingredients, IngredientTotals))
	{
		Result.Code = EAIREInventoryMutationCode::InvalidOperation;
		return Result;
	}
	TArray<FAIREInventoryItemStackSnapshot> NewMakoStacks = MakoContainer->ItemStacks;
	for (const TPair<FName, int32>& Ingredient : IngredientTotals)
	{
		if (!TryRemoveItemFromStacks(NewMakoStacks, Ingredient.Key, Ingredient.Value))
		{
			Result.Code = EAIREInventoryMutationCode::InsufficientQuantity;
			return Result;
		}
	}
	TArray<FAIREInventoryItemStackSnapshot> NewWarehouseStacks = WarehouseContainer->ItemStacks;
	FAIREItemRules ResultRules;
	const bool bResultInMako = Result.Destination == EAIREInventoryWorkResultDestination::Mako;
	if (Result.Destination != EAIREInventoryWorkResultDestination::WorldDrop
		&& (!ResolveItemRules(Request.Result.ItemId, false, ResultRules)
			|| !TryAddToStacks(bResultInMako ? NewMakoStacks : NewWarehouseStacks,
				bResultInMako ? MakoContainer->Capacity : WarehouseContainer->Capacity,
				Request.Result.ItemId, Request.Result.Count, ResultRules.MaxStackSize)))
	{
		Result.Code = EAIREInventoryMutationCode::InvalidOperation;
		return Result;
	}
	MakoContainer->ItemStacks = MoveTemp(NewMakoStacks);
	++MakoContainer->Revision;
	if (Result.Destination == EAIREInventoryWorkResultDestination::SharedWarehouse)
	{
		WarehouseContainer->ItemStacks = MoveTemp(NewWarehouseStacks);
		++WarehouseContainer->Revision;
	}
	Result.MakoRevision = MakoContainer->Revision;
	Result.WarehouseRevision = WarehouseContainer->Revision;
	AppliedWorkResults.Add(Request.WorkOrderId, Result);
	RecordAppliedMutation(MakeResult(EAIREInventoryMutationCode::Succeeded,
		Request.WorkOrderId, MakoContainer->Revision, WarehouseContainer->Revision));
	BroadcastContainerChanged(*MakoContainer);
	if (Result.Destination == EAIREInventoryWorkResultDestination::SharedWarehouse)
	{
		BroadcastContainerChanged(*WarehouseContainer);
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
	if (!InventorySessionId.IsValid()) { return Result; }
	if (Request.SessionId != InventorySessionId) { Result.Code = EAIREInventoryMutationCode::InvalidSession; return Result; }
	if (!Request.DeliveryId.IsValid()) { Result.Code = EAIREInventoryMutationCode::InvalidMutationId; return Result; }
	FAIREInventoryMutationResult ExistingMutation;
	if (FindAppliedMutation(Request.DeliveryId, ExistingMutation))
	{
		Result.Code = EAIREInventoryMutationCode::DuplicateOperation;
		return Result;
	}
	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	FAIREContainerState* WarehouseContainer = FindContainer(GetSharedWarehouseContainerId());
	if (!MakoContainer || !WarehouseContainer) { Result.Code = EAIREInventoryMutationCode::InvalidContainer; return Result; }
	if (MakoContainer->Revision != Request.ExpectedMakoRevision || WarehouseContainer->Revision != Request.ExpectedWarehouseRevision) { Result.Code = EAIREInventoryMutationCode::RevisionConflict; Result.MakoRevision = MakoContainer->Revision; Result.WarehouseRevision = WarehouseContainer->Revision; return Result; }
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
		Result.WarehouseRevision = WarehouseContainer->Revision;
		AppliedWorkResults.Add(Request.DeliveryId, Result);
		RecordAppliedMutation(MakeResult(EAIREInventoryMutationCode::Succeeded, Request.DeliveryId, MakoContainer->Revision, WarehouseContainer->Revision));
		BroadcastContainerChanged(*MakoContainer);
		return Result;
	}
	TArray<FAIREInventoryItemStackSnapshot> NewWarehouseStacks = WarehouseContainer->ItemStacks;
	FAIREItemRules WarehouseRules;
	if (ResolveItemRules(Request.Reward.ItemId, false, WarehouseRules)
		&& TryAddToStacks(NewWarehouseStacks, WarehouseContainer->Capacity, Request.Reward.ItemId, Request.Reward.Count, WarehouseRules.MaxStackSize))
	{
		WarehouseContainer->ItemStacks = MoveTemp(NewWarehouseStacks);
		++WarehouseContainer->Revision;
		Result = MakeWorkResult(EAIREInventoryMutationCode::Succeeded, EAIREInventoryWorkResultDestination::SharedWarehouse, Request.Reward);
		Result.MakoRevision = MakoContainer->Revision;
		Result.WarehouseRevision = WarehouseContainer->Revision;
		AppliedWorkResults.Add(Request.DeliveryId, Result);
		RecordAppliedMutation(MakeResult(EAIREInventoryMutationCode::Succeeded, Request.DeliveryId, MakoContainer->Revision, WarehouseContainer->Revision));
		BroadcastContainerChanged(*WarehouseContainer);
		return Result;
	}
	Result = MakeWorkResult(
		EAIREInventoryMutationCode::Succeeded,
		EAIREInventoryWorkResultDestination::WorldDrop,
		Request.Reward);
	Result.MakoRevision = MakoContainer->Revision;
	Result.WarehouseRevision = WarehouseContainer->Revision;
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

	FAIREContainerState* MakoContainer = FindContainer(GetMakoContainerId());
	if (!MakoContainer)
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

	MakoContainer->ItemStacks = MoveTemp(NewStacks);
	MakoContainer->EquippedItemId = EquippedItemId;
	bMakoInventoryInitialized = true;
	if (!MakoContainer->ItemStacks.IsEmpty()
		|| !MakoContainer->EquippedItemId.IsNone())
	{
		++MakoContainer->Revision;
		BroadcastContainerChanged(*MakoContainer);
	}
	return true;
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
		const bool bRequireCompanionItem =
			Container->ContainerId == GetMakoContainerId();
		if (Operation.Count <= 0
			|| !ResolveItemRules(
				Operation.ItemId,
				bRequireCompanionItem,
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
	AppliedImportCandidateIds.Add(Candidate.CandidateId);
	AppliedImportOperationIds.Append(CandidateOperationIds);
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

	FAIREContainerState Warehouse;
	Warehouse.ContainerId = GetSharedWarehouseContainerId();
	Warehouse.Capacity =
		AIREGameplayInventory::SharedWarehouseSlotCapacity;
	Containers.Add(Warehouse.ContainerId, MoveTemp(Warehouse));
}

UAIREGameplayInventorySubsystem::FAIREContainerState*
UAIREGameplayInventorySubsystem::FindContainer(const FName ContainerId)
{
	return Containers.Find(ContainerId);
}

const UAIREGameplayInventorySubsystem::FAIREContainerState*
UAIREGameplayInventorySubsystem::FindContainer(const FName ContainerId) const
{
	return Containers.Find(ContainerId);
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

FAIREInventoryMutationResult
UAIREGameplayInventorySubsystem::ValidateMutation(
	const FGuid& RequestSessionId,
	const FGuid& MutationId,
	const FAIREContainerState* Container,
	const int64 ExpectedRevision) const
{
	if (!InventorySessionId.IsValid())
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
	const FAIREInventoryMutationResult* Found =
		AppliedMutations.Find(MutationId);
	if (!Found)
	{
		return false;
	}
	OutResult = *Found;
	return true;
}

void UAIREGameplayInventorySubsystem::RecordAppliedMutation(
	const FAIREInventoryMutationResult& Result)
{
	if (Result.MutationId.IsValid())
	{
		AppliedMutations.Add(Result.MutationId, Result);
	}
}

void UAIREGameplayInventorySubsystem::BroadcastContainerChanged(
	const FAIREContainerState& Container)
{
#if WITH_DEV_AUTOMATION_TESTS
	++GInventoryAutomationBroadcastCount;
#endif
	OnContainerChanged.Broadcast(Container.ContainerId, Container.Revision);
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
	const FAIREContainerState* WarehouseContainer =
		FindContainer(GetSharedWarehouseContainerId());
	Result.MakoRevision = MakoContainer ? MakoContainer->Revision : INDEX_NONE;
	Result.WarehouseRevision = WarehouseContainer
		? WarehouseContainer->Revision
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
		return false;
	}
	OutResult = *Found;
	return true;
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
	RecordAppliedMutation(Result);
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
	RecordAppliedMutation(Result);
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
		TEXT("Warehouse fixed 50-slot capacity is enforced"),
		FillContainer(
			UAIREGameplayInventorySubsystem::GetSharedWarehouseContainerId(),
			AIREGameplayInventory::SharedWarehouseSlotCapacity,
			TEXT("AIRE.Test.Unique.Warehouse")));
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
	const FName WarehouseContainerId =
		UAIREGameplayInventorySubsystem::GetSharedWarehouseContainerId();
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
	FAIREInventoryContainerSnapshot WarehouseSnapshot;
	TestTrue(
		TEXT("MAKO snapshot is available"),
		Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot));
	TestTrue(
		TEXT("Warehouse snapshot is available"),
		Inventory->GetContainerSnapshot(
			WarehouseContainerId,
			WarehouseSnapshot));
	TestEqual(TEXT("MAKO has 20 general slots"), MakoSnapshot.Capacity, 20);
	TestEqual(
		TEXT("Warehouse has 50 slots"),
		WarehouseSnapshot.Capacity,
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
	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	FAIREInventoryTransferRequest TransferRequest;
	TransferRequest.SessionId = SessionId;
	TransferRequest.MutationId = FGuid::NewGuid();
	TransferRequest.SourceContainerId = MakoContainerId;
	TransferRequest.DestinationContainerId = WarehouseContainerId;
	TransferRequest.ExpectedSourceRevision = MakoSnapshot.Revision;
	TransferRequest.ExpectedDestinationRevision = WarehouseSnapshot.Revision + 1;
	TransferRequest.SourceSlotIndex = MakoSnapshot.ItemStacks[0].SlotIndex;
	TransferRequest.Count = 1;
	const int32 BroadcastsBeforeConflict = GInventoryAutomationBroadcastCount;
	TestTrue(
		TEXT("Transfer destination conflict is rejected"),
		Inventory->TryTransferItem(TransferRequest).Code
			== EAIREInventoryMutationCode::RevisionConflict);
	FAIREInventoryContainerSnapshot MakoAfterConflict;
	FAIREInventoryContainerSnapshot WarehouseAfterConflict;
	Inventory->GetContainerSnapshot(MakoContainerId, MakoAfterConflict);
	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseAfterConflict);
	TestEqual(
		TEXT("Rejected transfer preserves source revision"),
		MakoAfterConflict.Revision,
		MakoSnapshot.Revision);
	TestEqual(
		TEXT("Rejected transfer preserves destination revision"),
		WarehouseAfterConflict.Revision,
		WarehouseSnapshot.Revision);
	TestEqual(
		TEXT("Rejected transfer does not broadcast"),
		GInventoryAutomationBroadcastCount,
		BroadcastsBeforeConflict);

	TransferRequest.MutationId = FGuid::NewGuid();
	TransferRequest.ExpectedDestinationRevision = WarehouseSnapshot.Revision;
	const int32 BroadcastsBeforeTransfer = GInventoryAutomationBroadcastCount;
	TestTrue(
		TEXT("MAKO to warehouse transfer succeeds"),
		Inventory->TryTransferItem(TransferRequest).Code
			== EAIREInventoryMutationCode::Succeeded);
	TestEqual(
		TEXT("Two-container transfer broadcasts each container once"),
		GInventoryAutomationBroadcastCount,
		BroadcastsBeforeTransfer + 2);

	Inventory->GetContainerSnapshot(MakoContainerId, MakoSnapshot);
	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	FAIREInventoryTransferRequest ReverseTransfer;
	ReverseTransfer.SessionId = SessionId;
	ReverseTransfer.MutationId = FGuid::NewGuid();
	ReverseTransfer.SourceContainerId = WarehouseContainerId;
	ReverseTransfer.DestinationContainerId = MakoContainerId;
	ReverseTransfer.ExpectedSourceRevision = WarehouseSnapshot.Revision;
	ReverseTransfer.ExpectedDestinationRevision = MakoSnapshot.Revision;
	ReverseTransfer.SourceSlotIndex = WarehouseSnapshot.ItemStacks[0].SlotIndex;
	ReverseTransfer.Count = 1;
	TestTrue(
		TEXT("Warehouse to MAKO transfer succeeds"),
		Inventory->TryTransferItem(ReverseTransfer).Code
			== EAIREInventoryMutationCode::Succeeded);

	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	FAIREInventoryStartupImportCandidate Candidate;
	Candidate.CandidateId = TEXT("candidate.good");
	Candidate.Scope = Scope;
	Candidate.SessionId = SessionId;
	Candidate.ContainerId = WarehouseContainerId;
	Candidate.BaseRevision = WarehouseSnapshot.Revision;
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

	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	FAIREInventoryStartupImportCandidate ImportConflictCandidate = Candidate;
	ImportConflictCandidate.CandidateId = TEXT("candidate.conflict");
	ImportConflictCandidate.BaseRevision = WarehouseSnapshot.Revision - 1;
	ImportConflictCandidate.Operations[0].OperationId =
		TEXT("operation.conflict.add");
	TestTrue(
		TEXT("Startup import base revision conflict is rejected"),
		Inventory->TryApplyStartupImportCandidate(
			ImportConflictCandidate).Code
			== EAIREInventoryMutationCode::RevisionConflict);

	FAIREInventoryStartupImportCandidate DuplicateOperationCandidate = Candidate;
	DuplicateOperationCandidate.CandidateId = TEXT("candidate.duplicate");
	DuplicateOperationCandidate.BaseRevision = WarehouseSnapshot.Revision;
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
	RollbackCandidate.BaseRevision = WarehouseSnapshot.Revision;
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
	const int64 WarehouseRevisionBeforeRollback = WarehouseSnapshot.Revision;
	TestTrue(
		TEXT("Invalid import batch is rejected"),
		Inventory->TryApplyStartupImportCandidate(RollbackCandidate).Code
			== EAIREInventoryMutationCode::InsufficientQuantity);
	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	TestEqual(
		TEXT("Rejected import batch rolls back revision"),
		WarehouseSnapshot.Revision,
		WarehouseRevisionBeforeRollback);
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
	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	FAIREPlayerWarehouseTransferRequest PlayerDeposit;
	PlayerDeposit.SessionId = NewSessionId;
	PlayerDeposit.MutationId = FGuid::NewGuid();
	PlayerDeposit.Direction =
		EAIREPlayerWarehouseTransferDirection::DepositPlayerToWarehouse;
	PlayerDeposit.ExpectedWarehouseRevision = WarehouseSnapshot.Revision;
	PlayerDeposit.SourceSlotIndex = 0;
	PlayerDeposit.Count = 1;
	TestTrue(
		TEXT("Player to warehouse exact transfer succeeds"),
		Inventory->TryTransferPlayerWarehouse(
			PlayerInventory.Get(),
			PlayerDeposit).Code == EAIREInventoryMutationCode::Succeeded);
	TestEqual(TEXT("Player deposit removes exactly one"), PlayerInventory->Items[0].Count, 1);
	FAIREInventoryContainerSnapshot WarehouseAfterDeposit;
	Inventory->GetContainerSnapshot(
		WarehouseContainerId,
		WarehouseAfterDeposit);
	TestTrue(
		TEXT("Player transfer replay is idempotent"),
		Inventory->TryTransferPlayerWarehouse(
			PlayerInventory.Get(),
			PlayerDeposit).Code == EAIREInventoryMutationCode::AlreadyApplied);
	TestEqual(
		TEXT("Player transfer replay preserves player quantity"),
		PlayerInventory->Items[0].Count,
		1);
	FAIREInventoryContainerSnapshot WarehouseAfterDepositReplay;
	Inventory->GetContainerSnapshot(
		WarehouseContainerId,
		WarehouseAfterDepositReplay);
	TestEqual(
		TEXT("Player transfer replay preserves warehouse revision"),
		WarehouseAfterDepositReplay.Revision,
		WarehouseAfterDeposit.Revision);

	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	FAIREPlayerWarehouseTransferRequest PlayerWithdraw;
	PlayerWithdraw.SessionId = NewSessionId;
	PlayerWithdraw.MutationId = FGuid::NewGuid();
	PlayerWithdraw.Direction =
		EAIREPlayerWarehouseTransferDirection::WithdrawWarehouseToPlayer;
	PlayerWithdraw.ExpectedWarehouseRevision = WarehouseSnapshot.Revision;
	PlayerWithdraw.SourceSlotIndex = WarehouseSnapshot.ItemStacks[0].SlotIndex;
	PlayerWithdraw.Count = 1;
	TestTrue(
		TEXT("Warehouse to player exact transfer succeeds"),
		Inventory->TryTransferPlayerWarehouse(
			PlayerInventory.Get(),
			PlayerWithdraw).Code == EAIREInventoryMutationCode::Succeeded);
	TestEqual(TEXT("Player withdrawal restores exact count"), PlayerInventory->Items[0].Count, 2);

	FAIREInventoryMutationRequest WarehouseAdd;
	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	WarehouseAdd.SessionId = NewSessionId;
	WarehouseAdd.MutationId = FGuid::NewGuid();
	WarehouseAdd.ContainerId = WarehouseContainerId;
	WarehouseAdd.ExpectedRevision = WarehouseSnapshot.Revision;
	WarehouseAdd.ItemId = Stack2ItemId;
	WarehouseAdd.Count = 1;
	TestTrue(
		TEXT("Warehouse source is prepared for atomic rejection"),
		Inventory->TryAddItem(WarehouseAdd).Code
			== EAIREInventoryMutationCode::Succeeded);
	PlayerInventory->Items[0].Count = 4;
	Inventory->GetContainerSnapshot(WarehouseContainerId, WarehouseSnapshot);
	const FAIREInventoryItemStackSnapshot* WarehouseStack2 =
		WarehouseSnapshot.ItemStacks.FindByPredicate(
			[Stack2ItemId](const FAIREInventoryItemStackSnapshot& Stack)
			{
				return Stack.ItemId == Stack2ItemId;
			});
	if (!TestNotNull(TEXT("Warehouse stack-2 item exists"), WarehouseStack2))
	{
		return false;
	}
	PlayerWithdraw.MutationId = FGuid::NewGuid();
	PlayerWithdraw.ExpectedWarehouseRevision = WarehouseSnapshot.Revision;
	PlayerWithdraw.SourceSlotIndex = WarehouseStack2->SlotIndex;
	TestTrue(
		TEXT("Full player inventory rejects the entire withdrawal"),
		Inventory->TryTransferPlayerWarehouse(
			PlayerInventory.Get(),
			PlayerWithdraw).Code
			== EAIREInventoryMutationCode::CapacityExceeded);
	FAIREInventoryContainerSnapshot WarehouseAfterRejectedWithdraw;
	Inventory->GetContainerSnapshot(
		WarehouseContainerId,
		WarehouseAfterRejectedWithdraw);
	TestEqual(
		TEXT("Rejected player withdrawal preserves warehouse revision"),
		WarehouseAfterRejectedWithdraw.Revision,
		WarehouseSnapshot.Revision);
	TestEqual(
		TEXT("Rejected player withdrawal preserves player quantity"),
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
	const FName WarehouseContainerId =
		UAIREGameplayInventorySubsystem::GetSharedWarehouseContainerId();
	const FName IngredientItemId(TEXT("AIRE.Test.Stack4"));
	const FName ResultItemId(TEXT("AIRE.Test.Stack2"));

	TStrongObjectPtr<UGameInstance> TestGameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
		NewObject<UAIREGameplayInventorySubsystem>(TestGameInstance.Get()));
	const FGuid SessionId = Inventory->ResetInventorySession();

	auto GetSnapshots = [&Inventory, &MakoContainerId, &WarehouseContainerId](
		FAIREInventoryContainerSnapshot& OutMako,
		FAIREInventoryContainerSnapshot& OutWarehouse)
	{
		return Inventory->GetContainerSnapshot(MakoContainerId, OutMako)
			&& Inventory->GetContainerSnapshot(
				WarehouseContainerId,
				OutWarehouse);
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
	FAIREInventoryContainerSnapshot WarehouseBefore;
	if (!TestTrue(
			TEXT("Craft work snapshots are available"),
			GetSnapshots(MakoBefore, WarehouseBefore)))
	{
		return false;
	}

	FAIREMakoCraftWorkRequest CraftRequest;
	CraftRequest.SessionId = SessionId;
	CraftRequest.WorkOrderId = FGuid::NewGuid();
	CraftRequest.ExpectedMakoRevision = MakoBefore.Revision;
	CraftRequest.ExpectedWarehouseRevision = WarehouseBefore.Revision;
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
	FAIREInventoryContainerSnapshot WarehouseAfter;
	GetSnapshots(MakoAfter, WarehouseAfter);
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
		TEXT("MAKO craft does not change warehouse revision"),
		WarehouseAfter.Revision,
		WarehouseBefore.Revision);

	const FAIREInventoryWorkResult ReplayResult =
		Inventory->TryCompleteMakoCraftWork(CraftRequest);
	TestTrue(
		TEXT("Craft replay is idempotent"),
		ReplayResult.Code == EAIREInventoryMutationCode::AlreadyApplied
			&& ReplayResult.bAlreadyApplied);
	FAIREInventoryContainerSnapshot MakoAfterReplay;
	FAIREInventoryContainerSnapshot WarehouseAfterReplay;
	GetSnapshots(MakoAfterReplay, WarehouseAfterReplay);
	TestEqual(
		TEXT("Craft replay preserves MAKO revision"),
		MakoAfterReplay.Revision,
		MakoAfter.Revision);
	TestEqual(
		TEXT("Craft replay preserves warehouse revision"),
		WarehouseAfterReplay.Revision,
		WarehouseAfter.Revision);

	FAIREMakoCraftWorkRequest StaleRequest = CraftRequest;
	StaleRequest.WorkOrderId = FGuid::NewGuid();
	TestTrue(
		TEXT("Stale craft revision is rejected"),
		Inventory->TryCompleteMakoCraftWork(StaleRequest).Code
			== EAIREInventoryMutationCode::RevisionConflict);
	FAIREMakoCraftWorkRequest InsufficientRequest = CraftRequest;
	InsufficientRequest.WorkOrderId = FGuid::NewGuid();
	InsufficientRequest.ExpectedMakoRevision = MakoAfter.Revision;
	InsufficientRequest.ExpectedWarehouseRevision = WarehouseAfter.Revision;
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
	FAIREInventoryContainerSnapshot WarehouseAfterRejectedRequests;
	GetSnapshots(MakoAfterRejectedRequests, WarehouseAfterRejectedRequests);
	TestEqual(
		TEXT("Rejected craft requests preserve MAKO revision"),
		MakoAfterRejectedRequests.Revision,
		MakoAfter.Revision);
	TestEqual(
		TEXT("Rejected craft requests preserve warehouse revision"),
		WarehouseAfterRejectedRequests.Revision,
		WarehouseAfter.Revision);
	FAIREMakoWorkRewardRequest RewardRequest;
	RewardRequest.SessionId = SessionId;
	RewardRequest.DeliveryId = FGuid::NewGuid();
	RewardRequest.ExpectedMakoRevision =
		MakoAfterRejectedRequests.Revision;
	RewardRequest.ExpectedWarehouseRevision =
		WarehouseAfterRejectedRequests.Revision;
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
	FAIREInventoryContainerSnapshot WarehouseDuringEquipment;
	GetSnapshots(MakoDuringEquipment, WarehouseDuringEquipment);
	FAIREMakoCraftWorkRequest EquipmentBusyRequest = CraftRequest;
	EquipmentBusyRequest.WorkOrderId = FGuid::NewGuid();
	EquipmentBusyRequest.ExpectedMakoRevision = MakoDuringEquipment.Revision;
	EquipmentBusyRequest.ExpectedWarehouseRevision =
		WarehouseDuringEquipment.Revision;
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
		TEXT("Full warehouse is prepared"),
		FillContainer(
			WarehouseContainerId,
			AIREGameplayInventory::SharedWarehouseSlotCapacity,
			TEXT("AIRE.Test.Unique.FullWarehouse")));
	FAIREInventoryContainerSnapshot FullMako;
	FAIREInventoryContainerSnapshot FullWarehouse;
	FullInventory->GetContainerSnapshot(MakoContainerId, FullMako);
	FullInventory->GetContainerSnapshot(WarehouseContainerId, FullWarehouse);
	FAIREMakoCraftWorkRequest WorldDropRequest;
	WorldDropRequest.SessionId = FullSessionId;
	WorldDropRequest.WorkOrderId = FGuid::NewGuid();
	WorldDropRequest.ExpectedMakoRevision = FullMako.Revision;
	WorldDropRequest.ExpectedWarehouseRevision = FullWarehouse.Revision;
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
	return true;
}

#endif

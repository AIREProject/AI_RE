// Copyright MixUpProject. All Rights Reserved.

#include "AI_REPlayerInventoryComponent.h"
#include "Engine/World.h"

UAI_REPlayerInventoryComponent::UAI_REPlayerInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAI_REPlayerInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UAI_REPlayerInventoryComponent::HasItem(FName ItemId, int32 Amount) const
{
	return GetItemCount(ItemId) >= Amount;
}

bool UAI_REPlayerInventoryComponent::SwapSlots(int32 SlotIndexA, int32 SlotIndexB)
{
	return MoveItemSlot(SlotIndexA, SlotIndexB);
}

bool UAI_REPlayerInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0) return false;

	const int32 MaxStack = GetMaxStackForItem(ItemId);
	int32 RemainingCount = Count;

	// Fill existing stacks first
	for (FInventoryItemStack& ExistingStack : Items)
	{
		if (ExistingStack.ItemId != ItemId || ExistingStack.Count >= MaxStack) continue;

		const int32 AddCount = FMath::Min(RemainingCount, MaxStack - ExistingStack.Count);
		ExistingStack.Count += AddCount;
		RemainingCount -= AddCount;

		if (RemainingCount <= 0) break;
	}

	// Create new stacks
	while (RemainingCount > 0)
	{
		const int32 EmptySlotIndex = FindFirstEmptySlotIndex();
		if (EmptySlotIndex == INDEX_NONE) break;

		const int32 AddCount = FMath::Min(RemainingCount, MaxStack);
		FInventoryItemStack NewStack;
		NewStack.SlotIndex = EmptySlotIndex;
		NewStack.ItemId = ItemId;
		NewStack.Count = AddCount;
		Items.Add(NewStack);
		RemainingCount -= AddCount;
	}

	OnInventoryChanged.Broadcast();
	return RemainingCount <= 0;
}

bool UAI_REPlayerInventoryComponent::ConsumeItem(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0) return false;
	if (GetItemCount(ItemId) < Count) return false;

	int32 RemainingCount = Count;
	for (int32 i = Items.Num() - 1; i >= 0 && RemainingCount > 0; --i)
	{
		FInventoryItemStack& Stack = Items[i];
		if (Stack.ItemId != ItemId || Stack.Count <= 0) continue;

		const int32 RemoveCount = FMath::Min(Stack.Count, RemainingCount);
		Stack.Count -= RemoveCount;
		RemainingCount -= RemoveCount;

		if (Stack.Count <= 0)
		{
			Items.RemoveAt(i);
		}
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UAI_REPlayerInventoryComponent::MoveItemSlot(int32 FromSlotIndex, int32 ToSlotIndex)
{
	if (FromSlotIndex == ToSlotIndex || !IsSlotIndexValid(FromSlotIndex) || !IsSlotIndexValid(ToSlotIndex))
		return false;

	FInventoryItemStack* FromStack = FindStackBySlot(FromSlotIndex);
	if (!FromStack) return false;

	FInventoryItemStack* ToStack = FindStackBySlot(ToSlotIndex);
	if (!ToStack)
	{
		FromStack->SlotIndex = ToSlotIndex;
		OnInventoryChanged.Broadcast();
		return true;
	}

	if (FromStack->ItemId == ToStack->ItemId)
	{
		const int32 MaxStack = GetMaxStackForItem(FromStack->ItemId);
		const int32 MoveCount = FMath::Min(FromStack->Count, MaxStack - ToStack->Count);
		if (MoveCount > 0)
		{
			ToStack->Count += MoveCount;
			FromStack->Count -= MoveCount;
			if (FromStack->Count <= 0)
			{
				Items.RemoveAll([FromSlotIndex](const FInventoryItemStack& Stack) { return Stack.SlotIndex == FromSlotIndex; });
			}
			OnInventoryChanged.Broadcast();
			return true;
		}
	}

	Swap(FromStack->SlotIndex, ToStack->SlotIndex);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UAI_REPlayerInventoryComponent::DropItemFromSlot(int32 SlotIndex, int32 Count)
{
	FInventoryItemStack* Stack = FindStackBySlot(SlotIndex);
	if (!Stack || Stack->ItemId.IsNone() || Stack->Count <= 0) return false;

	const int32 RemoveCount = Count <= 0 ? Stack->Count : FMath::Min(Count, Stack->Count);
	Stack->Count -= RemoveCount;
	
	if (Stack->Count <= 0)
	{
		Items.RemoveAll([SlotIndex](const FInventoryItemStack& Candidate) { return Candidate.SlotIndex == SlotIndex; });
	}

	// TODO: Spawn physical item in the world.
	
	OnInventoryChanged.Broadcast();
	return true;
}

int32 UAI_REPlayerInventoryComponent::GetItemCount(FName ItemId) const
{
	int32 TotalCount = 0;
	for (const FInventoryItemStack& Stack : Items)
	{
		if (Stack.ItemId == ItemId) TotalCount += Stack.Count;
	}
	return TotalCount;
}

bool UAI_REPlayerInventoryComponent::IsInventoryFull() const
{
	if (FindFirstEmptySlotIndex() != INDEX_NONE) return false;
	
	for (const FInventoryItemStack& Stack : Items)
	{
		// 퀵슬롯(100번대)이 아닌 기본 인벤토리만 풀 상태인지 체크할 수도 있지만, 일단 전체 아이템 기준
		if (Stack.SlotIndex >= 0 && Stack.SlotIndex < MaxSlots && Stack.Count < GetMaxStackForItem(Stack.ItemId)) return false;
	}
	return true;
}

bool UAI_REPlayerInventoryComponent::IsSlotIndexValid(int32 SlotIndex) const
{
	// 0 ~ MaxSlots-1: 일반 인벤토리 슬롯
	// 100 ~ 110: 퀵슬롯
	return (SlotIndex >= 0 && SlotIndex < MaxSlots) || (SlotIndex >= 100 && SlotIndex < 110);
}

FInventoryItemStack* UAI_REPlayerInventoryComponent::FindStackBySlot(int32 SlotIndex)
{
	return Items.FindByPredicate([SlotIndex](const FInventoryItemStack& Stack) { return Stack.SlotIndex == SlotIndex; });
}

int32 UAI_REPlayerInventoryComponent::FindFirstEmptySlotIndex() const
{
	for (int32 i = 0; i < MaxSlots; ++i)
	{
		if (!const_cast<UAI_REPlayerInventoryComponent*>(this)->FindStackBySlot(i)) return i;
	}
	return INDEX_NONE;
}

int32 UAI_REPlayerInventoryComponent::GetMaxStackForItem(FName ItemId) const
{
	// Default max stack fallback. Hook up to DataAsset later.
	return 99;
}

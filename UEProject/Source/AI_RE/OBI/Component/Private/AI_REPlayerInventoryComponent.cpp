// Copyright MixUpProject. All Rights Reserved.

#include "AI_REPlayerInventoryComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "AI_REItemSubsystem.h"
#include "../../OBI/Component/Public/AI_REItemDataAsset.h"
#include "AI_REItemEffect.h"
#include "../../Global/Characters/Public/AI_RECharacterBase.h"
#include "Engine/Engine.h"

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

bool UAI_REPlayerInventoryComponent::UseItem(int32 SlotIndex)
{
	int32 Idx = FindStackIndexBySlot(SlotIndex);
	if (Idx == INDEX_NONE || Items[Idx].Count <= 0 || Items[Idx].ItemId.IsNone())
	{
		return false;
	}

	bool bConsumed = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UAI_REItemSubsystem* Subsystem = GI->GetSubsystem<UAI_REItemSubsystem>())
			{
				if (UAI_REItemDataAsset* DA = Subsystem->GetItemDataAsset(Items[Idx].ItemId))
				{
					if (DA->ItemEffect)
					{
						AAI_RECharacterBase* OwnerChar = Cast<AAI_RECharacterBase>(GetOwner());
						if (OwnerChar && DA->ItemEffect->ApplyEffect(OwnerChar))
						{
							bConsumed = true;
							GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("[Item Usage] %s 효과 적용 성공!"), *DA->DisplayName.ToString()));
						}
					}
					else if (DA->ItemType == EAI_REItemType::Consumable)
					{
						// Default consumable behavior if no effect defined
						bConsumed = true; 
						GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, FString::Printf(TEXT("[Item Usage] %s 기본 소비 동작 (이펙트 없음)"), *DA->DisplayName.ToString()));
					}
					else
					{
						GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("[Item Usage] %s 은(는) 사용할 수 없는 아이템입니다."), *DA->DisplayName.ToString()));
					}
				}
			}
		}
	}

	if (bConsumed)
	{
		Items[Idx].Count--;
		if (Items[Idx].Count <= 0)
		{
			Items.RemoveAtSwap(Idx);
		}
		OnInventoryChanged.Broadcast();
		return true;
	}

	return false;
}
bool UAI_REPlayerInventoryComponent::MoveItemSlot(int32 FromSlotIndex, int32 ToSlotIndex)
{
	if (FromSlotIndex == ToSlotIndex || !IsSlotIndexValid(FromSlotIndex) || !IsSlotIndexValid(ToSlotIndex))
		return false;

	int32 FromIdx = FindStackIndexBySlot(FromSlotIndex);
	if (FromIdx == INDEX_NONE) return false;

	int32 ToIdx = FindStackIndexBySlot(ToSlotIndex);
	if (ToIdx == INDEX_NONE)
	{
		Items[FromIdx].SlotIndex = ToSlotIndex;
		OnInventoryChanged.Broadcast();
		return true;
	}

	if (Items[FromIdx].ItemId == Items[ToIdx].ItemId)
	{
		const int32 MaxStack = GetMaxStackForItem(Items[FromIdx].ItemId);
		const int32 MoveCount = FMath::Min(Items[FromIdx].Count, MaxStack - Items[ToIdx].Count);
		if (MoveCount > 0)
		{
			Items[ToIdx].Count += MoveCount;
			Items[FromIdx].Count -= MoveCount;
			if (Items[FromIdx].Count <= 0)
			{
				Items.RemoveAtSwap(FromIdx);
			}
			OnInventoryChanged.Broadcast();
			return true;
		}
	}

	Swap(Items[FromIdx].SlotIndex, Items[ToIdx].SlotIndex);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UAI_REPlayerInventoryComponent::DropItemFromSlot(int32 SlotIndex, int32 Count)
{
	int32 Idx = FindStackIndexBySlot(SlotIndex);
	if (Idx == INDEX_NONE || Items[Idx].ItemId.IsNone() || Items[Idx].Count <= 0) return false;

	const int32 RemoveCount = Count <= 0 ? Items[Idx].Count : FMath::Min(Count, Items[Idx].Count);
	Items[Idx].Count -= RemoveCount;
	
	if (Items[Idx].Count <= 0)
	{
		Items.RemoveAtSwap(Idx);
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

int32 UAI_REPlayerInventoryComponent::FindStackIndexBySlot(int32 SlotIndex) const
{
	return Items.IndexOfByPredicate([SlotIndex](const FInventoryItemStack& Stack) { return Stack.SlotIndex == SlotIndex; });
}

int32 UAI_REPlayerInventoryComponent::FindFirstEmptySlotIndex() const
{
	for (int32 i = 0; i < MaxSlots; ++i)
	{
		if (FindStackIndexBySlot(i) == INDEX_NONE) return i;
	}
	return INDEX_NONE;
}

int32 UAI_REPlayerInventoryComponent::GetMaxStackForItem(FName ItemId) const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UAI_REItemSubsystem* Subsystem = GI->GetSubsystem<UAI_REItemSubsystem>())
			{
				if (UAI_REItemDataAsset* DA = Subsystem->GetItemDataAsset(ItemId))
				{
					return FMath::Max(1, DA->MaxStackSize);
				}
			}
		}
	}
	return 99;
}

// Copyright MixUpProject. All Rights Reserved.

#include "AI_REPlayerInventoryComponent.h"
#include "AIREGameplayInventorySubsystem.h"
#include "AI_REPlayerCombatComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "AI_REItemSubsystem.h"
#include "AI_REItemDataAsset.h"
#include "AI_REItemEffect.h"
#include "AI_RECharacterBase.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

UAI_REPlayerInventoryComponent::UAI_REPlayerInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAI_REPlayerInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	bPersistenceReadyForGameplay = false;
	RegisterWithGameplayInventory();
}

void UAI_REPlayerInventoryComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UAIREGameplayInventorySubsystem* InventorySubsystem =
				GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>())
			{
				InventorySubsystem->UnregisterPlayerInventory(this);
			}
		}
	}
	bPersistenceReadyForGameplay = false;
	Super::EndPlay(EndPlayReason);
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
	if (!bPersistenceReadyForGameplay || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

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

	if (RemainingCount < Count)
	{
		++Revision;
		NotifyPersistenceMutation();
	}
	OnInventoryChanged.Broadcast();
	return RemainingCount <= 0;
}

bool UAI_REPlayerInventoryComponent::BuildExactAddState(
	const FName ItemId,
	const int32 Count,
	TArray<FInventoryItemStack>& OutItems) const
{
	OutItems.Reset();
	if (ItemId.IsNone() || Count <= 0 || MaxSlots <= 0)
	{
		return false;
	}

	UGameInstance* GameInstance = GetWorld()
		? GetWorld()->GetGameInstance()
		: nullptr;
	UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
		: nullptr;
	UAI_REItemDataAsset* ItemData = IsValid(ItemSubsystem)
		? ItemSubsystem->GetItemDataAsset(ItemId)
		: nullptr;
	int32 MaxStackSize = IsValid(ItemData) ? ItemData->MaxStackSize : 0;
#if WITH_DEV_AUTOMATION_TESTS
	if (GIsAutomationTesting
		&& MaxStackSize < 1
		&& (ItemId == FName(TEXT("AIRE.Test.Stack2"))
			|| ItemId == FName(TEXT("AIRE.Test.Stack4"))))
	{
		MaxStackSize = ItemId == FName(TEXT("AIRE.Test.Stack2")) ? 2 : 4;
	}
#endif
	if (MaxStackSize < 1)
	{
		return false;
	}

	int64 FreeCapacity = 0;
	for (const FInventoryItemStack& Stack : Items)
	{
		if (Stack.SlotIndex >= 0
			&& Stack.SlotIndex < MaxSlots
			&& Stack.ItemId == ItemId)
		{
			FreeCapacity += FMath::Max(0, MaxStackSize - Stack.Count);
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < MaxSlots; ++SlotIndex)
	{
		const bool bIsOccupied = Items.ContainsByPredicate(
			[SlotIndex](const FInventoryItemStack& Stack)
			{
				return Stack.SlotIndex == SlotIndex;
			});
		if (!bIsOccupied)
		{
			FreeCapacity += static_cast<int64>(MaxStackSize);
		}
	}

	if (FreeCapacity < Count)
	{
		return false;
	}

	OutItems = Items;
	int32 RemainingCount = Count;
	for (FInventoryItemStack& Stack : OutItems)
	{
		if (RemainingCount == 0
			|| Stack.SlotIndex < 0
			|| Stack.SlotIndex >= MaxSlots
			|| Stack.ItemId != ItemId)
		{
			continue;
		}

		const int32 AddedCount = FMath::Min(
			RemainingCount,
			FMath::Max(0, MaxStackSize - Stack.Count));
		Stack.Count += AddedCount;
		RemainingCount -= AddedCount;
	}

	for (int32 SlotIndex = 0; SlotIndex < MaxSlots && RemainingCount > 0; ++SlotIndex)
	{
		const bool bIsOccupied = OutItems.ContainsByPredicate(
			[SlotIndex](const FInventoryItemStack& Stack)
			{
				return Stack.SlotIndex == SlotIndex;
			});
		if (bIsOccupied)
		{
			continue;
		}

		FInventoryItemStack& NewStack = OutItems.AddDefaulted_GetRef();
		NewStack.SlotIndex = SlotIndex;
		NewStack.ItemId = ItemId;
		NewStack.Count = FMath::Min(RemainingCount, MaxStackSize);
		RemainingCount -= NewStack.Count;
	}

	if (RemainingCount != 0)
	{
		OutItems.Reset();
		return false;
	}

	return true;
}

bool UAI_REPlayerInventoryComponent::BuildExactRemoveFromSlotState(
	const int32 SlotIndex,
	const int32 Count,
	TArray<FInventoryItemStack>& OutItems,
	FName& OutItemId) const
{
	OutItems.Reset();
	OutItemId = NAME_None;
	if (SlotIndex < 0 || SlotIndex >= MaxSlots || Count <= 0)
	{
		return false;
	}

	const int32 StackIndex = FindStackIndexBySlot(SlotIndex);
	if (StackIndex == INDEX_NONE)
	{
		return false;
	}

	const FInventoryItemStack& SourceStack = Items[StackIndex];
	if (SourceStack.ItemId.IsNone() || SourceStack.Count < Count)
	{
		return false;
	}

	UGameInstance* GameInstance = GetWorld()
		? GetWorld()->GetGameInstance()
		: nullptr;
	UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
		: nullptr;
	UAI_REItemDataAsset* ItemData = IsValid(ItemSubsystem)
		? ItemSubsystem->GetItemDataAsset(SourceStack.ItemId)
		: nullptr;
	int32 MaxStackSize = IsValid(ItemData) ? ItemData->MaxStackSize : 0;
#if WITH_DEV_AUTOMATION_TESTS
	if (GIsAutomationTesting
		&& MaxStackSize < 1
		&& (SourceStack.ItemId == FName(TEXT("AIRE.Test.Stack2"))
			|| SourceStack.ItemId == FName(TEXT("AIRE.Test.Stack4"))))
	{
		MaxStackSize = SourceStack.ItemId == FName(TEXT("AIRE.Test.Stack2"))
			? 2
			: 4;
	}
#endif
	if (MaxStackSize < 1)
	{
		return false;
	}

	OutItems = Items;
	OutItemId = SourceStack.ItemId;
	FInventoryItemStack& NewSourceStack = OutItems[StackIndex];
	NewSourceStack.Count -= Count;
	if (NewSourceStack.Count == 0)
	{
		OutItems.RemoveAt(StackIndex);
	}

	return true;
}

void UAI_REPlayerInventoryComponent::CommitExactInventoryState(
	TArray<FInventoryItemStack>&& NewItems)
{
	Items = MoveTemp(NewItems);
	++Revision;
}

void UAI_REPlayerInventoryComponent::CommitExactInventoryAndEquipmentState(
	TArray<FInventoryItemStack>&& NewItems,
	const FName NewEquippedWeaponItemId)
{
	Items = MoveTemp(NewItems);
	EquippedWeaponItemId = NewEquippedWeaponItemId;
	++Revision;
}

void UAI_REPlayerInventoryComponent::NotifyExactInventoryMutation()
{
	NotifyPersistenceMutation();
	OnInventoryChanged.Broadcast();
}

void UAI_REPlayerInventoryComponent::NotifyWeaponEquipResult(
	const FName WeaponItemId,
	const bool bSucceeded)
{
	OnWeaponEquipResult.Broadcast(WeaponItemId, bSucceeded);
}

bool UAI_REPlayerInventoryComponent::ConsumeItem(FName ItemId, int32 Count)
{
	if (!bPersistenceReadyForGameplay || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}
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

	++Revision;
	NotifyPersistenceMutation();
	OnInventoryChanged.Broadcast();
	return true;
}

bool UAI_REPlayerInventoryComponent::UseItem(int32 SlotIndex)
{
	if (!bPersistenceReadyForGameplay)
	{
		return false;
	}
	
	// 장착 슬롯(EquipmentSlot)에서 직접 해제를 시도한 경우 (SlotIndex가 INDEX_NONE)
	if (SlotIndex == INDEX_NONE)
	{
		if (!EquippedWeaponItemId.IsNone())
		{
			AAI_RECharacterBase* OwnerChar = Cast<AAI_RECharacterBase>(GetOwner());
			UAI_REPlayerCombatComponent* CombatComp = OwnerChar ? OwnerChar->FindComponentByClass<UAI_REPlayerCombatComponent>() : nullptr;
			if (CombatComp)
			{
				CombatComp->UnequipWeapon();
				FName UnequippedId = EquippedWeaponItemId;
				EquippedWeaponItemId = NAME_None;
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[Item Usage] 장착 해제!"));
				NotifyWeaponEquipResult(UnequippedId, false);
				++Revision;
				NotifyPersistenceMutation();
				OnInventoryChanged.Broadcast();
				return true;
			}
		}
		return false;
	}

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
					else if (DA->ItemType == EAI_REItemType::Weapon)
					{
						AAI_RECharacterBase* OwnerChar = Cast<AAI_RECharacterBase>(GetOwner());
						UAI_REPlayerCombatComponent* CombatComp = OwnerChar ? OwnerChar->FindComponentByClass<UAI_REPlayerCombatComponent>() : nullptr;
						
						if (CombatComp)
						{
							// Is it already equipped?
							if (EquippedWeaponItemId == Items[Idx].ItemId)
							{
								CombatComp->UnequipWeapon();
								EquippedWeaponItemId = NAME_None;
								GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, FString::Printf(TEXT("[Item Usage] %s 장착 해제!"), *DA->DisplayName.ToString()));
								NotifyWeaponEquipResult(Items[Idx].ItemId, false); // Broadcast unequip
							}
							else
							{
								if (CombatComp->TryEquipWeapon(DA))
								{
									EquippedWeaponItemId = Items[Idx].ItemId;
									GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, FString::Printf(TEXT("[Item Usage] %s 장착 완료!"), *DA->DisplayName.ToString()));
									NotifyWeaponEquipResult(Items[Idx].ItemId, true); // Broadcast equip
								}
							}
							
							// 무기는 소모되지 않으므로 bConsumed = false 유지
							// 상태 변경을 저장하기 위해 Revision 갱신 필요
							++Revision;
							NotifyPersistenceMutation();
							OnInventoryChanged.Broadcast();
							return true; // 무기 처리가 완료되었으므로 종료
						}
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
		++Revision;
		NotifyPersistenceMutation();
		OnInventoryChanged.Broadcast();
		return true;
	}

	return false;
}
bool UAI_REPlayerInventoryComponent::MoveItemSlot(int32 FromSlotIndex, int32 ToSlotIndex)
{
	if (!bPersistenceReadyForGameplay
		|| FromSlotIndex == ToSlotIndex
		|| !IsSlotIndexValid(FromSlotIndex)
		|| !IsSlotIndexValid(ToSlotIndex))
		return false;

	int32 FromIdx = FindStackIndexBySlot(FromSlotIndex);
	if (FromIdx == INDEX_NONE) return false;

	int32 ToIdx = FindStackIndexBySlot(ToSlotIndex);
	if (ToIdx == INDEX_NONE)
	{
		Items[FromIdx].SlotIndex = ToSlotIndex;
		++Revision;
		NotifyPersistenceMutation();
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
			++Revision;
			NotifyPersistenceMutation();
			OnInventoryChanged.Broadcast();
			return true;
		}
	}

	Swap(Items[FromIdx].SlotIndex, Items[ToIdx].SlotIndex);
	++Revision;
	NotifyPersistenceMutation();
	OnInventoryChanged.Broadcast();
	return true;
}

bool UAI_REPlayerInventoryComponent::DropItemFromSlot(int32 SlotIndex, int32 Count)
{
	if (!bPersistenceReadyForGameplay)
	{
		return false;
	}
	int32 Idx = FindStackIndexBySlot(SlotIndex);
	if (Idx == INDEX_NONE || Items[Idx].ItemId.IsNone() || Items[Idx].Count <= 0) return false;

	const int32 RemoveCount = Count <= 0 ? Items[Idx].Count : FMath::Min(Count, Items[Idx].Count);
	Items[Idx].Count -= RemoveCount;
	
	if (Items[Idx].Count <= 0)
	{
		Items.RemoveAtSwap(Idx);
	}

	// TODO: Spawn physical item in the world.

	++Revision;
	NotifyPersistenceMutation();
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
	// 100 ~ 109: 퀵슬롯
	return (SlotIndex >= 0 && SlotIndex < MaxSlots) || (SlotIndex >= 100 && SlotIndex < 110);
}

int64 UAI_REPlayerInventoryComponent::GetInventoryRevision() const
{
	return Revision;
}

FName UAI_REPlayerInventoryComponent::GetEquippedWeaponItemId() const
{
	return EquippedWeaponItemId;
}

void UAI_REPlayerInventoryComponent::RegisterWithGameplayInventory()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World)
		? World->GetGameInstance()
		: nullptr;
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
		: nullptr;
	UAI_REPlayerCombatComponent* CombatComponent = IsValid(GetOwner())
		? GetOwner()->FindComponentByClass<UAI_REPlayerCombatComponent>()
		: nullptr;
	if (!IsValid(InventorySubsystem)
		|| !InventorySubsystem->RegisterPlayerInventory(
			this,
			CombatComponent))
	{
		bPersistenceReadyForGameplay = true;
	}
}

void UAI_REPlayerInventoryComponent::NotifyPersistenceMutation()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World)
		? World->GetGameInstance()
		: nullptr;
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
		: nullptr;
	if (IsValid(InventorySubsystem))
	{
		InventorySubsystem->NotifyPlayerInventoryChanged(this);
	}
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

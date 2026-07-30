#include "Inventory/AIRECompanionInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AI_REItemSubsystem.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"
#include "Engine/GameInstance.h"

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

	MaxInventorySlots = CompanionConfig->MaxInventorySlots;
	EquipmentComponent = InEquipmentComponent;
	AbilitySystem = InAbilitySystem;
	WeaponEquipCompletedDelegateHandle = EquipmentComponent
		->OnWeaponEquipCompleted()
		.AddUObject(
			this,
			&UAIRECompanionInventoryComponent::HandleWeaponEquipCompleted);
	bIsInitialized = true;

	for (const FAIRECompanionInitialInventoryEntry& Entry
		: CompanionConfig->InitialInventory)
	{
		if (!TryAddItemDefinition(Entry.ItemDefinition, Entry.Count))
		{
			UE_LOG(
				LogAIRECompanionInventory,
				Error,
				TEXT("Companion initial inventory failed. Companion=%s Item=%s Count=%d"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Entry.ItemDefinition),
				Entry.Count);
			ShutdownInventory();
			return false;
		}
	}

	if (!CompanionConfig->DefaultEquippedWeaponItemId.IsNone()
		&& !EquipWeaponItem(CompanionConfig->DefaultEquippedWeaponItemId))
	{
		UE_LOG(
			LogAIRECompanionInventory,
			Error,
			TEXT("Companion default inventory weapon was rejected. Companion=%s ItemId=%s"),
			*GetNameSafe(GetOwner()),
			*CompanionConfig->DefaultEquippedWeaponItemId.ToString());
		ShutdownInventory();
		return false;
	}

	return true;
}

void UAIRECompanionInventoryComponent::ShutdownInventory()
{
	if (!PendingWeaponItemId.IsNone()
		&& EquipmentComponent.IsValid())
	{
		EquipmentComponent->UnequipCurrentWeapon();
	}

	if (WeaponEquipCompletedDelegateHandle.IsValid()
		&& EquipmentComponent.IsValid())
	{
		EquipmentComponent->OnWeaponEquipCompleted().Remove(
			WeaponEquipCompletedDelegateHandle);
	}
	WeaponEquipCompletedDelegateHandle.Reset();

	ItemStacks.Reset();
	KnownItemDefinitions.Reset();
	MaxInventorySlots = 0;
	EquippedWeaponItemId = NAME_None;
	PendingWeaponItemId = NAME_None;
	PreviousWeaponItemId = NAME_None;
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
	int32 TotalCount = 0;
	for (const FAIRECompanionInventoryStack& Stack : ItemStacks)
	{
		if (IsValid(Stack.ItemDefinition)
			&& Stack.ItemDefinition->ItemId == ItemId)
		{
			TotalCount += Stack.Count;
		}
	}
	return TotalCount;
}

bool UAIRECompanionInventoryComponent::TryAddItem(
	const FName ItemId,
	const int32 Count)
{
	if (!bIsInitialized || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

	UAIRECompanionItemDefinitionDataAsset* ItemDefinition =
		const_cast<UAIRECompanionItemDefinitionDataAsset*>(
			FindItemDefinition(ItemId));
	if (!IsValid(ItemDefinition))
	{
		UGameInstance* GameInstance = GetWorld()
			? GetWorld()->GetGameInstance()
			: nullptr;
		UAI_REItemSubsystem* ItemSubsystem = IsValid(GameInstance)
			? GameInstance->GetSubsystem<UAI_REItemSubsystem>()
			: nullptr;
		ItemDefinition = IsValid(ItemSubsystem)
			? Cast<UAIRECompanionItemDefinitionDataAsset>(
				ItemSubsystem->GetItemDataAsset(ItemId))
			: nullptr;
	}
	return TryAddItemDefinition(ItemDefinition, Count);
}

bool UAIRECompanionInventoryComponent::TryAddItemDefinition(
	UAIRECompanionItemDefinitionDataAsset* ItemDefinition,
	const int32 Count)
{
	if (!bIsInitialized || !IsValid(ItemDefinition) || Count <= 0)
	{
		return false;
	}

	FText ValidationError;
	if (!ItemDefinition->IsCompanionItemDefinitionValid(ValidationError))
	{
		return false;
	}
	KnownItemDefinitions.AddUnique(ItemDefinition);

	const int32 MaxStackSize = ItemDefinition->MaxStackSize;
	int32 FreeCapacity = 0;
	for (const FAIRECompanionInventoryStack& Stack : ItemStacks)
	{
		if (Stack.ItemDefinition == ItemDefinition)
		{
			FreeCapacity += FMath::Max(0, MaxStackSize - Stack.Count);
		}
	}
	FreeCapacity += FMath::Max(
		0,
		MaxInventorySlots - ItemStacks.Num()) * MaxStackSize;
	if (FreeCapacity < Count)
	{
		return false;
	}

	int32 RemainingCount = Count;
	for (FAIRECompanionInventoryStack& Stack : ItemStacks)
	{
		if (Stack.ItemDefinition != ItemDefinition
			|| Stack.Count >= MaxStackSize)
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
		FAIRECompanionInventoryStack& NewStack =
			ItemStacks.AddDefaulted_GetRef();
		NewStack.ItemDefinition = ItemDefinition;
		NewStack.Count = FMath::Min(RemainingCount, MaxStackSize);
		RemainingCount -= NewStack.Count;
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UAIRECompanionInventoryComponent::TryConsumeItem(
	const FName ItemId,
	const int32 Count)
{
	if (!bIsInitialized
		|| ItemId.IsNone()
		|| Count <= 0
		|| ItemId == EquippedWeaponItemId
		|| GetItemCount(ItemId) < Count)
	{
		return false;
	}

	int32 RemainingCount = Count;
	for (int32 Index = ItemStacks.Num() - 1;
		Index >= 0 && RemainingCount > 0;
		--Index)
	{
		FAIRECompanionInventoryStack& Stack = ItemStacks[Index];
		if (!IsValid(Stack.ItemDefinition)
			|| Stack.ItemDefinition->ItemId != ItemId)
		{
			continue;
		}

		const int32 RemovedCount = FMath::Min(
			RemainingCount,
			Stack.Count);
		Stack.Count -= RemovedCount;
		RemainingCount -= RemovedCount;
		if (Stack.Count == 0)
		{
			ItemStacks.RemoveAt(Index);
		}
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UAIRECompanionInventoryComponent::EquipWeaponItem(
	const FName ItemId)
{
	return EquipWeaponItemInternal(ItemId, false);
}

bool UAIRECompanionInventoryComponent::EquipWeaponItemInternal(
	const FName ItemId,
	const bool bIsRecovery)
{
	if (!bIsInitialized
		|| !EquipmentComponent.IsValid()
		|| !AbilitySystem.IsValid()
		|| !PendingWeaponItemId.IsNone()
		|| !HasItem(ItemId))
	{
		return false;
	}

	if (!bIsRecovery
		&& AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttacking))
	{
		return false;
	}

	if (!bIsRecovery && ItemId == EquippedWeaponItemId)
	{
		return true;
	}

	const UAIRECompanionItemDefinitionDataAsset* ItemDefinition =
		FindItemDefinition(ItemId);
	if (!IsValid(ItemDefinition)
		|| ItemDefinition->ItemType != EAI_REItemType::Weapon
		|| !IsValid(ItemDefinition->WeaponDefinition))
	{
		return false;
	}

	PreviousWeaponItemId = bIsRecovery
		? NAME_None
		: EquippedWeaponItemId;
	PendingWeaponItemId = ItemId;
	if (!EquipmentComponent->EquipWeapon(
			ItemDefinition->WeaponDefinition))
	{
		if (PendingWeaponItemId == ItemId)
		{
			PendingWeaponItemId = NAME_None;
			PreviousWeaponItemId = NAME_None;
		}
		return false;
	}

	return true;
}

FName UAIRECompanionInventoryComponent::GetEquippedWeaponItemId() const
{
	return EquippedWeaponItemId;
}

const UAIRECompanionItemDefinitionDataAsset*
UAIRECompanionInventoryComponent::FindItemDefinition(
	const FName ItemId) const
{
	for (const FAIRECompanionInventoryStack& Stack : ItemStacks)
	{
		if (IsValid(Stack.ItemDefinition)
			&& Stack.ItemDefinition->ItemId == ItemId)
		{
			return Stack.ItemDefinition;
		}
	}
	for (const UAIRECompanionItemDefinitionDataAsset* ItemDefinition
		: KnownItemDefinitions)
	{
		if (IsValid(ItemDefinition)
			&& ItemDefinition->ItemId == ItemId)
		{
			return ItemDefinition;
		}
	}
	return nullptr;
}

void UAIRECompanionInventoryComponent::HandleWeaponEquipCompleted(
	UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
	const bool bSucceeded)
{
	if (PendingWeaponItemId.IsNone())
	{
		return;
	}

	const FName CompletedItemId = PendingWeaponItemId;
	const FName RecoveryItemId = PreviousWeaponItemId;
	const UAIRECompanionItemDefinitionDataAsset* PendingItemDefinition =
		FindItemDefinition(CompletedItemId);
	if (!IsValid(PendingItemDefinition)
		|| PendingItemDefinition->WeaponDefinition != WeaponDefinition)
	{
		return;
	}

	PendingWeaponItemId = NAME_None;
	PreviousWeaponItemId = NAME_None;
	if (bSucceeded)
	{
		EquippedWeaponItemId = CompletedItemId;
		OnWeaponEquipResult.Broadcast(CompletedItemId, true);
		return;
	}

	OnWeaponEquipResult.Broadcast(CompletedItemId, false);
	if (!RecoveryItemId.IsNone())
	{
		if (!EquipWeaponItemInternal(RecoveryItemId, true))
		{
			EquippedWeaponItemId = NAME_None;
			OnWeaponEquipResult.Broadcast(RecoveryItemId, false);
		}
	}
}

void UAIRECompanionInventoryComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownInventory();
	Super::EndPlay(EndPlayReason);
}

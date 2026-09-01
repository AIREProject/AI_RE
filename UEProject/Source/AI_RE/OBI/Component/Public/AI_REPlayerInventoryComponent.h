// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI_REPlayerInventoryComponent.generated.h"

class UAIREGameplayInventorySubsystem;
class UAIRECompanionTestingBlueprintLibrary;

USTRUCT(BlueprintType)
struct FInventoryItemStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SlotIndex = -1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ItemId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Count = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FPlayerWeaponEquipResultSignature,
	FName,
	WeaponItemId,
	bool,
	bSucceeded);

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAI_REPlayerInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAI_REPlayerInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FInventoryChangedSignature OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FPlayerWeaponEquipResultSignature OnWeaponEquipResult;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool HasItem(FName ItemId, int32 Amount = 1) const;
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SwapSlots(int32 SlotIndexA, int32 SlotIndexB);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseItem(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	bool TryUnequipWeapon(int32 DestinationSlotIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool MoveItemSlot(int32 FromSlotIndex, int32 ToSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool DropItemFromSlot(int32 SlotIndex, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsInventoryFull() const;
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsSlotIndexValid(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int64 GetInventoryRevision() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	FName GetEquippedWeaponItemId() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Persistence")
	bool IsPersistenceReadyForGameplay() const
	{
		return bPersistenceReadyForGameplay;
	}

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	int32 MaxSlots = 30;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FInventoryItemStack> Items;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	int32 FindStackIndexBySlot(int32 SlotIndex) const;
	int32 FindFirstEmptySlotIndex() const;
	int32 GetMaxStackForItem(FName ItemId) const;

private:
	friend class UAIREGameplayInventorySubsystem;
	friend class UAIRECompanionTestingBlueprintLibrary;
	friend class FAIREGameplayInventoryPersistenceTestAccess;

	bool BuildExactAddState(
		FName ItemId,
		int32 Count,
		TArray<FInventoryItemStack>& OutItems) const;
	bool BuildExactRemoveFromSlotState(
		int32 SlotIndex,
		int32 Count,
		TArray<FInventoryItemStack>& OutItems,
		FName& OutItemId) const;
	void CommitExactInventoryState(TArray<FInventoryItemStack>&& NewItems);
	void CommitExactInventoryAndEquipmentState(
		TArray<FInventoryItemStack>&& NewItems,
		FName NewEquippedWeaponItemId);
	void NotifyExactInventoryMutation();
	void NotifyWeaponEquipResult(FName WeaponItemId, bool bSucceeded);
	void RegisterWithGameplayInventory();
	void NotifyPersistenceMutation();

	int64 Revision = 0;
	FName EquippedWeaponItemId;
	bool bPersistenceReadyForGameplay = false;
};

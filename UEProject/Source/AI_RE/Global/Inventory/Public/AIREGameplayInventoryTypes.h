#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.generated.h"

namespace AIREGameplayInventory
{
	inline constexpr int32 MakoItemSlotCapacity = 20;
	inline constexpr int32 SharedStorageSlotCapacity = 50;
	inline constexpr int32 LocalImportFormatVersion = 1;
	inline constexpr int32 MaxStableIdLength = 128;
	inline constexpr TCHAR MakoContainerId[] = TEXT("AIRE.Inventory.MAKO");
	inline constexpr TCHAR SharedStorageContainerId[] = TEXT("AIRE.Inventory.SharedStorage");
	inline constexpr TCHAR LegacySharedStorageContainerId[] =
		TEXT("AIRE.Inventory.SharedWarehouse");
}

UENUM(BlueprintType)
enum class EAIREInventoryMutationCode : uint8
{
	Succeeded,
	AlreadyApplied,
	NotInitialized,
	InvalidSession,
	InvalidContainer,
	InvalidMutationId,
	InvalidOperation,
	InvalidItem,
	InvalidQuantity,
	InvalidSlot,
	InsufficientQuantity,
	CapacityExceeded,
	RevisionConflict,
	EquipmentBusy,
	EquipmentRequestRejected,
	UnsupportedImportFormat,
	ScopeMismatch,
	DuplicateOperation,
	RecoveryFailed
};

UENUM(BlueprintType)
enum class EAIREInventoryWorkResultDestination : uint8
{
	None,
	Mako,
	SharedStorage,
	WorldDrop
};

UENUM(BlueprintType)
enum class EAIREEquipmentTransitionState : uint8
{
	Idle,
	Equipping,
	Recovering,
	RecoveryFailed
};

UENUM(BlueprintType)
enum class EAIREPlayerStorageTransferDirection : uint8
{
	DepositPlayerToStorage,
	WithdrawStorageToPlayer
};

UENUM(BlueprintType)
enum class EAIREPlayerMakoTransferDirection : uint8
{
	PlayerToMako,
	MakoToPlayer
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryItemStackSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryEquipmentSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	FName EquippedItemId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	FName PendingItemId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	EAIREEquipmentTransitionState TransitionState =
		EAIREEquipmentTransitionState::Idle;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryContainerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	FName ContainerId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	int64 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	TArray<FAIREInventoryItemStackSnapshot> ItemStacks;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	FAIREInventoryEquipmentSnapshot Equipment;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryMutationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FName ContainerId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryMoveRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FName ContainerId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 DestinationSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryTransferRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FName SourceContainerId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FName DestinationContainerId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedSourceRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedDestinationRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREPlayerStorageTransferRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	EAIREPlayerStorageTransferDirection Direction =
		EAIREPlayerStorageTransferDirection::DepositPlayerToStorage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedStorageRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREPlayerMakoTransferRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	EAIREPlayerMakoTransferDirection Direction =
		EAIREPlayerMakoTransferDirection::PlayerToMako;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedPlayerRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedMakoRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREPlayerWeaponEquipRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedPlayerRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 SourceSlotIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryEquipRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 SourceSlotIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryMutationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	EAIREInventoryMutationCode Code =
		EAIREInventoryMutationCode::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	int64 SourceRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory")
	int64 DestinationRevision = INDEX_NONE;

	bool WasApplied() const
	{
		return Code == EAIREInventoryMutationCode::Succeeded
			|| Code == EAIREInventoryMutationCode::AlreadyApplied;
	}
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryItemQuantity
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREPlayerCraftRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Crafting")
	FGuid SessionId;

	/** MutationId makes completion idempotent; preflight never records it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Crafting")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Crafting")
	int64 ExpectedStorageRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Crafting")
	TArray<FAIREInventoryItemQuantity> Ingredients;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Crafting")
	FAIREInventoryItemQuantity Result;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREMakoCraftWorkRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	FGuid SessionId;

	/** WorkOrderId is the idempotent mutation identifier for this craft completion. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	FGuid WorkOrderId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	int64 ExpectedMakoRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	int64 ExpectedStorageRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	TArray<FAIREInventoryItemQuantity> Ingredients;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	FAIREInventoryItemQuantity Result;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	bool bCanWorldDrop = false;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREMakoWorkRewardRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	FGuid SessionId;

	/** DeliveryId is the idempotent mutation identifier for this harvested reward. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	FGuid DeliveryId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	int64 ExpectedMakoRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	int64 ExpectedStorageRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory|Work")
	FAIREInventoryItemQuantity Reward;

};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryWorkResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Work")
	EAIREInventoryMutationCode Code = EAIREInventoryMutationCode::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Work")
	bool bAlreadyApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Work")
	EAIREInventoryWorkResultDestination Destination =
		EAIREInventoryWorkResultDestination::None;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Work")
	FAIREInventoryItemQuantity DeliveredItem;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Work")
	int64 MakoRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Work")
	int64 StorageRevision = INDEX_NONE;
};

struct AI_RE_API FAIREInventorySessionScope
{
	FString ProfileId;
	FString SaveSlotId;
	FString CompanionId;

	bool operator==(const FAIREInventorySessionScope& Other) const
	{
		return ProfileId == Other.ProfileId
			&& SaveSlotId == Other.SaveSlotId
			&& CompanionId == Other.CompanionId;
	}
};

enum class EAIREInventoryImportOperationType : uint8
{
	Add,
	Remove
};

struct AI_RE_API FAIREInventoryImportOperation
{
	FString OperationId;
	EAIREInventoryImportOperationType Type =
		EAIREInventoryImportOperationType::Add;
	FName ItemId;
	int32 Count = 0;
};

struct AI_RE_API FAIREInventoryStartupImportCandidate
{
	int32 LocalFormatVersion =
		AIREGameplayInventory::LocalImportFormatVersion;
	FString CandidateId;
	FAIREInventorySessionScope Scope;
	FGuid SessionId;
	FName ContainerId;
	int64 BaseRevision = INDEX_NONE;
	TArray<FAIREInventoryImportOperation> Operations;
};

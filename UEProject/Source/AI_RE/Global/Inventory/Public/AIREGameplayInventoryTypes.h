#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.generated.h"

namespace AIREGameplayInventory
{
	inline constexpr int32 MakoItemSlotCapacity = 20;
	inline constexpr int32 SharedWarehouseSlotCapacity = 50;
	inline constexpr int32 LocalImportFormatVersion = 1;
	inline constexpr int32 MaxStableIdLength = 128;
	inline constexpr TCHAR MakoContainerId[] = TEXT("AIRE.Inventory.MAKO");
	inline constexpr TCHAR SharedWarehouseContainerId[] = TEXT("AIRE.Inventory.SharedWarehouse");
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
enum class EAIREEquipmentTransitionState : uint8
{
	Idle,
	Equipping,
	Recovering,
	RecoveryFailed
};

UENUM(BlueprintType)
enum class EAIREPlayerWarehouseTransferDirection : uint8
{
	DepositPlayerToWarehouse,
	WithdrawWarehouseToPlayer
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
struct AI_RE_API FAIREPlayerWarehouseTransferRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid SessionId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	FGuid MutationId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	EAIREPlayerWarehouseTransferDirection Direction =
		EAIREPlayerWarehouseTransferDirection::DepositPlayerToWarehouse;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int64 ExpectedWarehouseRevision = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AIRE|Inventory")
	int32 Count = 0;
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

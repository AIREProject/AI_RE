#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "AIREGameplayInventoryPersistenceTypes.generated.h"

namespace AIREGameplayInventoryPersistence
{
	inline constexpr int32 SaveFormatVersion = 2;
	// Increment manually when the persisted item catalog becomes incompatible.
	inline constexpr int32 ItemContentVersion = 1;
	inline constexpr int32 UserIndex = 0;
	inline constexpr int32 MaxLedgerEntries = 256;
	inline constexpr int32 PlayerInventoryCapacity = 30;
	inline constexpr int32 PlayerQuickSlotStart = 100;
	inline constexpr int32 PlayerQuickSlotCount = 10;
	inline constexpr TCHAR CanonicalProfileId[] = TEXT("AIRE_OPEN");
	inline constexpr TCHAR CanonicalSaveSlotId[] = TEXT("demo-slot-1");
	inline constexpr TCHAR CanonicalCompanionId[] = TEXT("mako");
	inline constexpr TCHAR PrimarySlotName[] =
		TEXT("AIRE.Inventory.Local.Primary");
	inline constexpr TCHAR PreviousSlotName[] =
		TEXT("AIRE.Inventory.Local.Previous");
}

UENUM(BlueprintType)
enum class EAIREInventoryPersistenceOperation : uint8
{
	None,
	Load,
	Save,
	Delete
};

UENUM(BlueprintType)
enum class EAIREInventoryPersistenceResultCode : uint8
{
	NotStarted,
	InProgress,
	Succeeded,
	SucceededWithFallback,
	FreshStateSeeded,
	SafeEmptyNoValidSave,
	Coalesced,
	DeferredEquipmentTransition,
	DeferredPlayerRegistration,
	NoChanges,
	UnsupportedFormatVersion,
	UnsupportedContentVersion,
	ScopeMismatch,
	InvalidGeneration,
	InvalidSession,
	InvalidPayload,
	InvalidContainer,
	InvalidItem,
	InvalidLedger,
	IoFailure,
	Superseded,
	ShuttingDown
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInventoryPersistenceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Persistence")
	EAIREInventoryPersistenceOperation Operation =
		EAIREInventoryPersistenceOperation::None;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Persistence")
	EAIREInventoryPersistenceResultCode Code =
		EAIREInventoryPersistenceResultCode::NotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Persistence")
	int64 Generation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Inventory|Persistence")
	bool bUsedFallback = false;
};

USTRUCT()
struct AI_RE_API FAIREInventoryPersistedStack
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(SaveGame)
	FName ItemId;

	UPROPERTY(SaveGame)
	int32 Count = 0;
};

USTRUCT()
struct AI_RE_API FAIREInventoryPersistedEquipment
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName EquippedItemId;
};

USTRUCT()
struct AI_RE_API FAIREInventoryPersistedContainer
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName ContainerId;

	UPROPERTY(SaveGame)
	int32 Capacity = 0;

	UPROPERTY(SaveGame)
	int64 Revision = 0;

	UPROPERTY(SaveGame)
	TArray<FAIREInventoryPersistedStack> ItemStacks;

	UPROPERTY(SaveGame)
	FAIREInventoryPersistedEquipment Equipment;
};

USTRUCT()
struct AI_RE_API FAIREInventoryPersistedPlayerState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 InventoryCapacity = 0;

	UPROPERTY(SaveGame)
	int64 Revision = 0;

	UPROPERTY(SaveGame)
	TArray<FAIREInventoryPersistedStack> ItemStacks;

	UPROPERTY(SaveGame)
	FAIREInventoryPersistedEquipment Equipment;
};

USTRUCT()
struct AI_RE_API FAIREInventoryPersistedMutationEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid MutationId;

	UPROPERTY(SaveGame)
	EAIREInventoryMutationCode Code =
		EAIREInventoryMutationCode::NotInitialized;

	UPROPERTY(SaveGame)
	int64 SourceRevision = INDEX_NONE;

	UPROPERTY(SaveGame)
	int64 DestinationRevision = INDEX_NONE;
};

USTRUCT()
struct AI_RE_API FAIREInventoryPersistedWorkEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid OperationId;

	UPROPERTY(SaveGame)
	EAIREInventoryMutationCode Code =
		EAIREInventoryMutationCode::NotInitialized;

	UPROPERTY(SaveGame)
	EAIREInventoryWorkResultDestination Destination =
		EAIREInventoryWorkResultDestination::None;

	UPROPERTY(SaveGame)
	FName DeliveredItemId;

	UPROPERTY(SaveGame)
	int32 DeliveredItemCount = 0;

	UPROPERTY(SaveGame)
	int64 MakoRevision = INDEX_NONE;

	UPROPERTY(SaveGame)
	int64 StorageRevision = INDEX_NONE;
};

USTRUCT()
struct AI_RE_API FAIREInventoryPersistedOfflineTaskEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString TaskId;
};

USTRUCT()
struct AI_RE_API FAIREInventorySaveEnvelope
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 FormatVersion =
		AIREGameplayInventoryPersistence::SaveFormatVersion;

	UPROPERTY(SaveGame)
	int32 ContentVersion =
		AIREGameplayInventoryPersistence::ItemContentVersion;

	UPROPERTY(SaveGame)
	int64 Generation = 0;

	UPROPERTY(SaveGame)
	FString ProfileId;

	UPROPERTY(SaveGame)
	FString SaveSlotId;

	UPROPERTY(SaveGame)
	FString CompanionId;

	UPROPERTY(SaveGame)
	FGuid SourceSessionId;

	UPROPERTY(SaveGame)
	TArray<FAIREInventoryPersistedContainer> Containers;

	UPROPERTY(SaveGame)
	FAIREInventoryPersistedPlayerState Player;

	UPROPERTY(SaveGame)
	TArray<FAIREInventoryPersistedMutationEntry> Mutations;

	UPROPERTY(SaveGame)
	TArray<FAIREInventoryPersistedWorkEntry> WorkResults;

	UPROPERTY(SaveGame)
	TArray<FAIREInventoryPersistedOfflineTaskEntry> OfflineTasks;

	UPROPERTY(SaveGame)
	TArray<FString> ImportCandidateIds;

	UPROPERTY(SaveGame)
	TArray<FString> ImportOperationIds;
};

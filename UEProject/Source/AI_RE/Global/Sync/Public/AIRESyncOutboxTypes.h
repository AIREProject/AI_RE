#pragma once

#include "CoreMinimal.h"
#include "AIRESyncOutboxTypes.generated.h"

namespace AIRESyncOutbox
{
	inline constexpr int32 SaveFormatVersion = 1;
	inline constexpr int32 UserIndex = 0;
	inline constexpr int32 MaxEntries = 128;
	inline constexpr int32 MaxPersistedBytes = 2 * 1024 * 1024;
	inline constexpr int32 MaxBodyBytes = 256 * 1024;
	inline constexpr int32 MaxCoalescingKeyLength = 128;
	inline constexpr float AttemptTimeoutSeconds = 35.0f;
	inline constexpr TCHAR CanonicalProfileId[] = TEXT("AIRE_OPEN");
	inline constexpr TCHAR CanonicalSaveSlotId[] = TEXT("demo-slot-1");
	inline constexpr TCHAR CanonicalCompanionId[] = TEXT("mako");
	inline constexpr TCHAR PrimarySlotName[] = TEXT("AIRE.SyncOutbox.Primary");
	inline constexpr TCHAR PreviousSlotName[] = TEXT("AIRE.SyncOutbox.Previous");
}

UENUM(BlueprintType)
enum class EAIRESyncOutboxOperationKind : uint8
{
	Snapshot,
	Event
};

UENUM(BlueprintType)
enum class EAIRESyncOutboxEntryState : uint8
{
	Pending,
	InFlight,
	Acked
};

UENUM(BlueprintType)
enum class EAIRESyncOutboxEnqueueCode : uint8
{
	AcceptedPendingPersistence,
	CoalescedPendingPersistence,
	Duplicate,
	Conflict,
	NotReady,
	ShuttingDown,
	InvalidSchemaVersion,
	InvalidOperationKind,
	InvalidOperationId,
	InvalidScope,
	InvalidBody,
	InvalidCoalescingKey,
	PayloadTooLarge,
	QueueFull
};

UENUM(BlueprintType)
enum class EAIRESyncOutboxPersistenceCode : uint8
{
	NotStarted,
	InProgress,
	Succeeded,
	SucceededWithFallback,
	SafeEmptyNoValidSave,
	NoChanges,
	IoFailure,
	InvalidEnvelope,
	ShuttingDown
};

UENUM(BlueprintType)
enum class EAIRESyncOutboxDispatchCode : uint8
{
	StartedPersistence,
	NotReady,
	ShuttingDown,
	PersistencePending,
	NoTransport,
	AlreadyInFlight,
	QueueEmpty
};

UENUM(BlueprintType)
enum class EAIRESyncOutboxCancelCode : uint8
{
	RemovedPending,
	ReturnedInFlightToPending,
	NotFound,
	NotReady,
	ShuttingDown
};

UENUM()
enum class EAIRESyncOutboxTransportResultCode : uint8
{
	Acked,
	Failed,
	Cancelled
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRESyncOutboxScope
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	FString ProfileId;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	FString SaveSlotId;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	FString CompanionId;

	bool operator==(const FAIRESyncOutboxScope& Other) const
	{
		return ProfileId == Other.ProfileId
			&& SaveSlotId == Other.SaveSlotId
			&& CompanionId == Other.CompanionId;
	}

	bool operator!=(const FAIRESyncOutboxScope& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRESyncOutboxEnqueueRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	int32 SchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	EAIRESyncOutboxOperationKind Kind = EAIRESyncOutboxOperationKind::Snapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	FGuid OperationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	FAIRESyncOutboxScope Scope;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	FString CoalescingKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Sync|Outbox")
	TArray<uint8> Body;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRESyncOutboxEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	int32 SchemaVersion = 1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	EAIRESyncOutboxOperationKind Kind = EAIRESyncOutboxOperationKind::Snapshot;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FGuid OperationId;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FString BodyHash;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FAIRESyncOutboxScope Scope;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FString CoalescingKey;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	TArray<uint8> Body;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	int64 Sequence = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	EAIRESyncOutboxEntryState State = EAIRESyncOutboxEntryState::Pending;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	int32 AttemptCount = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FDateTime CreatedAtUtc;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FDateTime LastAttemptAtUtc;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRESyncOutboxEnqueueResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	EAIRESyncOutboxEnqueueCode Code = EAIRESyncOutboxEnqueueCode::NotReady;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FGuid OperationId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FGuid SupersededOperationId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	FString BodyHash;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	EAIRESyncOutboxEntryState ExistingState = EAIRESyncOutboxEntryState::Pending;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRESyncOutboxPersistenceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	EAIRESyncOutboxPersistenceCode Code = EAIRESyncOutboxPersistenceCode::NotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	int64 Generation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Sync|Outbox")
	bool bUsedFallback = false;
};

USTRUCT()
struct AI_RE_API FAIRESyncOutboxTransportAttempt
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid AttemptToken;

	UPROPERTY()
	FAIRESyncOutboxEntry Entry;
};

USTRUCT()
struct AI_RE_API FAIRESyncOutboxTransportResult
{
	GENERATED_BODY()

	UPROPERTY()
	EAIRESyncOutboxTransportResultCode Code =
		EAIRESyncOutboxTransportResultCode::Failed;

	UPROPERTY()
	FGuid AttemptToken;

	UPROPERTY()
	FGuid OperationId;

	UPROPERTY()
	FString BodyHash;
};

USTRUCT()
struct AI_RE_API FAIRESyncOutboxSaveEnvelope
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 FormatVersion = AIRESyncOutbox::SaveFormatVersion;

	UPROPERTY(SaveGame)
	int64 Generation = 0;

	UPROPERTY(SaveGame)
	FAIRESyncOutboxScope Scope;

	UPROPERTY(SaveGame)
	TArray<FAIRESyncOutboxEntry> Entries;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FAIRESyncOutboxPersistenceCompleted,
	const FAIRESyncOutboxPersistenceResult&);

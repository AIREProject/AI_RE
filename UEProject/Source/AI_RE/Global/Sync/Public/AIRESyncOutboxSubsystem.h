#pragma once

#include "CoreMinimal.h"
#include "AIRESyncOutboxTransport.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIRESyncOutboxSubsystem.generated.h"

class UAIRESyncOutboxSaveGame;
class USaveGame;

UCLASS()
class AI_RE_API UAIRESyncOutboxSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Sync|Outbox")
	bool IsReady() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Sync|Outbox")
	int32 GetEntryCount() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Sync|Outbox")
	TArray<FAIRESyncOutboxEntry> GetEntries() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Sync|Outbox")
	FAIRESyncOutboxPersistenceResult GetLastPersistenceResult() const;

	FAIRESyncOutboxEnqueueResult Enqueue(
		const FAIRESyncOutboxEnqueueRequest& Request);
	FAIRESyncOutboxPersistenceResult RequestPersistence();
	EAIRESyncOutboxDispatchCode DispatchNext();
	EAIRESyncOutboxCancelCode CancelOperation(const FGuid& OperationId);
	void FlushBestEffort();

	void SetTransport(TSharedPtr<IAIRESyncOutboxTransport> InTransport);
	FAIRESyncOutboxPersistenceCompleted& OnPersistenceCompleted();

private:
	friend class FAIRESyncOutboxTestAccess;

	struct FLoadSlotState
	{
		FString SlotName;
		bool bCompleted = false;
		bool bExists = false;
		bool bIoFailure = false;
		TOptional<FAIRESyncOutboxSaveEnvelope> LoadedEnvelope;
	};

	static FAIRESyncOutboxScope MakeCanonicalScope();
	static bool IsCanonicalScope(const FAIRESyncOutboxScope& Scope);
	static bool IsValidUtf8(const TArray<uint8>& Bytes);
	static FString ComputeBodyHash(const TArray<uint8>& Body);
	static bool IsValidBodyHash(const FString& BodyHash);
	static bool IsValidCoalescingKey(const FString& Key);

	void BeginLoad();
	void BeginLoadSlot(int32 SlotIndex, uint64 LoadEpoch);
	void HandleSlotExistence(
		int32 SlotIndex,
		uint64 LoadEpoch,
		bool bExists,
		bool bIoFailure);
	void HandleSlotLoaded(
		int32 SlotIndex,
		uint64 LoadEpoch,
		USaveGame* LoadedGame);
	void FinalizeLoad(uint64 LoadEpoch);
	bool ValidateEnvelope(
		const FAIRESyncOutboxSaveEnvelope& Envelope,
		FAIRESyncOutboxSaveEnvelope& OutNormalized,
		bool& bOutNeedsCompact) const;
	bool DoesEnvelopeFit(const FAIRESyncOutboxSaveEnvelope& Envelope) const;
	FAIRESyncOutboxSaveEnvelope BuildEnvelope(int64 Generation) const;
	FAIRESyncOutboxPersistenceResult StartPersistence();
	void HandleSaveCompleted(
		const FString& SlotName,
		uint64 SaveEpoch,
		int64 Generation,
		bool bSucceeded);
	FString GetNextSlotName() const;

	int32 FindEntryIndex(const FGuid& OperationId) const;
	int32 FindCoalescingCandidate(
		const FAIRESyncOutboxEnqueueRequest& Request) const;
	int32 FindNextPendingIndex() const;
	bool HasInFlightEntry() const;
	void BeginActiveTransport();
	void HandleTransportResult(
		uint64 CallbackLifecycleEpoch,
		const FAIRESyncOutboxTransportResult& Result);
	bool HandleAttemptTimeout(float DeltaTime);
	void ReturnActiveAttemptToPending(bool bCancelTransport);
	void ClearActiveAttempt();
	void ScheduleAckedCompaction();
	void FinalizeAckedCompaction();

	TArray<FAIRESyncOutboxEntry> Entries;
	TArray<FLoadSlotState> LoadSlots;
	TSet<FGuid> PendingCompactionOperationIds;
	TSharedPtr<IAIRESyncOutboxTransport> Transport;
	FAIRESyncOutboxPersistenceCompleted PersistenceCompletedDelegate;
	FAIRESyncOutboxPersistenceResult LastPersistenceResult;
	FTSTicker::FDelegateHandle AttemptTimeoutTickerHandle;
	FGuid ActiveAttemptToken;
	FGuid ActiveOperationId;
	uint64 LifecycleEpoch = 0;
	uint64 PersistenceEpoch = 0;
	uint64 ActiveSaveEpoch = 0;
	int64 LatestGeneration = 0;
	int64 HighestIssuedGeneration = 0;
	int64 NextSequence = 1;
	FString LatestSlotName;
	FString LastIssuedSlotName;
	bool bReady = false;
	bool bShuttingDown = false;
	bool bDirty = false;
	bool bSaveInFlight = false;
	bool bCompactionSaveInFlight = false;
	bool bStartTransportAfterSave = false;
};

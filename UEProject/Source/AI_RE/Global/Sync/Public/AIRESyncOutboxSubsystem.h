#pragma once

#include "CoreMinimal.h"
#include "AIRESyncOutboxTransport.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIRESyncOutboxSubsystem.generated.h"

class UAIRESyncOutboxSaveGame;
class USaveGame;

/**
 * 게임 상태 변경을 외부 Backend로 안전하게 전달하기 위한 영속 Outbox입니다.
 *
 * OperationId, Sequence, Scope와 BodyHash를 가진 요청을 먼저 SaveGame에 기록한 뒤
 * 한 건씩 전송합니다. Snapshot은 Coalescing할 수 있지만 Event 순서는 유지하며,
 * ACK의 OperationId와 BodyHash가 현재 Attempt와 일치할 때만 완료로 확정합니다.
 *
 * Timeout·전송 실패는 항목을 Pending으로 돌려 재시도하고, AttemptToken과
 * LifecycleEpoch로 종료 이후 또는 이전 시도의 늦은 Callback을 무시합니다.
 */
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

	/** SHA-256 used by persisted outbox bodies and exact HTTP content hashing. */
	static FString ComputeBodyHash(const TArray<uint8>& Body);

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

#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "Offline/AIREOfflineTaskTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIREOfflineTaskSubsystem.generated.h"

class UAIREGameplayInventorySubsystem;
class UWorld;
struct FAIREInventoryPersistenceResult;

UCLASS()
class AI_RE_API UAIREOfflineTaskSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Offline")
	bool SyncOfflineTasks();

	UFUNCTION(BlueprintPure, Category = "AIRE|Offline")
	EAIREOfflineTaskSyncState GetSyncState() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Offline")
	FAIREOfflineTaskSyncResult GetLastSyncResult() const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Offline")
	FAIREOfflineTaskSyncFinished OnSyncFinished;

private:
	void HandlePersistenceReady(const FAIREInventoryPersistenceResult& Result);
	void HandlePersistenceSaveCompleted(
		const FAIREInventoryPersistenceResult& Result);
	void SendListRequest(uint64 RequestEpoch);
	void HandleListResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		uint64 RequestEpoch,
		const FString& RequestId);
	void ProcessNextTask(uint64 RequestEpoch);
	void SendGameStateVersionRequest(uint64 RequestEpoch);
	void HandleGameStateVersionResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		uint64 RequestEpoch,
		const FString& RequestId);
	void SendGameStatePutRequest(uint64 RequestEpoch);
	void HandleGameStatePutResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		uint64 RequestEpoch,
		const FString& RequestId);
	void SendTaskTransition(
		uint64 RequestEpoch,
		const FString& Action,
		EAIREOfflineTaskStatus ExpectedStatus);
	void HandleTaskTransitionResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		uint64 RequestEpoch,
		const FString& RequestId,
		EAIREOfflineTaskStatus ExpectedStatus);
	void ApplyOrClaimCompletedTask(uint64 RequestEpoch);
	void SendClaimRequest(uint64 RequestEpoch);
	bool IsActiveContextValid() const;
	void AbortStaleContext();
	void Finish(EAIREOfflineTaskSyncState State, const FString& Code);
	void CancelActiveRequest();

	TWeakObjectPtr<UAIREGameplayInventorySubsystem> InventorySubsystem;
	FHttpRequestPtr ActiveRequest;
	TArray<FAIREOfflineTask> PendingTasks;
	FAIREOfflineTask ActiveTask;
	FAIREOfflineTaskSyncResult LastSyncResult;
	TWeakObjectPtr<UWorld> SyncWorld;
	FGuid SyncInventorySessionId;
	uint64 Epoch = 0;
	int64 PendingSaveGeneration = 0;
	int32 NextTaskIndex = 0;
	int32 PendingCraftReservations = 0;
	int64 BaseGameStateVersion = 0;
	bool bAutomaticSyncStarted = false;
	bool bSaveWasCoalesced = false;
	bool bShuttingDown = false;
};

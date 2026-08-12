#include "Offline/AIREOfflineTaskSubsystem.h"

#include "AIREGameplayInventorySubsystem.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Offline/AIREOfflineTaskJsonAdapter.h"
#include "Offline/AIREOfflineTaskSettings.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
	constexpr int32 MaxHttpResponseBytes = 262144;
	constexpr TCHAR FixedGameClientToken[] = TEXT("AIRE_GAME");
	constexpr TCHAR CanonicalSaveSlotId[] = TEXT("demo-slot-1");

	FString NewStableId(const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT("%s-%s"),
			Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	}

	FString JoinUrl(const FString& BaseUrl, const FString& Path)
	{
		FString Result = BaseUrl;
		Result.RemoveFromEnd(TEXT("/"));
		if (!Path.StartsWith(TEXT("/")))
		{
			Result += TEXT("/");
		}
		return Result + Path;
	}
}

void UAIREOfflineTaskSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAIREGameplayInventorySubsystem>();
	InventorySubsystem = GetGameInstance()->GetSubsystem<
		UAIREGameplayInventorySubsystem>();
	if (!InventorySubsystem.IsValid())
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("MissingInventorySubsystem"));
		return;
	}
	InventorySubsystem->OnPersistenceReady().AddUObject(
		this,
		&UAIREOfflineTaskSubsystem::HandlePersistenceReady);
	InventorySubsystem->OnPersistenceSaveCompleted().AddUObject(
		this,
		&UAIREOfflineTaskSubsystem::HandlePersistenceSaveCompleted);
	if (InventorySubsystem->IsPersistenceReady())
	{
		bAutomaticSyncStarted = true;
		SyncOfflineTasks();
	}
}

void UAIREOfflineTaskSubsystem::Deinitialize()
{
	bShuttingDown = true;
	++Epoch;
	CancelActiveRequest();
	if (InventorySubsystem.IsValid())
	{
		InventorySubsystem->OnPersistenceReady().RemoveAll(this);
		InventorySubsystem->OnPersistenceSaveCompleted().RemoveAll(this);
	}
	InventorySubsystem.Reset();
	PendingTasks.Reset();
	OnSyncFinished.Clear();
	Super::Deinitialize();
}

bool UAIREOfflineTaskSubsystem::SyncOfflineTasks()
{
	if (bShuttingDown
		|| !InventorySubsystem.IsValid()
		|| !InventorySubsystem->IsPersistenceReady()
		|| LastSyncResult.State == EAIREOfflineTaskSyncState::Syncing
		|| LastSyncResult.State == EAIREOfflineTaskSyncState::Transitioning
		|| LastSyncResult.State == EAIREOfflineTaskSyncState::Applying
		|| LastSyncResult.State == EAIREOfflineTaskSyncState::Saving
		|| LastSyncResult.State == EAIREOfflineTaskSyncState::Claiming)
	{
		return false;
	}
	FAIREInventoryContainerSnapshot MakoSnapshot;
	if (!InventorySubsystem->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetMakoContainerId(),
			MakoSnapshot))
	{
		return false;
	}
	const uint64 RequestEpoch = ++Epoch;
	SyncWorld = GetWorld();
	SyncInventorySessionId = MakoSnapshot.SessionId;
	PendingTasks.Reset();
	NextTaskIndex = 0;
	PendingSaveGeneration = 0;
	bSaveWasCoalesced = false;
	LastSyncResult = FAIREOfflineTaskSyncResult();
	LastSyncResult.State = EAIREOfflineTaskSyncState::Syncing;
	SendListRequest(RequestEpoch);
	return ActiveRequest.IsValid();
}

EAIREOfflineTaskSyncState UAIREOfflineTaskSubsystem::GetSyncState() const
{
	return LastSyncResult.State;
}

FAIREOfflineTaskSyncResult UAIREOfflineTaskSubsystem::GetLastSyncResult() const
{
	return LastSyncResult;
}

void UAIREOfflineTaskSubsystem::HandlePersistenceReady(
	const FAIREInventoryPersistenceResult& Result)
{
	(void)Result;
	if (!bAutomaticSyncStarted && !bShuttingDown)
	{
		bAutomaticSyncStarted = true;
		SyncOfflineTasks();
	}
}

void UAIREOfflineTaskSubsystem::HandlePersistenceSaveCompleted(
	const FAIREInventoryPersistenceResult& Result)
{
	if (bShuttingDown
		|| LastSyncResult.State != EAIREOfflineTaskSyncState::Saving
		|| Result.Generation != PendingSaveGeneration)
	{
		return;
	}
	if (!IsActiveContextValid())
	{
		AbortStaleContext();
		return;
	}
	if (Result.Code != EAIREInventoryPersistenceResultCode::Succeeded)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InventorySaveFailed"));
		return;
	}
	if (bSaveWasCoalesced)
	{
		bSaveWasCoalesced = false;
		const FAIREInventoryPersistenceResult FollowUpSave =
			InventorySubsystem->RequestInventorySave();
		if (FollowUpSave.Code == EAIREInventoryPersistenceResultCode::InProgress)
		{
			PendingSaveGeneration = FollowUpSave.Generation;
			return;
		}
		if (FollowUpSave.Code != EAIREInventoryPersistenceResultCode::Succeeded
			&& FollowUpSave.Code != EAIREInventoryPersistenceResultCode::NoChanges)
		{
			Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InventorySaveFailed"));
			return;
		}
	}
	SendClaimRequest(Epoch);
}

void UAIREOfflineTaskSubsystem::SendListRequest(const uint64 RequestEpoch)
{
	const UAIREOfflineTaskSettings* Settings =
		GetDefault<UAIREOfflineTaskSettings>();
	const FString RequestId = NewStableId(TEXT("task-list"));
	if (!IsValid(Settings))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InvalidTaskSettings"));
		return;
	}
	ActiveRequest = FHttpModule::Get().CreateRequest();
	const FString Path = FString::Printf(
		TEXT("%s?save_slot_id=%s"),
		*Settings->TasksPath,
		CanonicalSaveSlotId);
	ActiveRequest->SetURL(JoinUrl(Settings->BackendBaseUrl, Path));
	ActiveRequest->SetVerb(TEXT("GET"));
	ActiveRequest->SetHeader(
		TEXT("Authorization"),
		TEXT("Bearer ") + FString(FixedGameClientToken));
	ActiveRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	ActiveRequest->SetHeader(TEXT("X-Request-ID"), RequestId);
	ActiveRequest->SetTimeout(Settings->ResponseTimeoutSeconds);
	ActiveRequest->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, RequestEpoch, RequestId](
			FHttpRequestPtr Request,
			FHttpResponsePtr Response,
			const bool bWasSuccessful)
		{
			HandleListResponse(
				Request,
				Response,
				bWasSuccessful,
				RequestEpoch,
				RequestId);
		});
	if (!ActiveRequest->ProcessRequest())
	{
		ActiveRequest.Reset();
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("TaskListRequestFailed"));
	}
}

void UAIREOfflineTaskSubsystem::HandleListResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	const uint64 RequestEpoch,
	const FString& RequestId)
{
	if (bShuttingDown
		|| RequestEpoch != Epoch
		|| Request != ActiveRequest
		|| LastSyncResult.State != EAIREOfflineTaskSyncState::Syncing)
	{
		return;
	}
	if (!IsActiveContextValid())
	{
		AbortStaleContext();
		return;
	}
	ActiveRequest.Reset();
	if (!bWasSuccessful
		|| !Response.IsValid()
		|| !EHttpResponseCodes::IsOk(Response->GetResponseCode())
		|| Response->GetContent().Num() > MaxHttpResponseBytes)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("TaskListNetworkFailure"));
		return;
	}
	const FAIREParsedOfflineTaskList Parsed =
		FAIREOfflineTaskJsonAdapter::ParseListResponse(
			Response->GetContentAsString(),
			RequestId);
	if (!Parsed.bIsValid)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, Parsed.ErrorCode);
		return;
	}
	PendingTasks = Parsed.Tasks;
	ProcessNextTask(RequestEpoch);
}

void UAIREOfflineTaskSubsystem::ProcessNextTask(const uint64 RequestEpoch)
{
	if (bShuttingDown || RequestEpoch != Epoch || !InventorySubsystem.IsValid())
	{
		return;
	}
	if (!IsActiveContextValid())
	{
		AbortStaleContext();
		return;
	}
	while (PendingTasks.IsValidIndex(NextTaskIndex))
	{
		ActiveTask = PendingTasks[NextTaskIndex];
		if (FAIREOfflineTaskJsonAdapter::IsSupportedTask(ActiveTask)
			&& ActiveTask.Status != EAIREOfflineTaskStatus::Claimed)
		{
			break;
		}
		++NextTaskIndex;
	}
	if (!PendingTasks.IsValidIndex(NextTaskIndex))
	{
		Finish(EAIREOfflineTaskSyncState::Succeeded, TEXT("Succeeded"));
		return;
	}
	switch (ActiveTask.Status)
	{
	case EAIREOfflineTaskStatus::Pending:
		LastSyncResult.State = EAIREOfflineTaskSyncState::Transitioning;
		SendTaskTransition(
			RequestEpoch,
			TEXT("start"),
			EAIREOfflineTaskStatus::InProgress);
		break;
	case EAIREOfflineTaskStatus::InProgress:
		LastSyncResult.State = EAIREOfflineTaskSyncState::Transitioning;
		SendTaskTransition(
			RequestEpoch,
			TEXT("complete"),
			EAIREOfflineTaskStatus::Completed);
		break;
	case EAIREOfflineTaskStatus::Completed:
		ApplyOrClaimCompletedTask(RequestEpoch);
		break;
	case EAIREOfflineTaskStatus::Claimed:
		break;
	}
}

void UAIREOfflineTaskSubsystem::SendTaskTransition(
	const uint64 RequestEpoch,
	const FString& Action,
	const EAIREOfflineTaskStatus ExpectedStatus)
{
	const UAIREOfflineTaskSettings* Settings =
		GetDefault<UAIREOfflineTaskSettings>();
	const FString RequestId = NewStableId(TEXT("task-transition"));
	if (!IsValid(Settings))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InvalidTaskSettings"));
		return;
	}
	ActiveRequest = FHttpModule::Get().CreateRequest();
	const FString Path = FString::Printf(
		TEXT("%s/%s/%s"),
		*Settings->TasksPath,
		*ActiveTask.TaskId,
		*Action);
	ActiveRequest->SetURL(JoinUrl(Settings->BackendBaseUrl, Path));
	ActiveRequest->SetVerb(TEXT("POST"));
	ActiveRequest->SetHeader(
		TEXT("Authorization"),
		TEXT("Bearer ") + FString(FixedGameClientToken));
	ActiveRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	ActiveRequest->SetHeader(TEXT("X-Request-ID"), RequestId);
	ActiveRequest->SetTimeout(Settings->ResponseTimeoutSeconds);
	ActiveRequest->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, RequestEpoch, RequestId, ExpectedStatus](
			FHttpRequestPtr Request,
			FHttpResponsePtr Response,
			const bool bWasSuccessful)
		{
			HandleTaskTransitionResponse(
				Request,
				Response,
				bWasSuccessful,
				RequestEpoch,
				RequestId,
				ExpectedStatus);
		});
	if (!ActiveRequest->ProcessRequest())
	{
		ActiveRequest.Reset();
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("TaskTransitionRequestFailed"));
	}
}

void UAIREOfflineTaskSubsystem::HandleTaskTransitionResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	const uint64 RequestEpoch,
	const FString& RequestId,
	const EAIREOfflineTaskStatus ExpectedStatus)
{
	if (bShuttingDown
		|| RequestEpoch != Epoch
		|| Request != ActiveRequest)
	{
		return;
	}
	if (!IsActiveContextValid())
	{
		AbortStaleContext();
		return;
	}
	ActiveRequest.Reset();
	if (!bWasSuccessful
		|| !Response.IsValid()
		|| !EHttpResponseCodes::IsOk(Response->GetResponseCode())
		|| Response->GetContent().Num() > MaxHttpResponseBytes)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("TaskTransitionNetworkFailure"));
		return;
	}
	const FAIREParsedOfflineTask Parsed =
		FAIREOfflineTaskJsonAdapter::ParseTaskResponse(
			Response->GetContentAsString(),
			RequestId,
			ActiveTask.TaskId,
			ExpectedStatus,
			ExpectedStatus == EAIREOfflineTaskStatus::Completed);
	if (!Parsed.bIsValid)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, Parsed.ErrorCode);
		return;
	}
	ActiveTask = Parsed.Task;
	if (ExpectedStatus == EAIREOfflineTaskStatus::InProgress)
	{
		LastSyncResult.State = EAIREOfflineTaskSyncState::Transitioning;
		SendTaskTransition(
			RequestEpoch,
			TEXT("complete"),
			EAIREOfflineTaskStatus::Completed);
		return;
	}
	if (ExpectedStatus == EAIREOfflineTaskStatus::Completed)
	{
		if (ActiveTask.Status == EAIREOfflineTaskStatus::InProgress)
		{
			++NextTaskIndex;
			ProcessNextTask(RequestEpoch);
			return;
		}
		ApplyOrClaimCompletedTask(RequestEpoch);
		return;
	}
	if (ExpectedStatus == EAIREOfflineTaskStatus::Claimed)
	{
		if (ActiveTask.bHasResultQuantity && ActiveTask.ResultQuantity > 0)
		{
			++LastSyncResult.AppliedCount;
		}
		++NextTaskIndex;
		PendingSaveGeneration = 0;
		bSaveWasCoalesced = false;
		ProcessNextTask(RequestEpoch);
	}
}

void UAIREOfflineTaskSubsystem::ApplyOrClaimCompletedTask(
	const uint64 RequestEpoch)
{
	if (!IsActiveContextValid())
	{
		AbortStaleContext();
		return;
	}
	if (!ActiveTask.bHasResultQuantity)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("MissingTaskResultQuantity"));
		return;
	}
	if (ActiveTask.ResultQuantity == 0)
	{
		SendClaimRequest(RequestEpoch);
		return;
	}
	LastSyncResult.State = EAIREOfflineTaskSyncState::Applying;
	FAIREInventoryContainerSnapshot MakoSnapshot;
	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!InventorySubsystem->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetMakoContainerId(),
			MakoSnapshot)
		|| !InventorySubsystem->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			StorageSnapshot))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InventoryUnavailable"));
		return;
	}
	FAIREOfflineTaskApplyRequest ApplyRequest;
	if (!FAIREOfflineTaskJsonAdapter::BuildInventoryApplyRequest(
			ActiveTask,
			MakoSnapshot.SessionId,
			MakoSnapshot.Revision,
			StorageSnapshot.Revision,
			ApplyRequest))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InvalidTaskInventoryMapping"));
		return;
	}
	const FAIREOfflineTaskApplyResult ApplyResult =
		InventorySubsystem->TryApplyOfflineTaskResult(ApplyRequest);
	if (!ApplyResult.WasApplied())
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("TaskInventoryRejected"));
		return;
	}
	if (ApplyResult.Code == EAIREInventoryMutationCode::AlreadyApplied)
	{
		const FAIREInventoryPersistenceResult LastSave =
			InventorySubsystem->GetLastPersistenceSaveResult();
		const bool bHasPersistedEvidence =
			LastSave.Code == EAIREInventoryPersistenceResultCode::NotStarted
			|| LastSave.Code == EAIREInventoryPersistenceResultCode::Succeeded
			|| LastSave.Code == EAIREInventoryPersistenceResultCode::NoChanges;
		if (bHasPersistedEvidence)
		{
			SendClaimRequest(RequestEpoch);
			return;
		}
	}
	LastSyncResult.State = EAIREOfflineTaskSyncState::Saving;
	const FAIREInventoryPersistenceResult SaveResult =
		InventorySubsystem->RequestInventorySave();
	if (SaveResult.Code == EAIREInventoryPersistenceResultCode::Succeeded
		|| SaveResult.Code == EAIREInventoryPersistenceResultCode::NoChanges)
	{
		SendClaimRequest(RequestEpoch);
		return;
	}
	if (SaveResult.Code != EAIREInventoryPersistenceResultCode::InProgress
		&& SaveResult.Code != EAIREInventoryPersistenceResultCode::Coalesced)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InventorySaveFailed"));
		return;
	}
	PendingSaveGeneration = SaveResult.Generation;
	bSaveWasCoalesced =
		SaveResult.Code == EAIREInventoryPersistenceResultCode::Coalesced;
}

void UAIREOfflineTaskSubsystem::SendClaimRequest(const uint64 RequestEpoch)
{
	LastSyncResult.State = EAIREOfflineTaskSyncState::Claiming;
	SendTaskTransition(
		RequestEpoch,
		TEXT("claim"),
		EAIREOfflineTaskStatus::Claimed);
}

bool UAIREOfflineTaskSubsystem::IsActiveContextValid() const
{
	if (!InventorySubsystem.IsValid() || GetWorld() != SyncWorld.Get())
	{
		return false;
	}
	FAIREInventoryContainerSnapshot MakoSnapshot;
	return InventorySubsystem->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetMakoContainerId(),
			MakoSnapshot)
		&& MakoSnapshot.SessionId == SyncInventorySessionId;
}

void UAIREOfflineTaskSubsystem::AbortStaleContext()
{
	++Epoch;
	CancelActiveRequest();
	LastSyncResult.State = EAIREOfflineTaskSyncState::Failed;
	LastSyncResult.Code = TEXT("StaleWorldOrInventorySession");
}

void UAIREOfflineTaskSubsystem::Finish(
	const EAIREOfflineTaskSyncState State,
	const FString& Code)
{
	CancelActiveRequest();
	LastSyncResult.State = State;
	LastSyncResult.Code = Code;
	OnSyncFinished.Broadcast(LastSyncResult);
}

void UAIREOfflineTaskSubsystem::CancelActiveRequest()
{
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
}

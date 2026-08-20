#include "Offline/AIREOfflineTaskSubsystem.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AIRESyncOutboxSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Offline/AIREOfflineTaskJsonAdapter.h"
#include "Offline/AIREOfflineTaskSettings.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Subsystems/SubsystemCollection.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
	constexpr int32 AIREOfflineTaskMaxHttpResponseBytes = 262144;
	constexpr TCHAR AIREOfflineTaskGameClientToken[] = TEXT("AIRE_GAME");
	constexpr TCHAR AIREOfflineTaskSaveSlotId[] = TEXT("demo-slot-1");

	FString NewAIREOfflineTaskStableId(const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT("%s-%s"),
			Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	}

	FString JoinAIREOfflineTaskUrl(const FString& BaseUrl, const FString& Path)
	{
		FString Result = BaseUrl;
		Result.RemoveFromEnd(TEXT("/"));
		if (!Path.StartsWith(TEXT("/")))
		{
			Result += TEXT("/");
		}
		return Result + Path;
	}

	TSharedRef<FJsonObject> MakeAIREEquipmentJson(const FName EquippedItemId)
	{
		TSharedRef<FJsonObject> Equipment = MakeShared<FJsonObject>();
		if (EquippedItemId.IsNone())
		{
			Equipment->SetField(TEXT("equipped_item_id"), MakeShared<FJsonValueNull>());
		}
		else
		{
			Equipment->SetStringField(
				TEXT("equipped_item_id"),
				EquippedItemId.ToString());
		}
		return Equipment;
	}

	template <typename StackType>
	TArray<TSharedPtr<FJsonValue>> MakeAIREStackJson(const TArray<StackType>& Stacks)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Stacks.Num());
		for (const StackType& Stack : Stacks)
		{
			TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
			Value->SetNumberField(TEXT("slot_index"), Stack.SlotIndex);
			Value->SetStringField(TEXT("item_id"), Stack.ItemId.ToString());
			Value->SetNumberField(TEXT("count"), Stack.Count);
			Values.Add(MakeShared<FJsonValueObject>(Value));
		}
		return Values;
	}

	bool BuildAIREGameStateBody(
		const FString& OperationId,
		const int64 StateVersion,
		const FGuid& WorldSessionId,
		const FAIREInventoryPersistedPlayerState& Player,
		const FAIREInventoryContainerSnapshot& Mako,
		const FAIREInventoryContainerSnapshot& Storage,
		TArray<uint8>& OutContent,
		FString& OutSha256)
	{
		if (OperationId.IsEmpty()
			|| StateVersion <= 0
			|| !WorldSessionId.IsValid()
			|| Player.InventoryCapacity != 30)
		{
			return false;
		}
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schema_version"), 1);
		Root->SetNumberField(TEXT("content_version"), 1);
		Root->SetStringField(TEXT("operation_id"), OperationId);
		Root->SetNumberField(TEXT("state_version"), StateVersion);
		Root->SetStringField(
			TEXT("world_session_id"),
			WorldSessionId.ToString(EGuidFormats::DigitsWithHyphensLower));
		Root->SetStringField(TEXT("captured_at"), FDateTime::UtcNow().ToIso8601());
		Root->SetStringField(TEXT("save_slot_id"), AIREOfflineTaskSaveSlotId);
		Root->SetStringField(TEXT("companion_id"), TEXT("mako"));

		TSharedRef<FJsonObject> Inventory = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> PlayerJson = MakeShared<FJsonObject>();
		PlayerJson->SetNumberField(TEXT("capacity"), Player.InventoryCapacity);
		PlayerJson->SetNumberField(TEXT("revision"), Player.Revision);
		PlayerJson->SetArrayField(TEXT("stacks"), MakeAIREStackJson(Player.ItemStacks));
		PlayerJson->SetObjectField(
			TEXT("equipment"),
			MakeAIREEquipmentJson(Player.Equipment.EquippedItemId));
		Inventory->SetObjectField(TEXT("player"), PlayerJson);

		TArray<TSharedPtr<FJsonValue>> Containers;
		auto AppendContainer = [&Containers](
			const FAIREInventoryContainerSnapshot& Snapshot)
		{
			TSharedRef<FJsonObject> Container = MakeShared<FJsonObject>();
			Container->SetStringField(
				TEXT("container_id"),
				Snapshot.ContainerId.ToString());
			Container->SetNumberField(TEXT("capacity"), Snapshot.Capacity);
			Container->SetNumberField(TEXT("revision"), Snapshot.Revision);
			Container->SetArrayField(
				TEXT("stacks"),
				MakeAIREStackJson(Snapshot.ItemStacks));
			Container->SetObjectField(
				TEXT("equipment"),
				MakeAIREEquipmentJson(Snapshot.Equipment.EquippedItemId));
			Containers.Add(MakeShared<FJsonValueObject>(Container));
		};
		AppendContainer(Mako);
		AppendContainer(Storage);
		Inventory->SetArrayField(TEXT("containers"), Containers);
		Root->SetObjectField(TEXT("inventory"), Inventory);

		FString Body;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			return false;
		}
		FTCHARToUTF8 Utf8(*Body);
		OutContent.Reset(Utf8.Length());
		OutContent.Append(
			reinterpret_cast<const uint8*>(Utf8.Get()),
			Utf8.Length());
		OutSha256 = UAIRESyncOutboxSubsystem::ComputeBodyHash(OutContent);
		return OutContent.Num() > 0 && OutSha256.Len() == 64;
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
	PendingCraftReservations = 0;
	BaseGameStateVersion = 0;
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
	const FString RequestId = NewAIREOfflineTaskStableId(TEXT("task-list"));
	if (!IsValid(Settings))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InvalidTaskSettings"));
		return;
	}
	ActiveRequest = FHttpModule::Get().CreateRequest();
	const FString Path = FString::Printf(
		TEXT("%s?save_slot_id=%s"),
		*Settings->TasksPath,
		AIREOfflineTaskSaveSlotId);
	ActiveRequest->SetURL(JoinAIREOfflineTaskUrl(Settings->BackendBaseUrl, Path));
	ActiveRequest->SetVerb(TEXT("GET"));
	ActiveRequest->SetHeader(
		TEXT("Authorization"),
		TEXT("Bearer ") + FString(AIREOfflineTaskGameClientToken));
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
		|| Response->GetContent().Num() > AIREOfflineTaskMaxHttpResponseBytes)
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
	PendingCraftReservations = 0;

	for (const FAIREOfflineTask& Task : PendingTasks)
	{
		if (FAIREOfflineTaskJsonAdapter::IsSupportedTask(Task)
			&& Task.TaskType == TEXT("Crafting")
			&& Task.Status != EAIREOfflineTaskStatus::Claimed)
		{
			++PendingCraftReservations;
		}
	}

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
		if (PendingCraftReservations > 0)
		{
			Finish(
				EAIREOfflineTaskSyncState::Succeeded,
				TEXT("SucceededCraftReservationPending"));
			return;
		}
		LastSyncResult.State = EAIREOfflineTaskSyncState::Syncing;
		SendGameStateVersionRequest(RequestEpoch);
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

void UAIREOfflineTaskSubsystem::SendGameStateVersionRequest(
	const uint64 RequestEpoch)
{
	const UAIREOfflineTaskSettings* Settings =
		GetDefault<UAIREOfflineTaskSettings>();
	const FString RequestId = NewAIREOfflineTaskStableId(TEXT("game-state-get"));
	if (!IsValid(Settings))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InvalidTaskSettings"));
		return;
	}
	const FString Path = FString::Printf(
		TEXT("%s?save_slot_id=%s&companion_id=mako"),
		*Settings->GameStatePath,
		AIREOfflineTaskSaveSlotId);
	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetURL(JoinAIREOfflineTaskUrl(Settings->BackendBaseUrl, Path));
	ActiveRequest->SetVerb(TEXT("GET"));
	ActiveRequest->SetHeader(
		TEXT("Authorization"),
		TEXT("Bearer ") + FString(AIREOfflineTaskGameClientToken));
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
			HandleGameStateVersionResponse(
				Request,
				Response,
				bWasSuccessful,
				RequestEpoch,
				RequestId);
		});
	if (!ActiveRequest->ProcessRequest())
	{
		ActiveRequest.Reset();
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("GameStateGetRequestFailed"));
	}
}

void UAIREOfflineTaskSubsystem::HandleGameStateVersionResponse(
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
		|| Response->GetContent().Num() > AIREOfflineTaskMaxHttpResponseBytes)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("GameStateGetNetworkFailure"));
		return;
	}
	if (Response->GetResponseCode() == 404)
	{
		BaseGameStateVersion = 0;
		SendGameStatePutRequest(RequestEpoch);
		return;
	}
	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("GameStateGetRejected"));
		return;
	}
	TSharedPtr<FJsonObject> Body;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Response->GetContentAsString());
	double ParsedVersion = 0.0;
	FString ResponseRequestId;
	if (!FJsonSerializer::Deserialize(Reader, Body)
		|| !Body.IsValid()
		|| !Body->TryGetStringField(TEXT("request_id"), ResponseRequestId)
		|| ResponseRequestId != RequestId
		|| !Body->TryGetNumberField(TEXT("state_version"), ParsedVersion)
		|| ParsedVersion < 1.0
		|| ParsedVersion > static_cast<double>(MAX_int64)
		|| ParsedVersion != FMath::FloorToDouble(ParsedVersion))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InvalidGameStateGetResponse"));
		return;
	}
	BaseGameStateVersion = static_cast<int64>(ParsedVersion);
	SendGameStatePutRequest(RequestEpoch);
}

void UAIREOfflineTaskSubsystem::SendGameStatePutRequest(
	const uint64 RequestEpoch)
{
	const UAIREOfflineTaskSettings* Settings =
		GetDefault<UAIREOfflineTaskSettings>();
	if (!IsValid(Settings) || BaseGameStateVersion == MAX_int64)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InvalidGameStateVersion"));
		return;
	}
	FAIREInventoryPersistedPlayerState Player;
	FAIREInventoryContainerSnapshot Mako;
	FAIREInventoryContainerSnapshot Storage;
	if (!InventorySubsystem->GetPlayerPersistenceSnapshot(Player)
		|| !InventorySubsystem->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetMakoContainerId(),
			Mako)
		|| !InventorySubsystem->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			Storage))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("GameStateInventoryUnavailable"));
		return;
	}
	const FString RequestId = NewAIREOfflineTaskStableId(TEXT("game-state-put"));
	TArray<uint8> Content;
	FString ContentHash;
	if (!BuildAIREGameStateBody(
			RequestId,
			BaseGameStateVersion + 1,
			SyncInventorySessionId,
			Player,
			Mako,
			Storage,
			Content,
			ContentHash))
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("GameStateSerializationFailed"));
		return;
	}
	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetURL(
		JoinAIREOfflineTaskUrl(Settings->BackendBaseUrl, Settings->GameStatePath));
	ActiveRequest->SetVerb(TEXT("PUT"));
	ActiveRequest->SetHeader(
		TEXT("Authorization"),
		TEXT("Bearer ") + FString(AIREOfflineTaskGameClientToken));
	ActiveRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	ActiveRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	ActiveRequest->SetHeader(TEXT("X-Request-ID"), RequestId);
	ActiveRequest->SetHeader(TEXT("X-Content-SHA256"), ContentHash);
	if (BaseGameStateVersion > 0)
	{
		ActiveRequest->SetHeader(
			TEXT("X-Base-State-Version"),
			LexToString(BaseGameStateVersion));
	}
	ActiveRequest->SetContent(MoveTemp(Content));
	ActiveRequest->SetTimeout(Settings->ResponseTimeoutSeconds);
	ActiveRequest->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, RequestEpoch, RequestId](
			FHttpRequestPtr Request,
			FHttpResponsePtr Response,
			const bool bWasSuccessful)
		{
			HandleGameStatePutResponse(
				Request,
				Response,
				bWasSuccessful,
				RequestEpoch,
				RequestId);
		});
	if (!ActiveRequest->ProcessRequest())
	{
		ActiveRequest.Reset();
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("GameStatePutRequestFailed"));
	}
}

void UAIREOfflineTaskSubsystem::HandleGameStatePutResponse(
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
	ActiveRequest.Reset();
	if (!IsActiveContextValid())
	{
		AbortStaleContext();
		return;
	}
	if (!bWasSuccessful
		|| !Response.IsValid()
		|| !EHttpResponseCodes::IsOk(Response->GetResponseCode())
		|| Response->GetContent().Num() > AIREOfflineTaskMaxHttpResponseBytes)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("GameStatePutRejected"));
		return;
	}
	TSharedPtr<FJsonObject> Body;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Response->GetContentAsString());
	FString ResponseRequestId;
	if (!FJsonSerializer::Deserialize(Reader, Body)
		|| !Body.IsValid()
		|| !Body->TryGetStringField(TEXT("request_id"), ResponseRequestId)
		|| ResponseRequestId != RequestId)
	{
		Finish(EAIREOfflineTaskSyncState::Failed, TEXT("InvalidGameStatePutResponse"));
		return;
	}
	Finish(EAIREOfflineTaskSyncState::Succeeded, TEXT("Succeeded"));
}

void UAIREOfflineTaskSubsystem::SendTaskTransition(
	const uint64 RequestEpoch,
	const FString& Action,
	const EAIREOfflineTaskStatus ExpectedStatus)
{
	const UAIREOfflineTaskSettings* Settings =
		GetDefault<UAIREOfflineTaskSettings>();
	const FString RequestId = NewAIREOfflineTaskStableId(TEXT("task-transition"));
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
	ActiveRequest->SetURL(JoinAIREOfflineTaskUrl(Settings->BackendBaseUrl, Path));
	ActiveRequest->SetVerb(TEXT("POST"));
	ActiveRequest->SetHeader(
		TEXT("Authorization"),
		TEXT("Bearer ") + FString(AIREOfflineTaskGameClientToken));
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
		|| Response->GetContent().Num() > AIREOfflineTaskMaxHttpResponseBytes)
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
		if (ActiveTask.TaskType == TEXT("Crafting"))
		{
			PendingCraftReservations = FMath::Max(0, PendingCraftReservations - 1);
		}
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

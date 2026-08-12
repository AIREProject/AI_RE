#include "Offline/AIREOfflineTaskJsonAdapter.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr int32 AIREOfflineTaskMaxStableIdLength = 128;
	constexpr int32 AIREOfflineTaskMaxTasksPerSync = 50;
	constexpr TCHAR AIREOfflineTaskAdapterSaveSlotId[] = TEXT("demo-slot-1");

	bool IsAIREOfflineTaskStableId(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > AIREOfflineTaskMaxStableIdLength)
		{
			return false;
		}
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			const bool bAllowed = FChar::IsAlnum(Character)
				|| Character == TEXT('.')
				|| Character == TEXT('_')
				|| Character == TEXT(':')
				|| Character == TEXT('-');
			if (!bAllowed || (Index == 0 && !FChar::IsAlnum(Character)))
			{
				return false;
			}
		}
		return true;
	}

	bool TryGetAIREOfflineTaskInteger(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		const int64 Minimum,
		const int64 Maximum,
		int64& OutValue)
	{
		double Number = 0.0;
		if (!Object.IsValid()
			|| !Object->TryGetNumberField(Field, Number)
			|| !FMath::IsFinite(Number)
			|| FMath::FloorToDouble(Number) != Number
			|| Number < static_cast<double>(Minimum)
			|| Number > static_cast<double>(Maximum))
		{
			return false;
		}
		OutValue = static_cast<int64>(Number);
		return true;
	}

	bool TryGetAIREOfflineTaskNullableInteger(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		bool& bOutHasValue,
		int32& OutValue)
	{
		const TSharedPtr<FJsonValue>* Value = Object.IsValid()
			? Object->Values.Find(Field)
			: nullptr;
		if (!Value || !Value->IsValid())
		{
			return false;
		}
		if ((*Value)->IsNull())
		{
			bOutHasValue = false;
			OutValue = 0;
			return true;
		}
		int64 Parsed = 0;
		if (!TryGetAIREOfflineTaskInteger(Object, Field, 0, MAX_int32, Parsed))
		{
			return false;
		}
		bOutHasValue = true;
		OutValue = static_cast<int32>(Parsed);
		return true;
	}

	bool TryParseAIREOfflineTaskStatus(
		const FString& Value,
		EAIREOfflineTaskStatus& OutStatus)
	{
		if (Value == TEXT("Pending"))
		{
			OutStatus = EAIREOfflineTaskStatus::Pending;
			return true;
		}
		if (Value == TEXT("InProgress"))
		{
			OutStatus = EAIREOfflineTaskStatus::InProgress;
			return true;
		}
		if (Value == TEXT("Completed"))
		{
			OutStatus = EAIREOfflineTaskStatus::Completed;
			return true;
		}
		if (Value == TEXT("Claimed"))
		{
			OutStatus = EAIREOfflineTaskStatus::Claimed;
			return true;
		}
		return false;
	}

	bool ParseAIREOfflineTask(
		const TSharedPtr<FJsonObject>& Object,
		FAIREOfflineTask& OutTask)
	{
		FString Status;
		FString StartedAt;
		const TSharedPtr<FJsonValue>* ItemIdValue = Object.IsValid()
			? Object->Values.Find(TEXT("item_id"))
			: nullptr;
		if (!Object.IsValid()
			|| !Object->TryGetStringField(TEXT("task_id"), OutTask.TaskId)
			|| !IsAIREOfflineTaskStableId(OutTask.TaskId)
			|| !Object->TryGetStringField(TEXT("save_slot_id"), OutTask.SaveSlotId)
			|| OutTask.SaveSlotId != AIREOfflineTaskAdapterSaveSlotId
			|| !ItemIdValue
			|| !ItemIdValue->IsValid()
			|| !Object->TryGetStringField(TEXT("task_type"), OutTask.TaskType)
			|| (OutTask.TaskType != TEXT("Gathering")
				&& OutTask.TaskType != TEXT("Crafting")
				&& OutTask.TaskType != TEXT("Scouting"))
			|| !Object->TryGetStringField(TEXT("status"), Status)
			|| !TryParseAIREOfflineTaskStatus(Status, OutTask.Status)
			|| !Object->TryGetStringField(TEXT("started_at"), StartedAt)
			|| !FDateTime::ParseIso8601(*StartedAt, OutTask.StartedAt)
			|| !TryGetAIREOfflineTaskNullableInteger(
				Object,
				TEXT("quantity"),
				OutTask.bHasQuantity,
				OutTask.Quantity)
			|| !TryGetAIREOfflineTaskNullableInteger(
				Object,
				TEXT("result_quantity"),
				OutTask.bHasResultQuantity,
				OutTask.ResultQuantity)
			|| !TryGetAIREOfflineTaskNullableInteger(
				Object,
				TEXT("progress_quantity"),
				OutTask.bHasProgressQuantity,
				OutTask.ProgressQuantity))
		{
			return false;
		}
		if ((*ItemIdValue)->IsNull())
		{
			OutTask.ItemId.Reset();
		}
		else if (!Object->TryGetStringField(TEXT("item_id"), OutTask.ItemId)
			|| !IsAIREOfflineTaskStableId(OutTask.ItemId))
		{
			return false;
		}
		return (!OutTask.bHasProgressQuantity
				|| OutTask.Status == EAIREOfflineTaskStatus::InProgress)
			&& (!OutTask.bHasQuantity
				|| !OutTask.bHasResultQuantity
				|| OutTask.ResultQuantity <= OutTask.Quantity);
	}

	bool DeserializeAIREOfflineTaskObject(
		const FString& Json,
		TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject)
			&& OutObject.IsValid();
	}

	bool ValidateAIREOfflineTaskRequestId(
		const TSharedPtr<FJsonObject>& Root,
		const FString& ExpectedRequestId)
	{
		FString RequestId;
		return Root.IsValid()
			&& Root->TryGetStringField(TEXT("request_id"), RequestId)
			&& RequestId == ExpectedRequestId;
	}
}

FAIREParsedOfflineTaskList FAIREOfflineTaskJsonAdapter::ParseListResponse(
	const FString& Json,
	const FString& ExpectedRequestId)
{
	FAIREParsedOfflineTaskList Result;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!DeserializeAIREOfflineTaskObject(Json, Root)
		|| !ValidateAIREOfflineTaskRequestId(Root, ExpectedRequestId)
		|| !Root->TryGetArrayField(TEXT("tasks"), Values)
		|| !Values
		|| Values->Num() > AIREOfflineTaskMaxTasksPerSync)
	{
		Result.ErrorCode = TEXT("InvalidTaskListResponse");
		return Result;
	}
	TSet<FString> SeenTaskIds;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FAIREOfflineTask& Task = Result.Tasks.AddDefaulted_GetRef();
		if (!Value.IsValid()
			|| !Value->TryGetObject(Object)
			|| !Object
			|| !ParseAIREOfflineTask(*Object, Task)
			|| SeenTaskIds.Contains(Task.TaskId))
		{
			Result.Tasks.Reset();
			Result.ErrorCode = TEXT("InvalidTaskListResponse");
			return Result;
		}
		SeenTaskIds.Add(Task.TaskId);
	}
	Result.bIsValid = true;
	return Result;
}

FAIREParsedOfflineTask FAIREOfflineTaskJsonAdapter::ParseTaskResponse(
	const FString& Json,
	const FString& ExpectedRequestId,
	const FString& ExpectedTaskId,
	const EAIREOfflineTaskStatus ExpectedStatus,
	const bool bAllowInProgressInsteadOfCompleted)
{
	FAIREParsedOfflineTask Result;
	TSharedPtr<FJsonObject> Root;
	const TSharedPtr<FJsonObject>* TaskObject = nullptr;
	if (!DeserializeAIREOfflineTaskObject(Json, Root)
		|| !ValidateAIREOfflineTaskRequestId(Root, ExpectedRequestId)
		|| !Root->TryGetObjectField(TEXT("task"), TaskObject)
		|| !TaskObject
		|| !ParseAIREOfflineTask(*TaskObject, Result.Task)
		|| Result.Task.TaskId != ExpectedTaskId)
	{
		Result.ErrorCode = TEXT("InvalidTaskTransitionResponse");
		return Result;
	}
	const bool bExpectedStatus = Result.Task.Status == ExpectedStatus;
	const bool bNotReadyYet = bAllowInProgressInsteadOfCompleted
		&& ExpectedStatus == EAIREOfflineTaskStatus::Completed
		&& Result.Task.Status == EAIREOfflineTaskStatus::InProgress
		&& Result.Task.bHasProgressQuantity
		&& Result.Task.ProgressQuantity == 0
		&& !Result.Task.bHasResultQuantity;
	if (!bExpectedStatus && !bNotReadyYet)
	{
		Result.ErrorCode = TEXT("InvalidTaskTransitionResponse");
		return Result;
	}
	Result.bIsValid = true;
	return Result;
}

bool FAIREOfflineTaskJsonAdapter::IsSupportedTask(const FAIREOfflineTask& Task)
{
	return Task.bHasQuantity
		&& Task.Quantity >= 1
		&& Task.Quantity <= 50
		&& ((Task.TaskType == TEXT("Gathering")
				&& Task.ItemId == TEXT("PlantStem"))
			|| (Task.TaskType == TEXT("Crafting")
				&& Task.ItemId == TEXT("ShoddyBandage")));
}

bool FAIREOfflineTaskJsonAdapter::BuildInventoryApplyRequest(
	const FAIREOfflineTask& Task,
	const FGuid& SessionId,
	const int64 MakoRevision,
	const int64 StorageRevision,
	FAIREOfflineTaskApplyRequest& OutRequest)
{
	if (!IsSupportedTask(Task)
		|| Task.Status != EAIREOfflineTaskStatus::Completed
		|| !Task.bHasResultQuantity
		|| Task.ResultQuantity <= 0
		|| Task.ResultQuantity > Task.Quantity
		|| !SessionId.IsValid())
	{
		return false;
	}
	OutRequest = FAIREOfflineTaskApplyRequest();
	OutRequest.TaskId = Task.TaskId;
	OutRequest.SessionId = SessionId;
	OutRequest.ExpectedMakoRevision = MakoRevision;
	OutRequest.ExpectedStorageRevision = StorageRevision;
	if (Task.TaskType == TEXT("Crafting"))
	{
		const int64 Cost = static_cast<int64>(Task.ResultQuantity) * 2;
		if (Cost > MAX_int32)
		{
			return false;
		}
		FAIREInventoryItemQuantity& Ingredient =
			OutRequest.Costs.AddDefaulted_GetRef();
		Ingredient.ItemId = FName(TEXT("PlantStem"));
		Ingredient.Count = static_cast<int32>(Cost);
	}
	FAIREInventoryItemQuantity& Reward =
		OutRequest.Rewards.AddDefaulted_GetRef();
	Reward.ItemId = FName(*Task.ItemId);
	Reward.Count = Task.ResultQuantity;
	return true;
}

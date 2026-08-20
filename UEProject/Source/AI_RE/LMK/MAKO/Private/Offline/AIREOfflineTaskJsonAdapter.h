#pragma once

#include "CoreMinimal.h"
#include "Offline/AIREOfflineTaskTypes.h"

struct FAIREParsedOfflineTaskList
{
	bool bIsValid = false;
	FString ErrorCode;
	TArray<FAIREOfflineTask> Tasks;
};

struct FAIREParsedOfflineTask
{
	bool bIsValid = false;
	FString ErrorCode;
	FAIREOfflineTask Task;
};

class FAIREOfflineTaskJsonAdapter
{
public:
	static FAIREParsedOfflineTaskList ParseListResponse(
		const FString& Json,
		const FString& ExpectedRequestId);
	static FAIREParsedOfflineTask ParseTaskResponse(
		const FString& Json,
		const FString& ExpectedRequestId,
		const FString& ExpectedTaskId,
		EAIREOfflineTaskStatus ExpectedStatus,
		bool bAllowInProgressInsteadOfCompleted = false);
	static bool IsSupportedTask(const FAIREOfflineTask& Task);
	static bool BuildInventoryApplyRequest(
		const FAIREOfflineTask& Task,
		const FGuid& SessionId,
		int64 MakoRevision,
		int64 StorageRevision,
		FAIREOfflineTaskApplyRequest& OutRequest);
};

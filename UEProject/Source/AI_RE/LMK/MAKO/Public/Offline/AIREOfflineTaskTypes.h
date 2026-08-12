#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "AIREOfflineTaskTypes.generated.h"

UENUM(BlueprintType)
enum class EAIREOfflineTaskStatus : uint8
{
	Pending,
	InProgress,
	Completed,
	Claimed
};

UENUM(BlueprintType)
enum class EAIREOfflineTaskSyncState : uint8
{
	Idle,
	Syncing,
	Transitioning,
	Applying,
	Saving,
	Claiming,
	Succeeded,
	Failed
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREOfflineTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	FString TaskId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	FString SaveSlotId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	FString TaskType;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	EAIREOfflineTaskStatus Status = EAIREOfflineTaskStatus::Pending;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	FDateTime StartedAt;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	bool bHasQuantity = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	bool bHasResultQuantity = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	int32 ResultQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	bool bHasProgressQuantity = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	int32 ProgressQuantity = 0;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREOfflineTaskSyncResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	EAIREOfflineTaskSyncState State = EAIREOfflineTaskSyncState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	FString Code;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Offline")
	int32 AppliedCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIREOfflineTaskSyncFinished,
	const FAIREOfflineTaskSyncResult&,
	Result);

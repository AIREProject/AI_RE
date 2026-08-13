#pragma once

#include "CoreMinimal.h"
#include "AIRECompanionCommandTypes.generated.h"

UENUM(BlueprintType)
enum class EAIRECommandType : uint8
{
	Follow,
	HoldPosition,
	ReturnToPlayer,
	EngageTarget,
	DistractTarget,
	MoveToLocation,
	CancelCurrent,
	GatherResource,
	Attack,
	Switch,
	CraftItem
};

UENUM(BlueprintType)
enum class EAIRECommandPriority : uint8
{
	Low,
	Normal,
	High,
	Critical
};

UENUM(BlueprintType)
enum class EAIRECommandResultStatus : uint8
{
	Accepted,
	Running,
	Succeeded,
	Rejected,
	Failed,
	Cancelled,
	Expired
};

UENUM(BlueprintType)
enum class EAIRECommandResultReason : uint8
{
	None,
	LeaseCompleted,
	MalformedCandidate,
	RequestMismatch,
	DuplicateCommand,
	MultipleCandidatesNotSupported,
	InvalidLifetime,
	InvalidParameters,
	UnsupportedExecution,
	TargetIdentityUnavailable,
	HigherPriorityBehaviorActive,
	PlayerUnavailable,
	NavigationFailed,
	WorkOrderUnavailable,
	WorkOrderCancellationFailed,
	RecipeUnavailable,
	MaterialsUnavailable,
	WorkbenchUnavailable,
	WorkOrderFailed,
	ThreatUnavailable,
	ThreatTargetLost,
	ReplacedByNewCommand,
	PreemptedByLocalBehavior,
	OwnerEndingPlay
};

UENUM(BlueprintType)
enum class EAIREDirectCommandIntent : uint8
{
	None,
	Follow,
	HoldPosition,
	ReturnToPlayer
};

UENUM(BlueprintType)
enum class EAIREGatherResourceKind : uint8
{
	None,
	Wood,
	Stone
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRECommandCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FString CommandId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FString RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	EAIRECommandType Type = EAIRECommandType::Follow;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FString TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	EAIRECommandPriority Priority = EAIRECommandPriority::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FDateTime IssuedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FDateTime ExpiresAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FString ParameterTargetId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	EAIREGatherResourceKind GatherResource = EAIREGatherResourceKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	int32 GatherQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	bool bHasGatherQuantity = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FString CraftRecipeId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	int32 CraftQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	bool bHasCraftQuantity = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	bool bHasUnsupportedParameters = false;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRECommandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FString CommandId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FString RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	EAIRECommandType Type = EAIRECommandType::Follow;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	EAIRECommandResultStatus Status = EAIRECommandResultStatus::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	EAIRECommandResultReason Reason = EAIRECommandResultReason::None;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREDirectCommandSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FString CommandId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	EAIREDirectCommandIntent Intent = EAIREDirectCommandIntent::None;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	FDateTime ExpiresAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	int64 Generation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Command")
	bool bIsActive = false;
};

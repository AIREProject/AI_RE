#pragma once

#include "CoreMinimal.h"
#include "AIREChatTypes.generated.h"

UENUM(BlueprintType)
enum class EAIREChatTransportMode : uint8
{
	WebSocket,
	Http
};

UENUM(BlueprintType)
enum class EAIREChatConnectionState : uint8
{
	Disconnected,
	Connecting,
	Connected
};

UENUM(BlueprintType)
enum class EAIREChatRequestState : uint8
{
	Idle,
	Sending,
	RetryableFailed,
	Failed,
	Cancelled
};

UENUM(BlueprintType)
enum class EAIREGameWorldPeriod : uint8
{
	Dawn,
	Morning,
	Afternoon,
	Evening,
	Night
};

UENUM(BlueprintType)
enum class EAIREChatFakeScenario : uint8
{
	Disabled,
	Success,
	ConnectionFailure,
	CredentialRejected,
	Error,
	Timeout,
	MalformedResponse,
	DelayedSuccess
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREInGameChatContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Chat")
	FString SaveSlotId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Chat", meta = (ClampMin = "0"))
	int32 Day = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Chat", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float Hour = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Chat")
	EAIREGameWorldPeriod Period = EAIREGameWorldPeriod::Morning;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREChatResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	FString RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	FString ResponseId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	FString DisplayText;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREChatError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	FString RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	FString Code;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	bool bRetryable = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIREChatConnectionStateChanged,
	EAIREChatConnectionState,
	NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIREChatRequestStateChanged,
	EAIREChatRequestState,
	NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIREChatResponseReceived,
	const FAIREChatResult&,
	Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIREChatRequestFailed,
	const FAIREChatError&,
	Error);


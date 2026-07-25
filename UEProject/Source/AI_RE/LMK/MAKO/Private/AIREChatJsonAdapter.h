#pragma once

#include "CoreMinimal.h"
#include "AIREChatTypes.h"

enum class EAIREParsedChatFrameKind : uint8
{
	Ignored,
	Response,
	Error,
	Invalid
};

struct FAIREParsedChatFrame
{
	EAIREParsedChatFrameKind Kind = EAIREParsedChatFrameKind::Invalid;
	FAIREChatResult Result;
	FAIREChatError Error;
};

class FAIREChatJsonAdapter
{
public:
	static bool BuildInGameRequest(
		const FAIREInGameChatContext& Context,
		const FString& CompanionId,
		const FString& SessionId,
		const FString& RequestId,
		const FString& MessageId,
		const FString& UserMessage,
		FString& OutHttpBody,
		FString& OutWebSocketFrame,
		FString& OutError);

	static FAIREParsedChatFrame ParseWebSocketFrame(
		const FString& Message,
		const FString& ExpectedRequestId);

	static FAIREParsedChatFrame ParseHttpBody(
		const FString& Message,
		const FString& ExpectedRequestId,
		bool bIsErrorResponse);

	static bool IsStableId(const FString& Value);
};


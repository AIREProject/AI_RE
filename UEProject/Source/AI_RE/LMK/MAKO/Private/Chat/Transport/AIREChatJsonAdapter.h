#pragma once

#include "CoreMinimal.h"
#include "Chat/Contracts/AIREChatTypes.h"

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

struct FAIREChatResponseCorrelation
{
	FString RequestId;
	FString MessageId;
	FString SessionId;
	FString SaveSlotId;
	FString CompanionId;
};

class FAIREChatJsonAdapter
{
public:
	static bool BuildInGameRequest(
		const FAIREInGameChatContext& Context,
		const FAIREWorldContextV1& WorldContext,
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
		const FAIREChatResponseCorrelation& ExpectedCorrelation);

	static FAIREParsedChatFrame ParseHttpBody(
		const FString& Message,
		const FAIREChatResponseCorrelation& ExpectedCorrelation,
		bool bIsErrorResponse);

	static bool IsStableId(const FString& Value);
};


#include "AIREChatJsonAdapter.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 SchemaVersion = 1;
	constexpr int32 MaxStableIdLength = 128;
	constexpr int32 MaxUserMessageLength = 2000;
	constexpr int32 MaxDisplayTextLength = 4000;

	FString GetPeriodName(const EAIREGameWorldPeriod Period)
	{
		switch (Period)
		{
		case EAIREGameWorldPeriod::Dawn:
			return TEXT("Dawn");
		case EAIREGameWorldPeriod::Morning:
			return TEXT("Morning");
		case EAIREGameWorldPeriod::Afternoon:
			return TEXT("Afternoon");
		case EAIREGameWorldPeriod::Evening:
			return TEXT("Evening");
		case EAIREGameWorldPeriod::Night:
			return TEXT("Night");
		default:
			return FString();
		}
	}

	bool SerializeChatObject(const TSharedRef<FJsonObject>& Object, FString& OutJson)
	{
		OutJson.Reset();
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object, Writer);
	}

	bool DeserializeChatObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	FAIREParsedChatFrame InvalidFrame(
		const FString& RequestId,
		const FString& Code,
		const FString& Message)
	{
		FAIREParsedChatFrame Frame;
		Frame.Kind = EAIREParsedChatFrameKind::Invalid;
		Frame.Error.RequestId = RequestId;
		Frame.Error.Code = Code;
		Frame.Error.Message = Message;
		Frame.Error.bRetryable = false;
		return Frame;
	}

	FAIREParsedChatFrame ParseResponsePayload(
		const TSharedPtr<FJsonObject>& Payload,
		const FString& ExpectedRequestId)
	{
		if (!Payload.IsValid())
		{
			return InvalidFrame(ExpectedRequestId, TEXT("InvalidChatResponse"), TEXT("Chat response payload is missing."));
		}

		int32 ParsedSchemaVersion = 0;
		FString RequestId;
		FString ResponseId;
		FString InteractionMode;
		FString DisplayText;
		if (!Payload->TryGetNumberField(TEXT("schema_version"), ParsedSchemaVersion)
			|| ParsedSchemaVersion != SchemaVersion
			|| !Payload->TryGetStringField(TEXT("request_id"), RequestId)
			|| !FAIREChatJsonAdapter::IsStableId(RequestId)
			|| !Payload->TryGetStringField(TEXT("response_id"), ResponseId)
			|| !FAIREChatJsonAdapter::IsStableId(ResponseId)
			|| !Payload->TryGetStringField(TEXT("interaction_mode"), InteractionMode)
			|| InteractionMode != TEXT("InGame")
			|| !Payload->TryGetStringField(TEXT("display_text"), DisplayText)
			|| DisplayText.TrimStartAndEnd().IsEmpty()
			|| DisplayText.Len() > MaxDisplayTextLength)
		{
			return InvalidFrame(ExpectedRequestId, TEXT("InvalidChatResponse"), TEXT("Chat response validation failed."));
		}
		if (RequestId != ExpectedRequestId)
		{
			FAIREParsedChatFrame Frame;
			Frame.Kind = EAIREParsedChatFrameKind::Ignored;
			return Frame;
		}

		FAIREParsedChatFrame Frame;
		Frame.Kind = EAIREParsedChatFrameKind::Response;
		Frame.Result.RequestId = MoveTemp(RequestId);
		Frame.Result.ResponseId = MoveTemp(ResponseId);
		Frame.Result.DisplayText = MoveTemp(DisplayText);
		return Frame;
	}

	FAIREParsedChatFrame ParseErrorPayload(
		const TSharedPtr<FJsonObject>& Payload,
		const FString& ExpectedRequestId)
	{
		if (!Payload.IsValid())
		{
			return InvalidFrame(ExpectedRequestId, TEXT("InvalidErrorResponse"), TEXT("Error payload is missing."));
		}

		int32 ParsedSchemaVersion = 0;
		FString RequestId;
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (!Payload->TryGetNumberField(TEXT("schema_version"), ParsedSchemaVersion)
			|| ParsedSchemaVersion != SchemaVersion
			|| !Payload->TryGetStringField(TEXT("request_id"), RequestId)
			|| !FAIREChatJsonAdapter::IsStableId(RequestId)
			|| !Payload->TryGetObjectField(TEXT("error"), ErrorObject)
			|| ErrorObject == nullptr
			|| !ErrorObject->IsValid())
		{
			return InvalidFrame(ExpectedRequestId, TEXT("InvalidErrorResponse"), TEXT("Error response validation failed."));
		}
		if (RequestId != ExpectedRequestId)
		{
			FAIREParsedChatFrame Frame;
			Frame.Kind = EAIREParsedChatFrameKind::Ignored;
			return Frame;
		}

		FString Code;
		FString Message;
		bool bRetryable = false;
		if (!(*ErrorObject)->TryGetStringField(TEXT("code"), Code)
			|| Code.IsEmpty()
			|| !(*ErrorObject)->TryGetStringField(TEXT("message"), Message)
			|| Message.IsEmpty()
			|| !(*ErrorObject)->TryGetBoolField(TEXT("retryable"), bRetryable))
		{
			return InvalidFrame(ExpectedRequestId, TEXT("InvalidErrorResponse"), TEXT("Error response validation failed."));
		}

		FAIREParsedChatFrame Frame;
		Frame.Kind = EAIREParsedChatFrameKind::Error;
		Frame.Error.RequestId = MoveTemp(RequestId);
		Frame.Error.Code = MoveTemp(Code);
		Frame.Error.Message = MoveTemp(Message);
		Frame.Error.bRetryable = bRetryable;
		return Frame;
	}
}

bool FAIREChatJsonAdapter::BuildInGameRequest(
	const FAIREInGameChatContext& Context,
	const FString& CompanionId,
	const FString& SessionId,
	const FString& RequestId,
	const FString& MessageId,
	const FString& UserMessage,
	FString& OutHttpBody,
	FString& OutWebSocketFrame,
	FString& OutError)
{
	OutError.Reset();
	const FString TrimmedMessage = UserMessage.TrimStartAndEnd();
	const FString PeriodName = GetPeriodName(Context.Period);
	if (!IsStableId(Context.SaveSlotId)
		|| !IsStableId(CompanionId)
		|| !IsStableId(SessionId)
		|| !IsStableId(RequestId)
		|| !IsStableId(MessageId)
		|| TrimmedMessage.IsEmpty()
		|| TrimmedMessage.Len() > MaxUserMessageLength
		|| Context.Day < 0
		|| !FMath::IsFinite(Context.Hour)
		|| Context.Hour < 0.0f
		|| Context.Hour >= 24.0f
		|| PeriodName.IsEmpty())
	{
		OutError = TEXT("Chat request validation failed.");
		return false;
	}

	const TSharedRef<FJsonObject> TimeContext = MakeShared<FJsonObject>();
	TimeContext->SetStringField(TEXT("source"), TEXT("GameWorld"));
	TimeContext->SetNumberField(TEXT("day"), Context.Day);
	TimeContext->SetNumberField(TEXT("hour"), Context.Hour);
	TimeContext->SetStringField(TEXT("period"), PeriodName);

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("schema_version"), SchemaVersion);
	Payload->SetStringField(TEXT("request_id"), RequestId);
	Payload->SetStringField(TEXT("save_slot_id"), Context.SaveSlotId);
	Payload->SetStringField(TEXT("companion_id"), CompanionId);
	Payload->SetStringField(TEXT("session_id"), SessionId);
	Payload->SetStringField(TEXT("interaction_mode"), TEXT("InGame"));
	Payload->SetStringField(TEXT("message_id"), MessageId);
	Payload->SetStringField(TEXT("user_message"), TrimmedMessage);
	Payload->SetObjectField(TEXT("time_context"), TimeContext);
	Payload->SetArrayField(
		TEXT("recent_event_ids"),
		TArray<TSharedPtr<FJsonValue>>());
	Payload->SetObjectField(TEXT("game_context"), MakeShared<FJsonObject>());
	Payload->SetArrayField(
		TEXT("allowed_commands"),
		TArray<TSharedPtr<FJsonValue>>());
	if (!SerializeChatObject(Payload, OutHttpBody))
	{
		OutError = TEXT("Could not serialize the Chat request.");
		return false;
	}

	const TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
	Frame->SetStringField(TEXT("type"), TEXT("chat"));
	Frame->SetObjectField(TEXT("payload"), Payload);
	if (!SerializeChatObject(Frame, OutWebSocketFrame))
	{
		OutError = TEXT("Could not serialize the WebSocket Chat frame.");
		return false;
	}

	return true;
}

FAIREParsedChatFrame FAIREChatJsonAdapter::ParseWebSocketFrame(
	const FString& Message,
	const FString& ExpectedRequestId)
{
	TSharedPtr<FJsonObject> FrameObject;
	if (!DeserializeChatObject(Message, FrameObject))
	{
		return InvalidFrame(ExpectedRequestId, TEXT("MalformedJson"), TEXT("WebSocket response is not valid JSON."));
	}

	FString Type;
	if (!FrameObject->TryGetStringField(TEXT("type"), Type) || Type.IsEmpty())
	{
		return InvalidFrame(ExpectedRequestId, TEXT("InvalidFrame"), TEXT("WebSocket response type is missing."));
	}

	if (Type != TEXT("chat_response") && Type != TEXT("error"))
	{
		FAIREParsedChatFrame Frame;
		Frame.Kind = EAIREParsedChatFrameKind::Ignored;
		return Frame;
	}

	const TSharedPtr<FJsonObject>* Payload = nullptr;
	if (!FrameObject->TryGetObjectField(TEXT("payload"), Payload) || Payload == nullptr)
	{
		return InvalidFrame(ExpectedRequestId, TEXT("InvalidFrame"), TEXT("WebSocket response payload is missing."));
	}

	return Type == TEXT("chat_response")
		? ParseResponsePayload(*Payload, ExpectedRequestId)
		: ParseErrorPayload(*Payload, ExpectedRequestId);
}

FAIREParsedChatFrame FAIREChatJsonAdapter::ParseHttpBody(
	const FString& Message,
	const FString& ExpectedRequestId,
	const bool bIsErrorResponse)
{
	TSharedPtr<FJsonObject> Body;
	if (!DeserializeChatObject(Message, Body))
	{
		return InvalidFrame(ExpectedRequestId, TEXT("MalformedJson"), TEXT("HTTP response is not valid JSON."));
	}

	return bIsErrorResponse
		? ParseErrorPayload(Body, ExpectedRequestId)
		: ParseResponsePayload(Body, ExpectedRequestId);
}

bool FAIREChatJsonAdapter::IsStableId(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > MaxStableIdLength)
	{
		return false;
	}

	for (int32 Index = 0; Index < Value.Len(); ++Index)
	{
		const TCHAR Character = Value[Index];
		const bool bIsAllowed = FChar::IsAlnum(Character)
			|| Character == TEXT('.')
			|| Character == TEXT('_')
			|| Character == TEXT(':')
			|| Character == TEXT('-');
		if (!bIsAllowed || (Index == 0 && !FChar::IsAlnum(Character)))
		{
			return false;
		}
	}

	return true;
}

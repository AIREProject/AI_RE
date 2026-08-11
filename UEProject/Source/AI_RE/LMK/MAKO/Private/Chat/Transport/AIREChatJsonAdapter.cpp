#include "Chat/Transport/AIREChatJsonAdapter.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 SchemaVersion = 1;
	constexpr int32 ChatMaxStableIdLength = 128;
	constexpr int32 MaxUserMessageLength = 2000;
	constexpr int32 MaxDisplayTextLength = 4000;
	constexpr int32 MaxAIProviderLength = 64;
	constexpr int32 MaxAIModelVersionLength = 128;
	constexpr int32 MaxAIPromptVersionLength = 128;
	constexpr int32 MaxErrorCodeLength = 128;
	constexpr int32 MaxErrorMessageLength = 512;
	constexpr int32 MaxCommandCandidates = 4;
	constexpr int32 MaxCandidateParameterProperties = 16;
	constexpr int32 MaxCandidateParameterDepth = 4;
	constexpr int32 MaxCandidateParameterStringLength = 256;
	constexpr int32 MaxCandidateParameterArrayLength = 16;
	constexpr int32 MaxCandidateParameterObjectProperties = 16;
	constexpr double MaxCandidateLeaseSeconds = 60.0;

	const TCHAR* const FixedAllowedCommands[] =
	{
		TEXT("Command.Follow"),
		TEXT("Command.HoldPosition"),
		TEXT("Command.ReturnToPlayer"),
		TEXT("Command.EngageTarget"),
		TEXT("Command.DistractTarget"),
		TEXT("Command.MoveToLocation"),
		TEXT("Command.CancelCurrent"),
		TEXT("Command.GatherResource"),
		TEXT("Command.Attack"),
		TEXT("Command.Switch"),
	};

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

	bool ParseCommandType(const FString& Value, EAIRECommandType& OutType)
	{
		if (Value == TEXT("Command.Follow"))
		{
			OutType = EAIRECommandType::Follow;
		}
		else if (Value == TEXT("Command.HoldPosition"))
		{
			OutType = EAIRECommandType::HoldPosition;
		}
		else if (Value == TEXT("Command.ReturnToPlayer"))
		{
			OutType = EAIRECommandType::ReturnToPlayer;
		}
		else if (Value == TEXT("Command.EngageTarget"))
		{
			OutType = EAIRECommandType::EngageTarget;
		}
		else if (Value == TEXT("Command.DistractTarget"))
		{
			OutType = EAIRECommandType::DistractTarget;
		}
		else if (Value == TEXT("Command.MoveToLocation"))
		{
			OutType = EAIRECommandType::MoveToLocation;
		}
		else if (Value == TEXT("Command.CancelCurrent"))
		{
			OutType = EAIRECommandType::CancelCurrent;
		}
		else if (Value == TEXT("Command.GatherResource"))
		{
			OutType = EAIRECommandType::GatherResource;
		}
		else if (Value == TEXT("Command.Attack"))
		{
			OutType = EAIRECommandType::Attack;
		}
		else if (Value == TEXT("Command.Switch"))
		{
			OutType = EAIRECommandType::Switch;
		}
		else
		{
			return false;
		}

		return true;
	}

	bool ParseCommandPriority(const FString& Value, EAIRECommandPriority& OutPriority)
	{
		if (Value == TEXT("Low"))
		{
			OutPriority = EAIRECommandPriority::Low;
		}
		else if (Value == TEXT("Normal"))
		{
			OutPriority = EAIRECommandPriority::Normal;
		}
		else if (Value == TEXT("High"))
		{
			OutPriority = EAIRECommandPriority::High;
		}
		else if (Value == TEXT("Critical"))
		{
			OutPriority = EAIRECommandPriority::Critical;
		}
		else
		{
			return false;
		}

		return true;
	}

	bool ParseUtcIso8601(const FString& Value, FDateTime& OutValue)
	{
		return !Value.IsEmpty()
			&& Value.EndsWith(TEXT("Z"), ESearchCase::CaseSensitive)
			&& FDateTime::ParseIso8601(*Value, OutValue);
	}

	bool ValidateCandidateParameterValue(
		const TSharedPtr<FJsonValue>& Value,
		const int32 Depth)
	{
		if (!Value.IsValid())
		{
			return false;
		}

		switch (Value->Type)
		{
		case EJson::String:
		{
			FString StringValue;
			return Value->TryGetString(StringValue)
				&& StringValue.Len() <= MaxCandidateParameterStringLength;
		}
		case EJson::Number:
		{
			double NumberValue = 0.0;
			return Value->TryGetNumber(NumberValue) && FMath::IsFinite(NumberValue);
		}
		case EJson::Boolean:
		case EJson::Null:
			return true;
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
			if (Depth > MaxCandidateParameterDepth
				|| !Value->TryGetArray(ArrayValue)
				|| ArrayValue == nullptr
				|| ArrayValue->Num() > MaxCandidateParameterArrayLength)
			{
				return false;
			}

			for (const TSharedPtr<FJsonValue>& NestedValue : *ArrayValue)
			{
				if (!ValidateCandidateParameterValue(NestedValue, Depth + 1))
				{
					return false;
				}
			}
			return true;
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject>* ObjectValue = nullptr;
			if (Depth > MaxCandidateParameterDepth
				|| !Value->TryGetObject(ObjectValue)
				|| ObjectValue == nullptr
				|| !ObjectValue->IsValid()
				|| (*ObjectValue)->Values.Num() > MaxCandidateParameterObjectProperties)
			{
				return false;
			}

			for (const auto& Field : (*ObjectValue)->Values)
			{
				if (!ValidateCandidateParameterValue(Field.Value, Depth + 1))
				{
					return false;
				}
			}
			return true;
		}
		default:
			return false;
		}
	}

	bool ParseCandidateParameters(
		const EAIRECommandType Type,
		const TSharedPtr<FJsonObject>& Parameters,
		FAIRECommandCandidate& OutCandidate)
	{
		if (!Parameters.IsValid()
			|| Parameters->Values.Num() > MaxCandidateParameterProperties
			|| !ValidateCandidateParameterValue(
				MakeShared<FJsonValueObject>(Parameters),
				0))
		{
			return false;
		}

		bool bHasResource = false;
		for (const auto& Field : Parameters->Values)
		{
			const FString FieldName(*Field.Key);
			if (FieldName.Len() > MaxCandidateParameterStringLength)
			{
				return false;
			}

			if (Type == EAIRECommandType::Attack && FieldName == TEXT("target_id"))
			{
				FString TargetId;
				if (!Field.Value.IsValid()
					|| Field.Value->Type != EJson::String
					|| !Field.Value->TryGetString(TargetId)
					|| !FAIREChatJsonAdapter::IsStableId(TargetId))
				{
					return false;
				}
				OutCandidate.ParameterTargetId = MoveTemp(TargetId);
				continue;
			}

			if (Type == EAIRECommandType::GatherResource && FieldName == TEXT("resource"))
			{
				FString Resource;
				if (!Field.Value.IsValid()
					|| Field.Value->Type != EJson::String
					|| !Field.Value->TryGetString(Resource))
				{
					return false;
				}

				bHasResource = true;
				if (Resource == TEXT("wood"))
				{
					OutCandidate.GatherResource = EAIREGatherResourceKind::Wood;
				}
				else if (Resource == TEXT("stone"))
				{
					OutCandidate.GatherResource = EAIREGatherResourceKind::Stone;
				}
				else
				{
					OutCandidate.bHasUnsupportedParameters = true;
				}
				continue;
			}

			if (Type == EAIRECommandType::GatherResource && FieldName == TEXT("quantity"))
			{
				double Quantity = 0.0;
				if (!Field.Value.IsValid()
					|| Field.Value->Type != EJson::Number
					|| !Field.Value->TryGetNumber(Quantity)
					|| !FMath::IsFinite(Quantity)
					|| FMath::TruncToDouble(Quantity) != Quantity
					|| Quantity < TNumericLimits<int32>::Lowest()
					|| Quantity > TNumericLimits<int32>::Max())
				{
					return false;
				}
				OutCandidate.GatherQuantity = static_cast<int32>(Quantity);
				OutCandidate.bHasGatherQuantity = true;
				continue;
			}

			OutCandidate.bHasUnsupportedParameters = true;
		}

		return Type != EAIRECommandType::GatherResource || bHasResource;
	}

	bool ParseCommandCandidate(
		const TSharedPtr<FJsonObject>& Object,
		const FString& ExpectedRequestId,
		FAIRECommandCandidate& OutCandidate)
	{
		if (!Object.IsValid())
		{
			return false;
		}

		for (const auto& Field : Object->Values)
		{
			const FString FieldName(*Field.Key);
			if (FieldName != TEXT("command_id")
				&& FieldName != TEXT("request_id")
				&& FieldName != TEXT("type")
				&& FieldName != TEXT("target_id")
				&& FieldName != TEXT("priority")
				&& FieldName != TEXT("issued_at")
				&& FieldName != TEXT("expires_at")
				&& FieldName != TEXT("parameters"))
			{
				return false;
			}
		}

		if (!Object->HasField(TEXT("command_id"))
			|| !Object->HasField(TEXT("request_id"))
			|| !Object->HasField(TEXT("type"))
			|| !Object->HasField(TEXT("issued_at"))
			|| !Object->HasField(TEXT("expires_at")))
		{
			return false;
		}
		const bool bHasTargetId = Object->HasField(TEXT("target_id"));

		FString CommandId;
		FString RequestId;
		FString TypeValue;
		FString PriorityValue;
		FString IssuedAt;
		FString ExpiresAt;
		if (!Object->TryGetStringField(TEXT("command_id"), CommandId)
			|| !FAIREChatJsonAdapter::IsStableId(CommandId)
			|| !Object->TryGetStringField(TEXT("request_id"), RequestId)
			|| !FAIREChatJsonAdapter::IsStableId(RequestId)
			|| RequestId != ExpectedRequestId
			|| !Object->TryGetStringField(TEXT("type"), TypeValue)
			|| !Object->TryGetStringField(TEXT("issued_at"), IssuedAt)
			|| !Object->TryGetStringField(TEXT("expires_at"), ExpiresAt))
		{
			return false;
		}
		if (Object->HasField(TEXT("priority"))
			&& !Object->TryGetStringField(TEXT("priority"), PriorityValue))
		{
			return false;
		}

		FAIRECommandCandidate Candidate;
		Candidate.CommandId = MoveTemp(CommandId);
		Candidate.RequestId = MoveTemp(RequestId);
		if (!ParseCommandType(TypeValue, Candidate.Type)
			|| !ParseUtcIso8601(IssuedAt, Candidate.IssuedAtUtc)
			|| !ParseUtcIso8601(ExpiresAt, Candidate.ExpiresAtUtc)
			|| Candidate.ExpiresAtUtc <= Candidate.IssuedAtUtc
			|| Candidate.ExpiresAtUtc - Candidate.IssuedAtUtc
				> FTimespan::FromSeconds(MaxCandidateLeaseSeconds))
		{
			return false;
		}
		if (Object->HasField(TEXT("priority"))
			&& !ParseCommandPriority(PriorityValue, Candidate.Priority))
		{
			return false;
		}

		if (bHasTargetId)
		{
			const TSharedPtr<FJsonValue> TargetValue = Object->TryGetField(TEXT("target_id"));
			if (TargetValue.IsValid() && !TargetValue->IsNull())
			{
				if (TargetValue->Type != EJson::String
					|| !TargetValue->TryGetString(Candidate.TargetId)
					|| !FAIREChatJsonAdapter::IsStableId(Candidate.TargetId))
				{
					return false;
				}
			}
		}

		TSharedPtr<FJsonObject> Parameters = MakeShared<FJsonObject>();
		if (Object->HasField(TEXT("parameters")))
		{
			const TSharedPtr<FJsonValue> ParametersValue = Object->TryGetField(TEXT("parameters"));
			const TSharedPtr<FJsonObject>* ParametersObject = nullptr;
			if (!ParametersValue.IsValid()
				|| ParametersValue->Type != EJson::Object
				|| !ParametersValue->TryGetObject(ParametersObject)
				|| ParametersObject == nullptr
				|| !ParametersObject->IsValid())
			{
				return false;
			}
			Parameters = *ParametersObject;
		}
		if (!ParseCandidateParameters(Candidate.Type, Parameters, Candidate))
		{
			return false;
		}

		OutCandidate = MoveTemp(Candidate);
		return true;
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

	bool IsValidAIMetadata(const TSharedPtr<FJsonObject>& Metadata)
	{
		if (!Metadata.IsValid())
		{
			return false;
		}

		FString Provider;
		FString ModelVersion;
		FString PromptVersion;
		return Metadata->TryGetStringField(TEXT("provider"), Provider)
			&& !Provider.TrimStartAndEnd().IsEmpty()
			&& Provider.Len() <= MaxAIProviderLength
			&& Metadata->TryGetStringField(TEXT("model_version"), ModelVersion)
			&& !ModelVersion.TrimStartAndEnd().IsEmpty()
			&& ModelVersion.Len() <= MaxAIModelVersionLength
			&& Metadata->TryGetStringField(TEXT("prompt_version"), PromptVersion)
			&& !PromptVersion.TrimStartAndEnd().IsEmpty()
			&& PromptVersion.Len() <= MaxAIPromptVersionLength;
	}

	FAIREParsedChatFrame ParseResponsePayload(
		const TSharedPtr<FJsonObject>& Payload,
		const FAIREChatResponseCorrelation& ExpectedCorrelation,
		const bool bIgnoreCorrelationMismatch)
	{
		if (!Payload.IsValid())
		{
			return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("InvalidChatResponse"), TEXT("Chat response payload is missing."));
		}

		FString RequestId;
		FString ResponseId;
		FString SessionId;
		FString SaveSlotId;
		FString CompanionId;
		FString DisplayText;
		const TSharedPtr<FJsonObject>* AIMetadata = nullptr;
		if (!Payload->TryGetStringField(TEXT("request_id"), RequestId)
			|| !FAIREChatJsonAdapter::IsStableId(RequestId)
			|| !Payload->TryGetStringField(TEXT("session_id"), SessionId)
			|| !FAIREChatJsonAdapter::IsStableId(SessionId)
			|| !Payload->TryGetStringField(TEXT("save_slot_id"), SaveSlotId)
			|| !FAIREChatJsonAdapter::IsStableId(SaveSlotId)
			|| !Payload->TryGetStringField(TEXT("companion_id"), CompanionId)
			|| !FAIREChatJsonAdapter::IsStableId(CompanionId)
			|| !Payload->TryGetStringField(TEXT("response_id"), ResponseId)
			|| !FAIREChatJsonAdapter::IsStableId(ResponseId)
			|| !Payload->TryGetStringField(TEXT("display_text"), DisplayText)
			|| DisplayText.TrimStartAndEnd().IsEmpty()
			|| DisplayText.Len() > MaxDisplayTextLength
			|| !Payload->TryGetObjectField(TEXT("ai_metadata"), AIMetadata)
			|| AIMetadata == nullptr
			|| !IsValidAIMetadata(*AIMetadata))
		{
			return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("InvalidChatResponse"), TEXT("Chat response validation failed."));
		}

		FString MessageId;
		const TSharedPtr<FJsonValue> MessageIdValue = Payload->TryGetField(TEXT("message_id"));
		if (MessageIdValue.IsValid()
			&& MessageIdValue->Type != EJson::Null
			&& (!Payload->TryGetStringField(TEXT("message_id"), MessageId)
				|| !FAIREChatJsonAdapter::IsStableId(MessageId)))
		{
			return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("InvalidChatResponse"), TEXT("Chat response message_id is invalid."));
		}

		const bool bHasCorrelationMismatch = RequestId != ExpectedCorrelation.RequestId
			|| SessionId != ExpectedCorrelation.SessionId
			|| SaveSlotId != ExpectedCorrelation.SaveSlotId
			|| CompanionId != ExpectedCorrelation.CompanionId
			|| (!MessageId.IsEmpty() && MessageId != ExpectedCorrelation.MessageId);
		if (bHasCorrelationMismatch)
		{
			if (bIgnoreCorrelationMismatch)
			{
				FAIREParsedChatFrame Frame;
				Frame.Kind = EAIREParsedChatFrameKind::Ignored;
				return Frame;
			}

			return InvalidFrame(
				ExpectedCorrelation.RequestId,
				TEXT("ResponseCorrelationMismatch"),
				TEXT("HTTP Chat response does not match the active request."));
		}

		FAIREParsedChatFrame Frame;
		Frame.Kind = EAIREParsedChatFrameKind::Response;
		Frame.Result.RequestId = MoveTemp(RequestId);
		Frame.Result.ResponseId = MoveTemp(ResponseId);
		Frame.Result.DisplayText = MoveTemp(DisplayText);

		TArray<FAIRECommandCandidate> CommandCandidates;
		const TSharedPtr<FJsonValue> CandidatesValue = Payload->TryGetField(TEXT("command_candidates"));
		if (CandidatesValue.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
			if (CandidatesValue->Type != EJson::Array
				|| !CandidatesValue->TryGetArray(Candidates)
				|| Candidates == nullptr
				|| Candidates->Num() > MaxCommandCandidates)
			{
				return InvalidFrame(
					ExpectedCorrelation.RequestId,
					TEXT("InvalidCommandCandidates"),
					TEXT("Chat command_candidates must be an array of at most four candidates."));
			}

			CommandCandidates.Reserve(Candidates->Num());
			for (const TSharedPtr<FJsonValue>& CandidateValue : *Candidates)
			{
				const TSharedPtr<FJsonObject>* CandidateObject = nullptr;
				FAIRECommandCandidate Candidate;
				if (!CandidateValue.IsValid()
					|| CandidateValue->Type != EJson::Object
					|| !CandidateValue->TryGetObject(CandidateObject)
					|| CandidateObject == nullptr
					|| !CandidateObject->IsValid()
					|| !ParseCommandCandidate(*CandidateObject, ExpectedCorrelation.RequestId, Candidate))
				{
					return InvalidFrame(
						ExpectedCorrelation.RequestId,
						TEXT("InvalidCommandCandidates"),
						TEXT("Chat command candidate validation failed."));
				}

				CommandCandidates.Add(MoveTemp(Candidate));
			}
		}

		Frame.Result.CommandCandidates = MoveTemp(CommandCandidates);
		return Frame;
	}

	FAIREParsedChatFrame ParseErrorPayload(
		const TSharedPtr<FJsonObject>& Payload,
		const FAIREChatResponseCorrelation& ExpectedCorrelation,
		const bool bIgnoreCorrelationMismatch)
	{
		if (!Payload.IsValid())
		{
			return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("InvalidErrorResponse"), TEXT("Error payload is missing."));
		}

		FString RequestId;
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (!Payload->TryGetStringField(TEXT("request_id"), RequestId)
			|| !FAIREChatJsonAdapter::IsStableId(RequestId)
			|| !Payload->TryGetObjectField(TEXT("error"), ErrorObject)
			|| ErrorObject == nullptr
			|| !ErrorObject->IsValid())
		{
			return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("InvalidErrorResponse"), TEXT("Error response validation failed."));
		}
		if (RequestId != ExpectedCorrelation.RequestId)
		{
			if (bIgnoreCorrelationMismatch)
			{
				FAIREParsedChatFrame Frame;
				Frame.Kind = EAIREParsedChatFrameKind::Ignored;
				return Frame;
			}

			return InvalidFrame(
				ExpectedCorrelation.RequestId,
				TEXT("ResponseCorrelationMismatch"),
				TEXT("HTTP Chat error does not match the active request."));
		}

		FString Code;
		FString Message;
		bool bRetryable = false;
		const TSharedPtr<FJsonObject>* Details = nullptr;
		if (!(*ErrorObject)->TryGetStringField(TEXT("code"), Code)
			|| Code.TrimStartAndEnd().IsEmpty()
			|| Code.Len() > MaxErrorCodeLength
			|| !(*ErrorObject)->TryGetStringField(TEXT("message"), Message)
			|| Message.TrimStartAndEnd().IsEmpty()
			|| Message.Len() > MaxErrorMessageLength
			|| !(*ErrorObject)->TryGetBoolField(TEXT("retryable"), bRetryable)
			|| !(*ErrorObject)->TryGetObjectField(TEXT("details"), Details)
			|| Details == nullptr
			|| !Details->IsValid())
		{
			return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("InvalidErrorResponse"), TEXT("Error response validation failed."));
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
	TimeContext->SetNumberField(TEXT("hour"), FMath::FloorToInt(Context.Hour));
	TimeContext->SetStringField(TEXT("period"), PeriodName);

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("schema_version"), SchemaVersion);
	Payload->SetStringField(TEXT("request_id"), RequestId);
	Payload->SetStringField(TEXT("save_slot_id"), Context.SaveSlotId);
	Payload->SetStringField(TEXT("companion_id"), CompanionId);
	Payload->SetStringField(TEXT("session_id"), SessionId);
	Payload->SetStringField(TEXT("surface"), TEXT("game"));
	Payload->SetStringField(TEXT("message_id"), MessageId);
	Payload->SetStringField(TEXT("user_message"), TrimmedMessage);
	Payload->SetObjectField(TEXT("time_context"), TimeContext);
	Payload->SetArrayField(
		TEXT("recent_event_ids"),
		TArray<TSharedPtr<FJsonValue>>());
	Payload->SetObjectField(TEXT("game_context"), MakeShared<FJsonObject>());
	TArray<TSharedPtr<FJsonValue>> AllowedCommands;
	AllowedCommands.Reserve(UE_ARRAY_COUNT(FixedAllowedCommands));
	for (const TCHAR* Command : FixedAllowedCommands)
	{
		AllowedCommands.Add(MakeShared<FJsonValueString>(Command));
	}
	Payload->SetArrayField(TEXT("allowed_commands"), MoveTemp(AllowedCommands));
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
	const FAIREChatResponseCorrelation& ExpectedCorrelation)
{
	TSharedPtr<FJsonObject> FrameObject;
	if (!DeserializeChatObject(Message, FrameObject))
	{
		return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("MalformedJson"), TEXT("WebSocket response is not valid JSON."));
	}

	FString Type;
	if (!FrameObject->TryGetStringField(TEXT("type"), Type) || Type.IsEmpty())
	{
		return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("InvalidFrame"), TEXT("WebSocket response type is missing."));
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
		return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("InvalidFrame"), TEXT("WebSocket response payload is missing."));
	}

	return Type == TEXT("chat_response")
		? ParseResponsePayload(*Payload, ExpectedCorrelation, true)
		: ParseErrorPayload(*Payload, ExpectedCorrelation, true);
}

FAIREParsedChatFrame FAIREChatJsonAdapter::ParseHttpBody(
	const FString& Message,
	const FAIREChatResponseCorrelation& ExpectedCorrelation,
	const bool bIsErrorResponse)
{
	TSharedPtr<FJsonObject> Body;
	if (!DeserializeChatObject(Message, Body))
	{
		return InvalidFrame(ExpectedCorrelation.RequestId, TEXT("MalformedJson"), TEXT("HTTP response is not valid JSON."));
	}

	return bIsErrorResponse
		? ParseErrorPayload(Body, ExpectedCorrelation, false)
		: ParseResponsePayload(Body, ExpectedCorrelation, false);
}

bool FAIREChatJsonAdapter::IsStableId(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > ChatMaxStableIdLength)
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

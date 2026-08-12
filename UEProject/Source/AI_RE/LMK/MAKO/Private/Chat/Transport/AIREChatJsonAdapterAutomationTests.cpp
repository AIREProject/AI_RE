#if WITH_DEV_AUTOMATION_TESTS

#include "Chat/Transport/AIREChatJsonAdapter.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* const ExpectedAllowedCommands[] =
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

	FString SerializeObject(const TSharedPtr<FJsonObject>& Object)
	{
		FString Json;
		if (!Object.IsValid())
		{
			return Json;
		}
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return Json;
	}

	TSharedPtr<FJsonObject> MakeResponse()
	{
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetStringField(TEXT("request_id"), TEXT("request-1"));
		Response->SetStringField(TEXT("message_id"), TEXT("message-1"));
		Response->SetStringField(TEXT("session_id"), TEXT("session-1"));
		Response->SetStringField(TEXT("save_slot_id"), TEXT("slot-1"));
		Response->SetStringField(TEXT("companion_id"), TEXT("mako"));
		Response->SetStringField(TEXT("response_id"), TEXT("response-1"));
		Response->SetStringField(TEXT("display_text"), TEXT("알겠어."));

		TSharedPtr<FJsonObject> AIMetadata = MakeShared<FJsonObject>();
		AIMetadata->SetStringField(TEXT("provider"), TEXT("mock"));
		AIMetadata->SetStringField(TEXT("model_version"), TEXT("test"));
		AIMetadata->SetStringField(TEXT("prompt_version"), TEXT("test"));
		Response->SetObjectField(TEXT("ai_metadata"), AIMetadata);
		return Response;
	}

	TSharedPtr<FJsonObject> MakeCandidate(
		const TCHAR* CommandId,
		const TCHAR* Type,
		const TSharedPtr<FJsonObject>& Parameters = nullptr,
		const bool bIncludePriority = true)
	{
		TSharedPtr<FJsonObject> Candidate = MakeShared<FJsonObject>();
		Candidate->SetStringField(TEXT("command_id"), CommandId);
		Candidate->SetStringField(TEXT("request_id"), TEXT("request-1"));
		Candidate->SetStringField(TEXT("type"), Type);
		Candidate->SetStringField(TEXT("issued_at"), TEXT("2026-01-01T00:00:00Z"));
		Candidate->SetStringField(TEXT("expires_at"), TEXT("2026-01-01T00:00:30Z"));
		if (bIncludePriority)
		{
			Candidate->SetStringField(TEXT("priority"), TEXT("Normal"));
		}
		if (Parameters.IsValid())
		{
			Candidate->SetObjectField(TEXT("parameters"), Parameters);
		}
		return Candidate;
	}

	void SetCandidates(
		const TSharedPtr<FJsonObject>& Response,
		const TArray<TSharedPtr<FJsonObject>>& Candidates)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Candidates.Num());
		for (const TSharedPtr<FJsonObject>& Candidate : Candidates)
		{
			Values.Add(MakeShared<FJsonValueObject>(Candidate));
		}
		Response->SetArrayField(TEXT("command_candidates"), MoveTemp(Values));
	}

	FAIREChatResponseCorrelation MakeCorrelation()
	{
		FAIREChatResponseCorrelation Correlation;
		Correlation.RequestId = TEXT("request-1");
		Correlation.MessageId = TEXT("message-1");
		Correlation.SessionId = TEXT("session-1");
		Correlation.SaveSlotId = TEXT("slot-1");
		Correlation.CompanionId = TEXT("mako");
		return Correlation;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREChatJsonAdapterCommandCandidatesTest,
	"AIRE.Companion.Chat.JsonAdapter.CommandCandidates",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREChatJsonAdapterCommandCandidatesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FAIREInGameChatContext Context;
	Context.SaveSlotId = TEXT("slot-1");
	Context.Day = 1;
	Context.Hour = 12.0f;
	Context.Period = EAIREGameWorldPeriod::Morning;
	FAIREWorldContextV1 WorldContext;
	WorldContext.LocationId = TEXT("forest_camp");
	FString HttpBody;
	FString WebSocketFrame;
	FString BuildError;
	TestTrue(
		TEXT("In-game request serializes successfully"),
		FAIREChatJsonAdapter::BuildInGameRequest(
			Context,
			WorldContext,
			TEXT("mako"),
			TEXT("session-1"),
			TEXT("request-1"),
			TEXT("message-1"),
			TEXT("hello"),
			HttpBody,
			WebSocketFrame,
			BuildError));

	TSharedPtr<FJsonObject> Request;
	const TSharedRef<TJsonReader<>> RequestReader = TJsonReaderFactory<>::Create(HttpBody);
	TestTrue(
		TEXT("Serialized request parses as JSON"),
		FJsonSerializer::Deserialize(RequestReader, Request) && Request.IsValid());
	if (!Request.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* AllowedCommands = nullptr;
	TestTrue(
		TEXT("allowed_commands is an array"),
		Request->TryGetArrayField(TEXT("allowed_commands"), AllowedCommands)
			&& AllowedCommands != nullptr);
	if (AllowedCommands == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("allowed_commands contains exactly ten commands"), AllowedCommands->Num(), 10);
	for (int32 Index = 0; Index < AllowedCommands->Num() && Index < 10; ++Index)
	{
		FString Command;
		TestTrue(
			TEXT("allowed_commands entry is a string"),
			(*AllowedCommands)[Index].IsValid()
				&& (*AllowedCommands)[Index]->TryGetString(Command));
		TestEqual(TEXT("allowed_commands preserves fixed command order"), Command, FString(ExpectedAllowedCommands[Index]));
	}

	const FAIREChatResponseCorrelation Correlation = MakeCorrelation();
	TSharedPtr<FJsonObject> GatherParameters = MakeShared<FJsonObject>();
	GatherParameters->SetStringField(TEXT("resource"), TEXT("wood"));
	GatherParameters->SetNumberField(TEXT("quantity"), 7);
	TSharedPtr<FJsonObject> GatherCandidate = MakeCandidate(
		TEXT("command-gather"),
		TEXT("Command.GatherResource"),
		GatherParameters);
	TSharedPtr<FJsonObject> AttackParameters = MakeShared<FJsonObject>();
	AttackParameters->SetStringField(TEXT("target_id"), TEXT("enemy-1"));
	TSharedPtr<FJsonObject> AttackCandidate = MakeCandidate(
		TEXT("command-attack"),
		TEXT("Command.Attack"),
		AttackParameters);
	TSharedPtr<FJsonObject> Response = MakeResponse();
	SetCandidates(Response, {GatherCandidate, AttackCandidate});
	FAIREParsedChatFrame Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(TEXT("Valid command candidates produce a response"), Parsed.Kind == EAIREParsedChatFrameKind::Response);
	TestEqual(TEXT("Two valid command candidates are returned"), Parsed.Result.CommandCandidates.Num(), 2);
	if (Parsed.Result.CommandCandidates.Num() == 2)
	{
		const FAIRECommandCandidate& Gather = Parsed.Result.CommandCandidates[0];
		TestTrue(TEXT("Gather candidate type is parsed"), Gather.Type == EAIRECommandType::GatherResource);
		TestTrue(TEXT("Gather resource wood is parsed"), Gather.GatherResource == EAIREGatherResourceKind::Wood);
		TestTrue(TEXT("Gather quantity is present"), Gather.bHasGatherQuantity);
		TestEqual(TEXT("Gather quantity is integral"), Gather.GatherQuantity, 7);
		const FAIRECommandCandidate& Attack = Parsed.Result.CommandCandidates[1];
		TestTrue(TEXT("Attack candidate type is parsed"), Attack.Type == EAIRECommandType::Attack);
		TestEqual(TEXT("Attack target_id parameter is parsed"), Attack.ParameterTargetId, FString(TEXT("enemy-1")));
	}

	const TCHAR* const OtherBackendGeneratedTypes[] =
	{
		TEXT("Command.Follow"),
		TEXT("Command.HoldPosition"),
		TEXT("Command.CancelCurrent"),
		TEXT("Command.Switch"),
	};
	for (int32 Index = 0;
		Index < UE_ARRAY_COUNT(OtherBackendGeneratedTypes);
		++Index)
	{
		Response = MakeResponse();
		SetCandidates(
			Response,
			{MakeCandidate(
				*FString::Printf(TEXT("command-generated-%d"), Index),
				OtherBackendGeneratedTypes[Index])});
		Parsed = FAIREChatJsonAdapter::ParseHttpBody(
			SerializeObject(Response),
			Correlation,
			false);
		TestTrue(
			TEXT("Every non-parameterized backend-generated type parses"),
			Parsed.Kind == EAIREParsedChatFrameKind::Response);
	}

	TSharedPtr<FJsonObject> OptionalAttack = MakeCandidate(
		TEXT("command-optional"),
		TEXT("Command.Attack"),
		nullptr,
		false);
	Response = MakeResponse();
	SetCandidates(Response, {OptionalAttack});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(SerializeObject(Response), Correlation, false);
	TestTrue(TEXT("Optional priority and parameters use backend defaults"), Parsed.Kind == EAIREParsedChatFrameKind::Response);
	if (Parsed.Result.CommandCandidates.Num() == 1)
	{
		TestTrue(
			TEXT("Omitted priority defaults to Normal"),
			Parsed.Result.CommandCandidates[0].Priority == EAIRECommandPriority::Normal);
	}

	TSharedPtr<FJsonObject> UnsupportedParameters = MakeShared<FJsonObject>();
	UnsupportedParameters->SetStringField(TEXT("resource"), TEXT("wood"));
	UnsupportedParameters->SetBoolField(TEXT("future_flag"), true);
	TSharedPtr<FJsonObject> UnsupportedCandidate = MakeCandidate(
		TEXT("command-extra"),
		TEXT("Command.GatherResource"),
		UnsupportedParameters);
	Response = MakeResponse();
	SetCandidates(Response, {UnsupportedCandidate});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(SerializeObject(Response), Correlation, false);
	TestTrue(TEXT("Unknown parameters do not invalidate structure"), Parsed.Kind == EAIREParsedChatFrameKind::Response);
	if (Parsed.Result.CommandCandidates.Num() == 1)
	{
		TestTrue(
			TEXT("Unknown parameters are surfaced to the gateway"),
			Parsed.Result.CommandCandidates[0].bHasUnsupportedParameters);
	}

	Response = MakeResponse();
	const TSharedPtr<FJsonObject> ValidCandidate = MakeCandidate(
		TEXT("command-valid"),
		TEXT("Command.Attack"));
	TSharedPtr<FJsonObject> MalformedCandidate = MakeCandidate(
		TEXT("command-malformed"),
		TEXT("Command.Attack"));
	MalformedCandidate->SetStringField(TEXT("request_id"), TEXT("other-request"));
	SetCandidates(Response, {ValidCandidate, MalformedCandidate});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(SerializeObject(Response), Correlation, false);
	TestTrue(TEXT("One malformed candidate invalidates the candidate frame"), Parsed.Kind == EAIREParsedChatFrameKind::Invalid);
	TestEqual(TEXT("Malformed candidate frame reports its contract error"), Parsed.Error.Code, FString(TEXT("InvalidCommandCandidates")));
	TestEqual(TEXT("Malformed candidate frame returns no partial candidates"), Parsed.Result.CommandCandidates.Num(), 0);

	Response = MakeResponse();
	TSharedPtr<FJsonObject> OversizedLease = MakeCandidate(
		TEXT("command-long-lease"),
		TEXT("Command.HoldPosition"));
	OversizedLease->SetStringField(
		TEXT("expires_at"),
		TEXT("2026-01-01T00:01:01Z"));
	SetCandidates(Response, {OversizedLease});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("A lease longer than sixty seconds invalidates the frame"),
		Parsed.Kind == EAIREParsedChatFrameKind::Invalid);

	Response = MakeResponse();
	SetCandidates(
		Response,
		{MakeCandidate(TEXT("command-unknown"), TEXT("Command.Unknown"))});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("An unknown command enum invalidates the frame"),
		Parsed.Kind == EAIREParsedChatFrameKind::Invalid);

	Response = MakeResponse();
	TSharedPtr<FJsonObject> UnknownPriority = MakeCandidate(
		TEXT("command-priority"),
		TEXT("Command.HoldPosition"));
	UnknownPriority->SetStringField(TEXT("priority"), TEXT("Urgent"));
	SetCandidates(Response, {UnknownPriority});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("An unknown priority invalidates the frame"),
		Parsed.Kind == EAIREParsedChatFrameKind::Invalid);

	Response = MakeResponse();
	TSharedPtr<FJsonObject> OversizedParameters = MakeShared<FJsonObject>();
	OversizedParameters->SetStringField(
		TEXT("future_value"),
		FString::ChrN(257, TEXT('x')));
	SetCandidates(
		Response,
		{MakeCandidate(
			TEXT("command-oversized"),
			TEXT("Command.MoveToLocation"),
			OversizedParameters)});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("An oversized parameter string invalidates the frame"),
		Parsed.Kind == EAIREParsedChatFrameKind::Invalid);

	Response = MakeResponse();
	SetCandidates(Response, {});
	Response->RemoveField(TEXT("command_candidates"));
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(SerializeObject(Response), Correlation, false);
	TestTrue(TEXT("Missing command_candidates is accepted"), Parsed.Kind == EAIREParsedChatFrameKind::Response);
	TestEqual(TEXT("Missing command_candidates yields an empty list"), Parsed.Result.CommandCandidates.Num(), 0);

	Response = MakeResponse();
	SetCandidates(Response, {ValidCandidate, ValidCandidate, ValidCandidate, ValidCandidate, ValidCandidate});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(SerializeObject(Response), Correlation, false);
	TestTrue(TEXT("More than four candidates are rejected"), Parsed.Kind == EAIREParsedChatFrameKind::Invalid);
	TestEqual(TEXT("Candidate count rejection reports its contract error"), Parsed.Error.Code, FString(TEXT("InvalidCommandCandidates")));

	return true;
}

#endif

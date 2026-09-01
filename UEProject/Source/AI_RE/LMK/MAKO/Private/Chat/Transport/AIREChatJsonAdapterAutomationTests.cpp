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
	Context.Period = EAIREGameWorldPeriod::Afternoon;
	FAIREWorldContextV1 WorldContext;
	WorldContext.LocationId = TEXT("forest_camp");
	WorldContext.NearbyResources.Add({TEXT("wood"), 1});
	WorldContext.AvailableWorkstations.Add(TEXT("Workbench.Blacksmith"));
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
			true,
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
	const TSharedPtr<FJsonObject>* TimeContext = nullptr;
	TestTrue(
		TEXT("In-game request contains a time context"),
		Request->TryGetObjectField(TEXT("time_context"), TimeContext)
			&& TimeContext != nullptr
			&& TimeContext->IsValid());
	if (TimeContext == nullptr || !TimeContext->IsValid())
	{
		return false;
	}
	FString TimeSource;
	TestTrue(
		TEXT("In-game Chat time context source is RealWorld"),
		(*TimeContext)->TryGetStringField(TEXT("source"), TimeSource));
	TestEqual(
		TEXT("In-game Chat preserves the PC real-world time source"),
		TimeSource,
		FString(TEXT("RealWorld")));
	double SerializedDay = 0.0;
	double SerializedHour = 0.0;
	TestTrue(
		TEXT("In-game Chat time context preserves the supplied real-world day"),
		(*TimeContext)->TryGetNumberField(TEXT("day"), SerializedDay));
	TestTrue(
		TEXT("In-game Chat time context preserves the supplied real-world hour"),
		(*TimeContext)->TryGetNumberField(TEXT("hour"), SerializedHour));
	TestEqual(TEXT("Serialized real-world day matches context"), SerializedDay, 1.0);
	TestEqual(TEXT("Serialized real-world hour matches context"), SerializedHour, 12.0);
	const TArray<TSharedPtr<FJsonValue>>* AllowedCommands = nullptr;
	TestTrue(
		TEXT("allowed_commands is an array"),
		Request->TryGetArrayField(TEXT("allowed_commands"), AllowedCommands)
			&& AllowedCommands != nullptr);
	if (AllowedCommands == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("allowed_commands contains exactly eleven commands"), AllowedCommands->Num(), 11);
	for (int32 Index = 0;
		Index < AllowedCommands->Num()
			&& Index < UE_ARRAY_COUNT(ExpectedAllowedCommands);
		++Index)
	{
		FString Command;
		TestTrue(
			TEXT("allowed_commands entry is a string"),
			(*AllowedCommands)[Index].IsValid()
				&& (*AllowedCommands)[Index]->TryGetString(Command));
		TestEqual(TEXT("allowed_commands preserves fixed command order"), Command, FString(ExpectedAllowedCommands[Index]));
	}
	FString GatherCommand;
	TestTrue(
		TEXT("GatherResource is advertised when wood is nearby"),
		(*AllowedCommands)[9]->TryGetString(GatherCommand));
	TestEqual(
		TEXT("GatherResource is the first appended capability"),
		GatherCommand,
		FString(TEXT("Command.GatherResource")));
	FString CraftCommand;
	TestTrue(
		TEXT("CraftItem is advertised when a Blacksmith is available"),
		(*AllowedCommands)[10]->TryGetString(CraftCommand));
	TestEqual(
		TEXT("CraftItem is the appended capability"),
		CraftCommand,
		FString(TEXT("Command.CraftItem")));

	FString RequestWithoutCraftRuntime;
	FString FrameWithoutCraftRuntime;
	TestTrue(
		TEXT("Request without valid Craft runtime serializes successfully"),
		FAIREChatJsonAdapter::BuildInGameRequest(
			Context,
			WorldContext,
			TEXT("mako"),
			TEXT("session-1"),
			TEXT("request-3"),
			TEXT("message-3"),
			TEXT("hello"),
			false,
			RequestWithoutCraftRuntime,
			FrameWithoutCraftRuntime,
			BuildError));
	TSharedPtr<FJsonObject> RequestWithoutCraftRuntimeObject;
	const TSharedRef<TJsonReader<>> RequestWithoutCraftRuntimeReader =
		TJsonReaderFactory<>::Create(RequestWithoutCraftRuntime);
	TestTrue(
		TEXT("Request without valid Craft runtime parses as JSON"),
		FJsonSerializer::Deserialize(
			RequestWithoutCraftRuntimeReader,
			RequestWithoutCraftRuntimeObject)
			&& RequestWithoutCraftRuntimeObject.IsValid());
	const TArray<TSharedPtr<FJsonValue>>* CommandsWithoutCraftRuntime = nullptr;
	TestTrue(
		TEXT("CraftItem is not advertised without valid Craft runtime"),
		RequestWithoutCraftRuntimeObject.IsValid()
			&& RequestWithoutCraftRuntimeObject->TryGetArrayField(
				TEXT("allowed_commands"),
				CommandsWithoutCraftRuntime)
			&& CommandsWithoutCraftRuntime != nullptr
			&& CommandsWithoutCraftRuntime->Num() == 10);

	FAIREWorldContextV1 ContextWithoutWorkbench = WorldContext;
	ContextWithoutWorkbench.AvailableWorkstations.Reset();
	FString RequestWithoutWorkbench;
	FString FrameWithoutWorkbench;
	TestTrue(
		TEXT("Request without a nearby Blacksmith serializes successfully"),
		FAIREChatJsonAdapter::BuildInGameRequest(
			Context,
			ContextWithoutWorkbench,
			TEXT("mako"),
			TEXT("session-1"),
			TEXT("request-2"),
			TEXT("message-2"),
			TEXT("hello"),
			true,
			RequestWithoutWorkbench,
			FrameWithoutWorkbench,
			BuildError));
	TSharedPtr<FJsonObject> RequestWithoutWorkbenchObject;
	const TSharedRef<TJsonReader<>> RequestWithoutWorkbenchReader =
		TJsonReaderFactory<>::Create(RequestWithoutWorkbench);
	TestTrue(
		TEXT("Request without a nearby Blacksmith parses as JSON"),
		FJsonSerializer::Deserialize(
			RequestWithoutWorkbenchReader,
			RequestWithoutWorkbenchObject)
			&& RequestWithoutWorkbenchObject.IsValid());
	const TArray<TSharedPtr<FJsonValue>>* CommandsWithoutWorkbench = nullptr;
	TestTrue(
		TEXT("CraftItem is not advertised without a nearby Blacksmith"),
		RequestWithoutWorkbenchObject.IsValid()
			&& RequestWithoutWorkbenchObject->TryGetArrayField(
				TEXT("allowed_commands"),
				CommandsWithoutWorkbench)
			&& CommandsWithoutWorkbench != nullptr
			&& CommandsWithoutWorkbench->Num() == 10);

	FAIREWorldContextV1 ContextWithoutWood = WorldContext;
	ContextWithoutWood.NearbyResources.Reset();
	FString RequestWithoutWood;
	FString FrameWithoutWood;
	TestTrue(
		TEXT("Request without nearby wood serializes"),
		FAIREChatJsonAdapter::BuildInGameRequest(
			Context,
			ContextWithoutWood,
			TEXT("mako"),
			TEXT("session-1"),
			TEXT("request-4"),
			TEXT("message-4"),
			TEXT("hello"),
			true,
			RequestWithoutWood,
			FrameWithoutWood,
			BuildError));
	TSharedPtr<FJsonObject> RequestWithoutWoodObject;
	const TSharedRef<TJsonReader<>> RequestWithoutWoodReader =
		TJsonReaderFactory<>::Create(RequestWithoutWood);
	FJsonSerializer::Deserialize(
		RequestWithoutWoodReader,
		RequestWithoutWoodObject);
	const TArray<TSharedPtr<FJsonValue>>* CommandsWithoutWood = nullptr;
	TestTrue(
		TEXT("GatherResource is not advertised without wood"),
		RequestWithoutWoodObject.IsValid()
			&& RequestWithoutWoodObject->TryGetArrayField(
				TEXT("allowed_commands"),
				CommandsWithoutWood)
			&& CommandsWithoutWood != nullptr
			&& CommandsWithoutWood->Num() == 10
			&& !CommandsWithoutWood->ContainsByPredicate(
				[](const TSharedPtr<FJsonValue>& Value)
				{
					FString Command;
					return Value.IsValid()
						&& Value->TryGetString(Command)
						&& Command == TEXT("Command.GatherResource");
				}));

	const FAIREChatResponseCorrelation Correlation = MakeCorrelation();
	TSharedPtr<FJsonObject> GatherParameters = MakeShared<FJsonObject>();
	GatherParameters->SetStringField(TEXT("resource"), TEXT("wood"));
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
		TestFalse(TEXT("Gather quantity is absent"), Gather.bHasGatherQuantity);
		const FAIRECommandCandidate& Attack = Parsed.Result.CommandCandidates[1];
		TestTrue(TEXT("Attack candidate type is parsed"), Attack.Type == EAIRECommandType::Attack);
		TestEqual(TEXT("Attack target_id parameter is parsed"), Attack.ParameterTargetId, FString(TEXT("enemy-1")));
	}

	TSharedPtr<FJsonObject> GatherWithQuantity = MakeShared<FJsonObject>();
	GatherWithQuantity->SetStringField(TEXT("resource"), TEXT("wood"));
	GatherWithQuantity->SetNumberField(TEXT("quantity"), 1);
	Response = MakeResponse();
	SetCandidates(
		Response,
		{MakeCandidate(
			TEXT("command-gather-quantity"),
			TEXT("Command.GatherResource"),
			GatherWithQuantity)});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("Gather quantity is marked unsupported"),
		Parsed.Kind == EAIREParsedChatFrameKind::Response
			&& Parsed.Result.CommandCandidates.Num() == 1
			&& Parsed.Result.CommandCandidates[0].bHasUnsupportedParameters);

	TSharedPtr<FJsonObject> StoneParameters = MakeShared<FJsonObject>();
	StoneParameters->SetStringField(TEXT("resource"), TEXT("stone"));
	Response = MakeResponse();
	SetCandidates(
		Response,
		{MakeCandidate(
			TEXT("command-gather-stone"),
			TEXT("Command.GatherResource"),
			StoneParameters)});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("Stone Gather is supported"),
		Parsed.Kind == EAIREParsedChatFrameKind::Response
			&& Parsed.Result.CommandCandidates.Num() == 1
			&& !Parsed.Result.CommandCandidates[0].bHasUnsupportedParameters
			&& Parsed.Result.CommandCandidates[0].GatherResource
				== EAIREGatherResourceKind::Stone);

	TSharedPtr<FJsonObject> IronParameters = MakeShared<FJsonObject>();
	IronParameters->SetStringField(TEXT("resource"), TEXT("iron_ore"));
	Response = MakeResponse();
	SetCandidates(
		Response,
		{MakeCandidate(
			TEXT("command-gather-iron"),
			TEXT("Command.GatherResource"),
			IronParameters)});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("Iron ore Gather is supported"),
		Parsed.Kind == EAIREParsedChatFrameKind::Response
			&& Parsed.Result.CommandCandidates.Num() == 1
			&& !Parsed.Result.CommandCandidates[0].bHasUnsupportedParameters
			&& Parsed.Result.CommandCandidates[0].GatherResource
				== EAIREGatherResourceKind::IronOre);

	TSharedPtr<FJsonObject> CraftParameters = MakeShared<FJsonObject>();
	CraftParameters->SetStringField(TEXT("recipe_id"), TEXT("recipe-11"));
	CraftParameters->SetNumberField(TEXT("quantity"), 1);
	Response = MakeResponse();
	SetCandidates(
		Response,
		{MakeCandidate(
			TEXT("command-craft"),
			TEXT("Command.CraftItem"),
			CraftParameters)});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("A strict CraftItem candidate parses"),
		Parsed.Kind == EAIREParsedChatFrameKind::Response);
	if (Parsed.Result.CommandCandidates.Num() == 1)
	{
		const FAIRECommandCandidate& Craft = Parsed.Result.CommandCandidates[0];
		TestTrue(
			TEXT("CraftItem candidate type is parsed"),
			Craft.Type == EAIRECommandType::CraftItem);
		TestEqual(
			TEXT("CraftItem stable recipe id is parsed"),
			Craft.CraftRecipeId,
			FString(TEXT("recipe-11")));
		TestTrue(TEXT("CraftItem quantity is present"), Craft.bHasCraftQuantity);
		TestEqual(TEXT("CraftItem quantity is fixed to one"), Craft.CraftQuantity, 1);
	}

	TSharedPtr<FJsonObject> InvalidCraftParameters = MakeShared<FJsonObject>();
	InvalidCraftParameters->SetStringField(TEXT("recipe_id"), TEXT("recipe-11"));
	InvalidCraftParameters->SetNumberField(TEXT("quantity"), 2);
	Response = MakeResponse();
	SetCandidates(
		Response,
		{MakeCandidate(
			TEXT("command-invalid-craft"),
			TEXT("Command.CraftItem"),
			InvalidCraftParameters)});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("CraftItem quantity other than one invalidates the frame"),
		Parsed.Kind == EAIREParsedChatFrameKind::Invalid);

	TSharedPtr<FJsonObject> CraftWithExtraParameter =
		MakeShared<FJsonObject>();
	CraftWithExtraParameter->SetStringField(
		TEXT("recipe_id"),
		TEXT("recipe-11"));
	CraftWithExtraParameter->SetNumberField(TEXT("quantity"), 1);
	CraftWithExtraParameter->SetBoolField(TEXT("future_flag"), true);
	Response = MakeResponse();
	SetCandidates(
		Response,
		{MakeCandidate(
			TEXT("command-extra-craft"),
			TEXT("Command.CraftItem"),
			CraftWithExtraParameter)});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("CraftItem extra parameter is surfaced for gateway rejection"),
		Parsed.Kind == EAIREParsedChatFrameKind::Response
			&& Parsed.Result.CommandCandidates.Num() == 1
			&& Parsed.Result.CommandCandidates[0].bHasUnsupportedParameters);

	TSharedPtr<FJsonObject> CraftWithoutQuantity = MakeShared<FJsonObject>();
	CraftWithoutQuantity->SetStringField(
		TEXT("recipe_id"),
		TEXT("recipe-11"));
	Response = MakeResponse();
	SetCandidates(
		Response,
		{MakeCandidate(
			TEXT("command-missing-craft"),
			TEXT("Command.CraftItem"),
			CraftWithoutQuantity)});
	Parsed = FAIREChatJsonAdapter::ParseHttpBody(
		SerializeObject(Response),
		Correlation,
		false);
	TestTrue(
		TEXT("CraftItem missing quantity invalidates the frame"),
		Parsed.Kind == EAIREParsedChatFrameKind::Invalid);

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

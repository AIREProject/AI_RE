#if WITH_DEV_AUTOMATION_TESTS

#include "Chat/Context/AIREWorldContextBuilder.h"
#include "Chat/Transport/AIREChatJsonAdapter.h"

#include "Algo/Reverse.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool BuildRequest(
		const FAIREWorldContextV1& WorldContext,
		FString& OutHttpBody,
		FString& OutWebSocketFrame,
		FString& OutError)
	{
		FAIREInGameChatContext ChatContext;
		ChatContext.SaveSlotId = TEXT("demo-slot-1");
		ChatContext.LocationId = TEXT("forest_camp");
		ChatContext.Day = 1;
		ChatContext.Hour = 12.0f;
		ChatContext.Period = EAIREGameWorldPeriod::Afternoon;
		return FAIREChatJsonAdapter::BuildInGameRequest(
			ChatContext,
			WorldContext,
			TEXT("mako"),
			TEXT("session-1"),
			TEXT("request-1"),
			TEXT("message-1"),
			TEXT("hello"),
			OutHttpBody,
			OutWebSocketFrame,
			OutError);
	}

	TSharedPtr<FJsonObject> ParseObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}

	FAIREWorldContextV1 MakeFullContext()
	{
		FAIREWorldContextV1 Context;
		Context.LocationId = TEXT("forest_camp");
		Context.Threat.bPresent = true;
		Context.Threat.Count = 2;

		Context.CurrentWork.Type = EAIREWorldContextWorkType::Harvesting;
		Context.CurrentWork.State = EAIREWorldContextWorkState::Working;

		FAIREWorldContextInventory Storage;
		Storage.ContainerId = TEXT("AIRE.Inventory.SharedStorage");
		Storage.FreeSlots = 48;
		Storage.ItemTotals.Add({TEXT("Stone"), 5});
		Storage.ItemTotals.Add({TEXT("Branch"), 4});
		Context.Inventories.Add(MoveTemp(Storage));

		FAIREWorldContextInventory Mako;
		Mako.ContainerId = TEXT("AIRE.Inventory.MAKO");
		Mako.FreeSlots = 12;
		Mako.ItemTotals.Add({TEXT("Stone"), 2});
		Mako.ItemTotals.Add({TEXT("Branch"), 4});
		Mako.bTruncated = true;
		Context.Inventories.Add(MoveTemp(Mako));
		return Context;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREWorldContextV1AutomationTest,
	"AIRE.Companion.Chat.Context.V1",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREWorldContextV1AutomationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FAIREWorldContextV1 FullContext = MakeFullContext();
	FString HttpBody;
	FString WebSocketFrame;
	FString Error;
	TestTrue(
		TEXT("Full Context v1 serializes"),
		BuildRequest(FullContext, HttpBody, WebSocketFrame, Error));

	const TSharedPtr<FJsonObject> Http = ParseObject(HttpBody);
	TestTrue(TEXT("HTTP request is valid JSON"), Http.IsValid());
	if (!Http.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* GameContext = nullptr;
	TestTrue(
		TEXT("HTTP request contains a versioned game_context object"),
		Http->TryGetObjectField(TEXT("game_context"), GameContext)
			&& GameContext != nullptr
			&& GameContext->IsValid());
	if (GameContext == nullptr || !GameContext->IsValid())
	{
		return false;
	}
	TestEqual(TEXT("Context has exactly seven fields"), (*GameContext)->Values.Num(), 7);
	TestEqual(
		TEXT("Context schema is version 1"),
		static_cast<int32>((*GameContext)->GetNumberField(TEXT("schema_version"))),
		1);
	TestEqual(
		TEXT("Context uses the forest camp stable location"),
		(*GameContext)->GetStringField(TEXT("location_id")),
		FString(TEXT("forest_camp")));
	const TSharedPtr<FJsonObject>* Threat = nullptr;
	TestTrue(
		TEXT("Threat is a structured object"),
		(*GameContext)->TryGetObjectField(TEXT("threat"), Threat)
			&& Threat != nullptr
			&& Threat->IsValid());
	if (Threat != nullptr && Threat->IsValid())
	{
		TestTrue(
			TEXT("Threat presence is serialized"),
			(*Threat)->GetBoolField(TEXT("present")));
		TestEqual(
			TEXT("Threat count is serialized"),
			static_cast<int32>((*Threat)->GetNumberField(TEXT("count"))),
			2);
		TestTrue(
			TEXT("Unknown threat kind is null"),
			(*Threat)->HasTypedField<EJson::Null>(TEXT("nearest_kind")));
	}
	const TArray<TSharedPtr<FJsonValue>>* Resources = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Workstations = nullptr;
	TestTrue(
		TEXT("Unavailable resources use an empty array"),
		(*GameContext)->TryGetArrayField(TEXT("nearby_resources"), Resources)
			&& Resources != nullptr
			&& Resources->IsEmpty());
	TestTrue(
		TEXT("Unavailable workstations use an empty array"),
		(*GameContext)->TryGetArrayField(TEXT("available_workstations"), Workstations)
			&& Workstations != nullptr
			&& Workstations->IsEmpty());
	const TSharedPtr<FJsonObject>* CurrentWork = nullptr;
	TestTrue(
		TEXT("Active WorkOrder is serialized"),
		(*GameContext)->TryGetObjectField(TEXT("current_work"), CurrentWork)
			&& CurrentWork != nullptr
			&& CurrentWork->IsValid());
	if (CurrentWork != nullptr && CurrentWork->IsValid())
	{
		TestEqual(
			TEXT("Work type is stable"),
			(*CurrentWork)->GetStringField(TEXT("type")),
			FString(TEXT("Harvesting")));
		TestEqual(
			TEXT("Work state is stable"),
			(*CurrentWork)->GetStringField(TEXT("state")),
			FString(TEXT("Working")));
	}
	const TArray<TSharedPtr<FJsonValue>>* Inventories = nullptr;
	TestTrue(
		TEXT("Both inventory summaries are serialized"),
		(*GameContext)->TryGetArrayField(TEXT("inventories"), Inventories)
			&& Inventories != nullptr
			&& Inventories->Num() == 2);
	if (Inventories != nullptr && Inventories->Num() == 2)
	{
		const TSharedPtr<FJsonObject> FirstInventory = (*Inventories)[0]->AsObject();
		TestEqual(
			TEXT("Inventory output is ordered by stable container id"),
			FirstInventory->GetStringField(TEXT("container_id")),
			FString(TEXT("AIRE.Inventory.MAKO")));
		TestEqual(
			TEXT("MAKO free slots are serialized"),
			static_cast<int32>(FirstInventory->GetNumberField(TEXT("free_slots"))),
			12);
		TestTrue(
			TEXT("Unreported MAKO items set truncated"),
			FirstInventory->GetBoolField(TEXT("truncated")));
		const TArray<TSharedPtr<FJsonValue>>* ItemTotals = nullptr;
		TestTrue(
			TEXT("MAKO selected item totals are serialized"),
			FirstInventory->TryGetArrayField(TEXT("item_totals"), ItemTotals)
				&& ItemTotals != nullptr
				&& ItemTotals->Num() == 2);
		if (ItemTotals != nullptr && ItemTotals->Num() == 2)
		{
			const TSharedPtr<FJsonObject> Branch = (*ItemTotals)[0]->AsObject();
			TestEqual(
				TEXT("Item totals are ordered by stable item id"),
				Branch->GetStringField(TEXT("item_id")),
				FString(TEXT("Branch")));
			TestEqual(
				TEXT("Selected item count is serialized"),
				static_cast<int32>(Branch->GetNumberField(TEXT("count"))),
				4);
		}
	}

	const TSharedPtr<FJsonObject> WebSocket = ParseObject(WebSocketFrame);
	const TSharedPtr<FJsonObject>* Payload = nullptr;
	const TSharedPtr<FJsonObject>* WebSocketContext = nullptr;
	TestTrue(
		TEXT("WebSocket uses the same structured Context"),
		WebSocket.IsValid()
			&& WebSocket->TryGetObjectField(TEXT("payload"), Payload)
			&& Payload != nullptr
			&& Payload->IsValid()
			&& (*Payload)->TryGetObjectField(TEXT("game_context"), WebSocketContext)
			&& WebSocketContext != nullptr
			&& WebSocketContext->IsValid()
			&& (*WebSocketContext)->Values.Num() == (*GameContext)->Values.Num());

	FString SecondHttpBody;
	FString SecondFrame;
	TestTrue(
		TEXT("The same Context serializes twice"),
		BuildRequest(FullContext, SecondHttpBody, SecondFrame, Error));
	TestEqual(TEXT("Context request serialization is deterministic"), SecondHttpBody, HttpBody);
	FAIREWorldContextV1 ReorderedContext = FullContext;
	Algo::Reverse(ReorderedContext.Inventories);
	for (FAIREWorldContextInventory& Inventory : ReorderedContext.Inventories)
	{
		Algo::Reverse(Inventory.ItemTotals);
	}
	TestTrue(
		TEXT("Reordered Context serializes"),
		BuildRequest(ReorderedContext, SecondHttpBody, SecondFrame, Error));
	TestEqual(
		TEXT("Collection input order does not change the request"),
		SecondHttpBody,
		HttpBody);

	const FAIREWorldContextV1 EmptyContext =
		FAIREWorldContextBuilder::Build(nullptr, TEXT("forest_camp"));
	TestTrue(
		TEXT("Unavailable owner produces a safe complete Context"),
		BuildRequest(EmptyContext, HttpBody, WebSocketFrame, Error));
	const TSharedPtr<FJsonObject> EmptyHttp = ParseObject(HttpBody);
	const TSharedPtr<FJsonObject>* EmptyGameContext = nullptr;
	TestTrue(
		TEXT("Safe empty Context is not an empty object"),
		EmptyHttp.IsValid()
			&& EmptyHttp->TryGetObjectField(TEXT("game_context"), EmptyGameContext)
			&& EmptyGameContext != nullptr
			&& EmptyGameContext->IsValid()
			&& (*EmptyGameContext)->Values.Num() == 7);
	if (EmptyGameContext != nullptr && EmptyGameContext->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* EmptyInventories = nullptr;
		TestTrue(
			TEXT("Unavailable inventories use an empty array"),
			(*EmptyGameContext)->TryGetArrayField(
				TEXT("inventories"),
				EmptyInventories)
				&& EmptyInventories != nullptr
				&& EmptyInventories->IsEmpty());
		TestTrue(
			TEXT("Unavailable WorkOrder uses null"),
			(*EmptyGameContext)->HasTypedField<EJson::Null>(
				TEXT("current_work")));
	}

	FAIREWorldContextV1 InvalidContext = FullContext;
	InvalidContext.LocationId = TEXT("/Game/Maps/ForestCamp");
	TestFalse(
		TEXT("A map path is rejected as a stable location id"),
		BuildRequest(InvalidContext, HttpBody, WebSocketFrame, Error));
	InvalidContext = FullContext;
	InvalidContext.LocationId = TEXT("한글");
	TestFalse(
		TEXT("Stable ids are restricted to the contract ASCII alphabet"),
		BuildRequest(InvalidContext, HttpBody, WebSocketFrame, Error));
	InvalidContext = FullContext;
	InvalidContext.Threat.Count = 33;
	TestFalse(
		TEXT("Threat count above the contract bound is rejected"),
		BuildRequest(InvalidContext, HttpBody, WebSocketFrame, Error));

	FAIREWorldContextV1 BoundaryContext = FullContext;
	BoundaryContext.Threat.Count = AIREWorldContext::MaxThreatCount;
	for (int32 Index = 0;
		Index < AIREWorldContext::MaxNearbyResourceTypes;
		++Index)
	{
		BoundaryContext.NearbyResources.Add(
			{FString::Printf(TEXT("Resource.%02d"), Index), 32});
		BoundaryContext.AvailableWorkstations.Add(
			FString::Printf(TEXT("Workbench.%02d"), Index));
	}
	for (FAIREWorldContextInventory& Inventory : BoundaryContext.Inventories)
	{
		Inventory.ItemTotals.Reset();
		for (int32 Index = 0;
			Index < AIREWorldContext::MaxInventoryItemTypes;
			++Index)
		{
			Inventory.ItemTotals.Add(
				{FString::Printf(TEXT("Item.%02d"), Index), 1});
		}
	}
	TestTrue(
		TEXT("Exact Context collection and threat bounds are accepted"),
		BuildRequest(BoundaryContext, HttpBody, WebSocketFrame, Error));
	BoundaryContext.NearbyResources.Add({TEXT("Resource.Overflow"), 1});
	TestFalse(
		TEXT("A ninth resource kind is rejected"),
		BuildRequest(BoundaryContext, HttpBody, WebSocketFrame, Error));
	BoundaryContext.NearbyResources.SetNum(
		AIREWorldContext::MaxNearbyResourceTypes);
	BoundaryContext.Inventories[0].ItemTotals.Add(
		{TEXT("Item.Overflow"), 1});
	TestFalse(
		TEXT("A seventeenth inventory item kind is rejected"),
		BuildRequest(BoundaryContext, HttpBody, WebSocketFrame, Error));

	FAIREWorldContextV1 InvalidSizeInput = FullContext;
	InvalidSizeInput.LocationId = FString::ChrN(
		AIREWorldContext::MaxStableIdLength + 1,
		TEXT('x'));
	TestFalse(
		TEXT("Stable id length above the contract bound is rejected"),
		BuildRequest(
			InvalidSizeInput,
			HttpBody,
			WebSocketFrame,
			Error));

	return true;
}

#endif

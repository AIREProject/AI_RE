#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Offline/AIREOfflineTaskJsonAdapter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREOfflineTaskJsonAdapterTest,
	"AIRE.OfflineTask.JsonAdapter",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREOfflineTaskJsonAdapterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString RequestId(TEXT("task-list-test-1"));
	const FString ValidList = TEXT(R"JSON(
{
  "request_id": "task-list-test-1",
  "tasks": [
    {
      "task_id": "task-1",
      "save_slot_id": "demo-slot-1",
      "item_id": "ShoddyBandage",
      "task_type": "Crafting",
      "status": "Completed",
      "started_at": "2026-08-12T12:00:00Z",
      "quantity": 3,
      "result_quantity": 2,
      "progress_quantity": null
    }
  ]
}
)JSON");
	const FAIREParsedOfflineTaskList Parsed =
		FAIREOfflineTaskJsonAdapter::ParseListResponse(ValidList, RequestId);
	TestTrue(TEXT("Current task list response parses"), Parsed.bIsValid);
	TestEqual(TEXT("One task"), Parsed.Tasks.Num(), 1);
	if (Parsed.Tasks.Num() == 1)
	{
		FAIREOfflineTaskApplyRequest ApplyRequest;
		TestTrue(
			TEXT("Supported craft maps to inventory operation"),
			FAIREOfflineTaskJsonAdapter::BuildInventoryApplyRequest(
				Parsed.Tasks[0],
				FGuid::NewGuid(),
				1,
				2,
				ApplyRequest));
		TestEqual(TEXT("Craft consumes two stems per result"),
			ApplyRequest.Costs[0].Count, 4);
		TestEqual(TEXT("Craft grants completed result quantity"),
			ApplyRequest.Rewards[0].Count, 2);
	}
	TestFalse(
		TEXT("Wrong request correlation is rejected"),
		FAIREOfflineTaskJsonAdapter::ParseListResponse(
			ValidList,
			TEXT("other-request")).bIsValid);
	const FString MissingNullableField = ValidList.Replace(
		TEXT("      \"progress_quantity\": null\n"),
		TEXT(""));
	TestFalse(
		TEXT("Missing nullable DTO field is rejected"),
		FAIREOfflineTaskJsonAdapter::ParseListResponse(
			MissingNullableField,
			RequestId).bIsValid);

	const FString ClaimedResponse = TEXT(R"JSON(
{
  "request_id": "task-claim-test-1",
  "task": {
    "task_id": "task-1",
    "save_slot_id": "demo-slot-1",
    "item_id": "ShoddyBandage",
    "task_type": "Crafting",
    "status": "Claimed",
    "started_at": "2026-08-12T12:00:00Z",
    "quantity": 3,
    "result_quantity": 2,
    "progress_quantity": null
  }
}
)JSON");
	TestTrue(
		TEXT("Current claim response parses"),
		FAIREOfflineTaskJsonAdapter::ParseTaskResponse(
			ClaimedResponse,
			TEXT("task-claim-test-1"),
			TEXT("task-1"),
			EAIREOfflineTaskStatus::Claimed).bIsValid);

	const FString NotReadyResponse = TEXT(R"JSON(
{
  "request_id": "task-complete-test-1",
  "task": {
    "task_id": "task-2",
    "save_slot_id": "demo-slot-1",
    "item_id": "PlantStem",
    "task_type": "Gathering",
    "status": "InProgress",
    "started_at": "2026-08-12T12:00:00Z",
    "quantity": 1,
    "result_quantity": null,
    "progress_quantity": 0
  }
}
)JSON");
	TestTrue(
		TEXT("Complete response may remain in progress before first unit"),
		FAIREOfflineTaskJsonAdapter::ParseTaskResponse(
			NotReadyResponse,
			TEXT("task-complete-test-1"),
			TEXT("task-2"),
			EAIREOfflineTaskStatus::Completed,
			true).bIsValid);
	TestFalse(
		TEXT("Not-ready transition requires explicit allowance"),
		FAIREOfflineTaskJsonAdapter::ParseTaskResponse(
			NotReadyResponse,
			TEXT("task-complete-test-1"),
			TEXT("task-2"),
			EAIREOfflineTaskStatus::Completed).bIsValid);
	return true;
}

#endif

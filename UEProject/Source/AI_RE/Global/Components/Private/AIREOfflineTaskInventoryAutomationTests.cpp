#if WITH_DEV_AUTOMATION_TESTS

#include "AIREGameplayInventorySubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREOfflineTaskInventoryTest,
	"AIRE.Inventory.OfflineTask.AtomicApply",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREOfflineTaskInventoryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
		NewObject<UAIREGameplayInventorySubsystem>(GameInstance.Get()));
	FAIREInventorySessionScope Scope;
	Scope.ProfileId = TEXT("profile.test");
	Scope.SaveSlotId = TEXT("save.test");
	Scope.CompanionId = TEXT("companion.test");
	const FGuid SessionId = Inventory->ResetInventorySession(Scope);

	FAIREInventoryContainerSnapshot Mako;
	FAIREInventoryContainerSnapshot Storage;
	Inventory->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetMakoContainerId(),
		Mako);
	Inventory->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
		Storage);
	FAIREInventoryMutationRequest Seed;
	Seed.SessionId = SessionId;
	Seed.MutationId = FGuid::NewGuid();
	Seed.ContainerId = Mako.ContainerId;
	Seed.ExpectedRevision = Mako.Revision;
	Seed.ItemId = FName(TEXT("AIRE.Test.Stack4"));
	Seed.Count = 4;
	TestEqual(
		TEXT("Craft material is seeded"),
		Inventory->TryAddItem(Seed).Code,
		EAIREInventoryMutationCode::Succeeded);

	Inventory->GetContainerSnapshot(Mako.ContainerId, Mako);
	Inventory->GetContainerSnapshot(Storage.ContainerId, Storage);
	FAIREOfflineTaskApplyRequest TaskResult;
	TaskResult.TaskId = TEXT("task-test-1");
	TaskResult.SessionId = SessionId;
	TaskResult.ExpectedMakoRevision = Mako.Revision;
	TaskResult.ExpectedStorageRevision = Storage.Revision;
	FAIREInventoryItemQuantity& Cost = TaskResult.Costs.AddDefaulted_GetRef();
	Cost.ItemId = FName(TEXT("AIRE.Test.Stack4"));
	Cost.Count = 4;
	FAIREInventoryItemQuantity& Reward =
		TaskResult.Rewards.AddDefaulted_GetRef();
	Reward.ItemId = FName(TEXT("AIRE.Test.Stack2"));
	Reward.Count = 2;
	const FAIREOfflineTaskApplyResult Applied =
		Inventory->TryApplyOfflineTaskResult(TaskResult);
	TestEqual(
		TEXT("Task result applies atomically"),
		Applied.Code,
		EAIREInventoryMutationCode::Succeeded);
	const FAIREOfflineTaskApplyResult Duplicate =
		Inventory->TryApplyOfflineTaskResult(TaskResult);
	TestEqual(
		TEXT("Stable task ID prevents duplicate mutation"),
		Duplicate.Code,
		EAIREInventoryMutationCode::AlreadyApplied);

	FAIREInventoryContainerSnapshot After;
	Inventory->GetContainerSnapshot(Mako.ContainerId, After);
	int32 MaterialCount = 0;
	int32 RewardCount = 0;
	for (const FAIREInventoryItemStackSnapshot& Stack : After.ItemStacks)
	{
		if (Stack.ItemId == Cost.ItemId)
		{
			MaterialCount += Stack.Count;
		}
		if (Stack.ItemId == Reward.ItemId)
		{
			RewardCount += Stack.Count;
		}
	}
	TestEqual(TEXT("Material is consumed once"), MaterialCount, 0);
	TestEqual(TEXT("Reward is granted once"), RewardCount, 2);

	FAIREOfflineTaskApplyRequest Insufficient = TaskResult;
	Insufficient.TaskId = TEXT("task-test-2");
	Insufficient.ExpectedMakoRevision = After.Revision;
	Insufficient.Costs[0].Count = 1;
	const FAIREOfflineTaskApplyResult Rejected =
		Inventory->TryApplyOfflineTaskResult(Insufficient);
	TestEqual(
		TEXT("Insufficient material rejects the whole task result"),
		Rejected.Code,
		EAIREInventoryMutationCode::InsufficientQuantity);
	FAIREInventoryContainerSnapshot AfterRejected;
	Inventory->GetContainerSnapshot(Mako.ContainerId, AfterRejected);
	TestEqual(
		TEXT("Rejected task result preserves revision"),
		AfterRejected.Revision,
		After.Revision);
	return true;
}

#endif

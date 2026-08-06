#include "AIREGameplayInventorySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI_REPlayerInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr TCHAR Stack2ItemName[] = TEXT("AIRE.Test.Stack2");
	constexpr TCHAR Stack4ItemName[] = TEXT("AIRE.Test.Stack4");
	constexpr TCHAR UniqueResultItemName[] = TEXT("AIRE.Test.Unique.CraftResult");

	int32 CountSnapshotItem(
		const TArray<FAIREInventoryItemStackSnapshot>& Stacks,
		const FName ItemId)
	{
		int32 TotalCount = 0;
		for (const FAIREInventoryItemStackSnapshot& Stack : Stacks)
		{
			if (Stack.ItemId == ItemId)
			{
				TotalCount += Stack.Count;
			}
		}
		return TotalCount;
	}

	bool AddToContainer(
		UAIREGameplayInventorySubsystem& Inventory,
		const FGuid& SessionId,
		const FName ContainerId,
		const FName ItemId,
		const int32 Count)
	{
		FAIREInventoryContainerSnapshot Snapshot;
		if (!Inventory.GetContainerSnapshot(ContainerId, Snapshot))
		{
			return false;
		}

		FAIREInventoryMutationRequest Request;
		Request.SessionId = SessionId;
		Request.MutationId = FGuid::NewGuid();
		Request.ContainerId = ContainerId;
		Request.ExpectedRevision = Snapshot.Revision;
		Request.ItemId = ItemId;
		Request.Count = Count;
		return Inventory.TryAddItem(Request).Code
			== EAIREInventoryMutationCode::Succeeded;
	}

	FAIREPlayerCraftRequest MakePlayerCraftRequest(
		const FGuid& SessionId,
		const int64 StorageRevision,
		const int32 IngredientCount,
		const FName ResultItemId = FName(UniqueResultItemName))
	{
		FAIREPlayerCraftRequest Request;
		Request.SessionId = SessionId;
		Request.MutationId = FGuid::NewGuid();
		Request.ExpectedStorageRevision = StorageRevision;
		FAIREInventoryItemQuantity& Ingredient =
			Request.Ingredients.AddDefaulted_GetRef();
		Ingredient.ItemId = FName(Stack4ItemName);
		Ingredient.Count = IngredientCount;
		Request.Result.ItemId = ResultItemId;
		Request.Result.Count = 1;
		return Request;
	}

	FAIREMakoCraftWorkRequest MakeMakoCraftRequest(
		const FGuid& SessionId,
		const int64 MakoRevision,
		const int64 StorageRevision,
		const int32 IngredientCount)
	{
		FAIREMakoCraftWorkRequest Request;
		Request.SessionId = SessionId;
		Request.WorkOrderId = FGuid::NewGuid();
		Request.ExpectedMakoRevision = MakoRevision;
		Request.ExpectedStorageRevision = StorageRevision;
		FAIREInventoryItemQuantity& Ingredient =
			Request.Ingredients.AddDefaulted_GetRef();
		Ingredient.ItemId = FName(Stack4ItemName);
		Ingredient.Count = IngredientCount;
		Request.Result.ItemId = FName(Stack2ItemName);
		Request.Result.Count = 1;
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREGameplayInventoryCraftSettlementTest,
	"AIRE.Inventory.Subsystem.CraftSettlement",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREGameplayInventoryCraftSettlementTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const FName MakoContainerId =
		UAIREGameplayInventorySubsystem::GetMakoContainerId();
	const FName StorageContainerId =
		UAIREGameplayInventorySubsystem::GetSharedStorageContainerId();
	const FName Stack4ItemId(Stack4ItemName);
	const FName Stack2ItemId(Stack2ItemName);
	const FName UniqueResultItemId(UniqueResultItemName);

	{
		TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
		TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
			NewObject<UAIREGameplayInventorySubsystem>(GameInstance.Get()));
		TStrongObjectPtr<UAI_REPlayerInventoryComponent> PlayerInventory(
			NewObject<UAI_REPlayerInventoryComponent>());
		const FGuid SessionId = Inventory->ResetInventorySession();
		FInventoryItemStack& PlayerStack =
			PlayerInventory->Items.AddDefaulted_GetRef();
		PlayerStack.SlotIndex = 0;
		PlayerStack.ItemId = Stack4ItemId;
		PlayerStack.Count = 3;

		FAIREInventoryContainerSnapshot StorageBefore;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageBefore);
		FAIREPlayerCraftRequest Request = MakePlayerCraftRequest(
			SessionId,
			StorageBefore.Revision,
			1);
		FAIREInventoryItemQuantity& DuplicateIngredient =
			Request.Ingredients.AddDefaulted_GetRef();
		DuplicateIngredient.ItemId = Stack4ItemId;
		DuplicateIngredient.Count = 2;
		FAIREInventoryMutationResult PreflightResult;
		TestTrue(
			TEXT("Player craft aggregates duplicate ingredients during preflight"),
			Inventory->CanCompletePlayerCraft(
				PlayerInventory.Get(), Request, PreflightResult));
		TestTrue(
			TEXT("Player duplicate-ingredient craft succeeds"),
			Inventory->TryCompletePlayerCraft(PlayerInventory.Get(), Request).Code
				== EAIREInventoryMutationCode::Succeeded);
		TestEqual(
			TEXT("Player duplicate ingredients consume their combined quantity"),
			PlayerInventory->GetItemCount(Stack4ItemId),
			0);
		TestEqual(
			TEXT("Player duplicate-ingredient craft adds its result locally"),
			PlayerInventory->GetItemCount(UniqueResultItemId),
			1);
		FAIREInventoryContainerSnapshot StorageAfter;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageAfter);
		TestEqual(
			TEXT("Player-local craft leaves storage revision unchanged"),
			StorageAfter.Revision,
			StorageBefore.Revision);

		const FAIREInventoryMutationResult ReplayResult =
			Inventory->TryCompletePlayerCraft(PlayerInventory.Get(), Request);
		TestTrue(
			TEXT("Player craft replay is already applied"),
			ReplayResult.Code == EAIREInventoryMutationCode::AlreadyApplied);
		TestEqual(
			TEXT("Player craft replay does not duplicate the result"),
			PlayerInventory->GetItemCount(UniqueResultItemId),
			1);
		FAIREInventoryContainerSnapshot StorageAfterReplay;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageAfterReplay);
		TestEqual(
			TEXT("Player craft replay preserves storage revision"),
			StorageAfterReplay.Revision,
			StorageAfter.Revision);
	}

	{
		TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
		TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
			NewObject<UAIREGameplayInventorySubsystem>(GameInstance.Get()));
		TStrongObjectPtr<UAI_REPlayerInventoryComponent> PlayerInventory(
			NewObject<UAI_REPlayerInventoryComponent>());
		const FGuid SessionId = Inventory->ResetInventorySession();
		FInventoryItemStack& PlayerStack =
			PlayerInventory->Items.AddDefaulted_GetRef();
		PlayerStack.SlotIndex = 0;
		PlayerStack.ItemId = Stack4ItemId;
		PlayerStack.Count = 1;
		TestTrue(
			TEXT("Storage ingredient shortage is prepared"),
			AddToContainer(*Inventory, SessionId, StorageContainerId, Stack4ItemId, 2));
		FAIREInventoryContainerSnapshot StorageBefore;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageBefore);
		FAIREPlayerCraftRequest Request = MakePlayerCraftRequest(
			SessionId,
			StorageBefore.Revision,
			3);
		TestTrue(
			TEXT("Player craft settles local inventory before storage shortage"),
			Inventory->TryCompletePlayerCraft(PlayerInventory.Get(), Request).Code
				== EAIREInventoryMutationCode::Succeeded);
		TestEqual(
			TEXT("Player-local source is consumed first"),
			PlayerInventory->GetItemCount(Stack4ItemId),
			0);
		TestEqual(
			TEXT("Player craft result is delivered to Player inventory"),
			PlayerInventory->GetItemCount(UniqueResultItemId),
			1);
		FAIREInventoryContainerSnapshot StorageAfter;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageAfter);
		TestEqual(
			TEXT("Storage supplies only the Player shortage"),
			CountSnapshotItem(StorageAfter.ItemStacks, Stack4ItemId),
			0);
		TestEqual(
			TEXT("Storage changes once when it supplies a Player shortage"),
			StorageAfter.Revision,
			StorageBefore.Revision + 1);
	}

	{
		TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
		TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
			NewObject<UAIREGameplayInventorySubsystem>(GameInstance.Get()));
		TStrongObjectPtr<UAI_REPlayerInventoryComponent> PlayerInventory(
			NewObject<UAI_REPlayerInventoryComponent>());
		const FGuid SessionId = Inventory->ResetInventorySession();
		FInventoryItemStack& PlayerStack =
			PlayerInventory->Items.AddDefaulted_GetRef();
		PlayerStack.SlotIndex = 0;
		PlayerStack.ItemId = Stack4ItemId;
		PlayerStack.Count = 1;
		TestTrue(
			TEXT("Combined insufficiency storage state is prepared"),
			AddToContainer(*Inventory, SessionId, StorageContainerId, Stack4ItemId, 1));
		FAIREInventoryContainerSnapshot StorageBefore;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageBefore);
		const TArray<FInventoryItemStack> PlayerBefore = PlayerInventory->Items;
		FAIREPlayerCraftRequest Request = MakePlayerCraftRequest(
			SessionId,
			StorageBefore.Revision,
			3);
		TestTrue(
			TEXT("Combined Player and storage insufficiency is rejected"),
			Inventory->TryCompletePlayerCraft(PlayerInventory.Get(), Request).Code
				== EAIREInventoryMutationCode::InsufficientQuantity);
		TestEqual(
			TEXT("Rejected Player craft preserves Player stack count"),
			PlayerInventory->Items.Num(),
			PlayerBefore.Num());
		TestEqual(
			TEXT("Rejected Player craft preserves Player item quantity"),
			PlayerInventory->GetItemCount(Stack4ItemId),
			1);
		FAIREInventoryContainerSnapshot StorageAfter;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageAfter);
		TestEqual(
			TEXT("Rejected Player craft preserves storage ingredient count"),
			CountSnapshotItem(StorageAfter.ItemStacks, Stack4ItemId),
			1);
		TestEqual(
			TEXT("Rejected Player craft preserves storage revision"),
			StorageAfter.Revision,
			StorageBefore.Revision);
	}

	{
		TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
		TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
			NewObject<UAIREGameplayInventorySubsystem>(GameInstance.Get()));
		TStrongObjectPtr<UAI_REPlayerInventoryComponent> PlayerInventory(
			NewObject<UAI_REPlayerInventoryComponent>());
		const FGuid SessionId = Inventory->ResetInventorySession();
		PlayerInventory->MaxSlots = 1;
		FInventoryItemStack& PlayerStack =
			PlayerInventory->Items.AddDefaulted_GetRef();
		PlayerStack.SlotIndex = 0;
		PlayerStack.ItemId = Stack4ItemId;
		PlayerStack.Count = 2;
		FAIREInventoryContainerSnapshot StorageBefore;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageBefore);
		FAIREPlayerCraftRequest Request = MakePlayerCraftRequest(
			SessionId,
			StorageBefore.Revision,
			1,
			Stack2ItemId);
		TestTrue(
			TEXT("Player result capacity failure is rejected"),
			Inventory->TryCompletePlayerCraft(PlayerInventory.Get(), Request).Code
				== EAIREInventoryMutationCode::CapacityExceeded);
		TestEqual(
			TEXT("Result capacity failure preserves Player ingredients"),
			PlayerInventory->GetItemCount(Stack4ItemId),
			2);
		TestEqual(
			TEXT("Result capacity failure does not add Player result"),
			PlayerInventory->GetItemCount(Stack2ItemId),
			0);
		FAIREInventoryContainerSnapshot StorageAfter;
		Inventory->GetContainerSnapshot(StorageContainerId, StorageAfter);
		TestEqual(
			TEXT("Result capacity failure preserves storage revision"),
			StorageAfter.Revision,
			StorageBefore.Revision);
	}

	auto RunMakoSettlement = [this, &MakoContainerId, &StorageContainerId,
		&Stack4ItemId, &Stack2ItemId](
		const TCHAR* ScenarioName,
		const int32 MakoIngredientCount,
		const int32 StorageIngredientCount,
		const int32 RequestedIngredientCount,
		const EAIREInventoryMutationCode ExpectedCode,
		const EAIREInventoryWorkResultDestination ExpectedDestination,
		const int32 ExpectedMakoIngredientCount,
		const int32 ExpectedStorageIngredientCount)
	{
		TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
		TStrongObjectPtr<UAIREGameplayInventorySubsystem> Inventory(
			NewObject<UAIREGameplayInventorySubsystem>(GameInstance.Get()));
		const FGuid SessionId = Inventory->ResetInventorySession();
		if (MakoIngredientCount > 0
			&& !AddToContainer(*Inventory, SessionId, MakoContainerId,
				Stack4ItemId, MakoIngredientCount))
		{
			return TestTrue(FString::Printf(TEXT("%s prepares MAKO"), ScenarioName), false);
		}
		if (StorageIngredientCount > 0
			&& !AddToContainer(*Inventory, SessionId, StorageContainerId,
				Stack4ItemId, StorageIngredientCount))
		{
			return TestTrue(FString::Printf(TEXT("%s prepares storage"), ScenarioName), false);
		}

		FAIREInventoryContainerSnapshot MakoBefore;
		FAIREInventoryContainerSnapshot StorageBefore;
		Inventory->GetContainerSnapshot(MakoContainerId, MakoBefore);
		Inventory->GetContainerSnapshot(StorageContainerId, StorageBefore);
		const FAIREMakoCraftWorkRequest Request = MakeMakoCraftRequest(
			SessionId, MakoBefore.Revision, StorageBefore.Revision,
			RequestedIngredientCount);
		const FAIREInventoryWorkResult Result =
			Inventory->TryCompleteMakoCraftWork(Request);
		const bool bExpectedResult = Result.Code == ExpectedCode
			&& (ExpectedCode != EAIREInventoryMutationCode::Succeeded
				|| Result.Destination == ExpectedDestination);
		if (!TestTrue(FString::Printf(TEXT("%s returns the expected result"), ScenarioName), bExpectedResult))
		{
			return false;
		}

		FAIREInventoryContainerSnapshot MakoAfter;
		FAIREInventoryContainerSnapshot StorageAfter;
		Inventory->GetContainerSnapshot(MakoContainerId, MakoAfter);
		Inventory->GetContainerSnapshot(StorageContainerId, StorageAfter);
		if (ExpectedCode == EAIREInventoryMutationCode::Succeeded)
		{
			TestEqual(FString::Printf(TEXT("%s leaves expected MAKO ingredients"), ScenarioName),
				CountSnapshotItem(MakoAfter.ItemStacks, Stack4ItemId),
				ExpectedMakoIngredientCount);
			TestEqual(FString::Printf(TEXT("%s leaves expected storage ingredients"), ScenarioName),
				CountSnapshotItem(StorageAfter.ItemStacks, Stack4ItemId),
				ExpectedStorageIngredientCount);
			TestEqual(FString::Printf(TEXT("%s stores one result at the selected destination"), ScenarioName),
				ExpectedDestination == EAIREInventoryWorkResultDestination::Mako
					? CountSnapshotItem(MakoAfter.ItemStacks, Stack2ItemId)
					: CountSnapshotItem(StorageAfter.ItemStacks, Stack2ItemId), 1);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s preserves MAKO revision"), ScenarioName),
				MakoAfter.Revision, MakoBefore.Revision);
			TestEqual(FString::Printf(TEXT("%s preserves storage revision"), ScenarioName),
				StorageAfter.Revision, StorageBefore.Revision);
			TestEqual(FString::Printf(TEXT("%s preserves MAKO count"), ScenarioName),
				CountSnapshotItem(MakoAfter.ItemStacks, Stack4ItemId), MakoIngredientCount);
			TestEqual(FString::Printf(TEXT("%s preserves storage count"), ScenarioName),
				CountSnapshotItem(StorageAfter.ItemStacks, Stack4ItemId), StorageIngredientCount);
		}
		return true;
	};

	TestTrue(
		TEXT("MAKO craft can settle ingredients split with the storage"),
		RunMakoSettlement(TEXT("Split MAKO and storage craft"), 1, 2, 3,
			EAIREInventoryMutationCode::Succeeded,
			EAIREInventoryWorkResultDestination::Mako, 0, 0));
	TestTrue(
		TEXT("MAKO craft can use storage-only ingredients"),
		RunMakoSettlement(TEXT("Storage-only craft"), 0, 3, 3,
			EAIREInventoryMutationCode::Succeeded,
			EAIREInventoryWorkResultDestination::Mako, 0, 0));
	TestTrue(
		TEXT("MAKO craft keeps storage stock when MAKO has enough"),
		RunMakoSettlement(TEXT("MAKO-first craft"), 3, 2, 3,
			EAIREInventoryMutationCode::Succeeded,
			EAIREInventoryWorkResultDestination::Mako, 0, 2));
	TestTrue(
		TEXT("MAKO craft combined insufficiency is atomic"),
		RunMakoSettlement(TEXT("Insufficient MAKO and storage craft"), 1, 1, 3,
			EAIREInventoryMutationCode::InsufficientQuantity,
			EAIREInventoryWorkResultDestination::None, 1, 1));
	return true;
}

#endif

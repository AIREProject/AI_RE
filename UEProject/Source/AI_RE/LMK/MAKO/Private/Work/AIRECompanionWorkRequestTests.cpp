#if WITH_DEV_AUTOMATION_TESTS

#include "Work/AIRECompanionCraftingWorkRequest.h"
#include "Work/AIRECompanionHarvestWorkRequest.h"

#include "AI_RECraftingTypes.h"
#include "AI_REHarvestableResourceActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "AI_REItemActor.h"
#include "AI_REItemDataAsset.h"
#include "AI_REWorkBench.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "Work/AIRECompanionWorkOrderComponent.h"

#include <limits>

namespace
{
	const FName TestRecipeRowId(TEXT("AIRE.Test.ValidRecipe"));

	FAI_RECraftingRecipe MakeValidRecipe()
	{
		FAI_RECraftingRecipe Recipe;
		Recipe.ResultItemId = FName(TEXT("AIRE.Test.Result"));
		Recipe.ResultAmount = 1;
		Recipe.RequiredWorkbench = EWorkbenchType::Basic;
		Recipe.CraftingTime = 1.0f;
		FAI_RECraftingIngredient& Ingredient =
			Recipe.Ingredients.AddDefaulted_GetRef();
		Ingredient.ItemId = FName(TEXT("AIRE.Test.Ingredient"));
		Ingredient.Amount = 1;
		return Recipe;
	}

	void ReplaceRecipe(UDataTable& RecipeTable, const FAI_RECraftingRecipe& Recipe)
	{
		RecipeTable.RemoveRow(TestRecipeRowId);
		RecipeTable.AddRow(TestRecipeRowId, Recipe);
	}

	bool IsEmptyWorkOrder(const FAIRECompanionWorkOrderSnapshot& Snapshot)
	{
		return !Snapshot.WorkOrderId.IsValid()
			&& Snapshot.TargetActor.Get() == nullptr
			&& Snapshot.WorkType == EAIRECompanionWorkOrderType::None
			&& Snapshot.RecipeTable.Get() == nullptr
			&& Snapshot.RecipeRowId.IsNone()
			&& Snapshot.State == EAIRECompanionWorkOrderState::None;
	}

	bool AreSameWorkOrders(
		const FAIRECompanionWorkOrderSnapshot& Left,
		const FAIRECompanionWorkOrderSnapshot& Right)
	{
		return Left.WorkOrderId == Right.WorkOrderId
			&& Left.TargetActor.Get() == Right.TargetActor.Get()
			&& Left.WorkType == Right.WorkType
			&& Left.RecipeTable.Get() == Right.RecipeTable.Get()
			&& Left.RecipeRowId == Right.RecipeRowId
			&& Left.State == Right.State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECompanionWorkRequestValidationTest,
	"AIRE.Companion.Work.RequestValidation",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECompanionWorkRequestValidationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	if (!TestNotNull(TEXT("Engine is available for work request test"), GEngine))
	{
		return false;
	}
	const FName WorldName = MakeUniqueObjectName(
		nullptr,
		UWorld::StaticClass(),
		NAME_None,
		EUniqueObjectNameOptions::GloballyUnique);
	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		WorldName,
		GetTransientPackage());
	if (!TestNotNull(TEXT("Transient work request world is created"), TestWorld))
	{
		return false;
	}
	FWorldContext& WorldContext =
		GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		GEngine->ShutdownWorldNetDriver(TestWorld);
		TestWorld->DestroyWorld(true);
		TestWorld->SetPhysicsScene(nullptr);
		GEngine->DestroyWorldContext(TestWorld);
	};
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAI_REWorkBench* Workbench =
		TestWorld->SpawnActor<AAI_REWorkBench>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!TestNotNull(TEXT("Basic test workbench is spawned"), Workbench))
	{
		return false;
	}
	TStrongObjectPtr<UAIRECompanionWorkOrderComponent> CraftWorkOrder(
		NewObject<UAIRECompanionWorkOrderComponent>(Workbench));
	TStrongObjectPtr<UDataTable> RecipeTable(NewObject<UDataTable>());
	RecipeTable->RowStruct = FAI_RECraftingRecipe::StaticStruct();
	const FAI_RECraftingRecipe ValidRecipe = MakeValidRecipe();
	ReplaceRecipe(*RecipeTable, ValidRecipe);

	auto TestCraftRejection = [this, &CraftWorkOrder](
		const TCHAR* What,
		const bool bRequestAccepted,
		const FGuid& ReturnedWorkOrderId)
	{
		TestFalse(What, bRequestAccepted);
		TestFalse(
			TEXT("Rejected crafting request returns no WorkOrder ID"),
			ReturnedWorkOrderId.IsValid());
		return TestTrue(
			TEXT("Rejected crafting request preserves the empty snapshot"),
			IsEmptyWorkOrder(CraftWorkOrder->GetWorkOrderSnapshot()));
	};
	auto TryCraft = [&CraftWorkOrder](
		AAI_REWorkBenchBase* Target,
		UDataTable* Table,
		const FName RowId,
		FGuid& OutWorkOrderId)
	{
		return FAIRECompanionCraftingWorkRequest::TryRequest(
			CraftWorkOrder.Get(),
			Target,
			Table,
			RowId,
			OutWorkOrderId);
	};

	FGuid ReturnedWorkOrderId;
	TestFalse(
		TEXT("Null WorkOrder component rejects crafting request"),
		FAIRECompanionCraftingWorkRequest::TryRequest(
			nullptr,
			Workbench,
			RecipeTable.Get(),
			TestRecipeRowId,
			ReturnedWorkOrderId));
	TestFalse(
		TEXT("Null crafting component request returns no ID"),
		ReturnedWorkOrderId.IsValid());
	TestCraftRejection(
		TEXT("Null crafting target is rejected"),
		TryCraft(nullptr, RecipeTable.Get(), TestRecipeRowId, ReturnedWorkOrderId),
		ReturnedWorkOrderId);
	TestCraftRejection(
		TEXT("Null recipe table is rejected"),
		TryCraft(Workbench, nullptr, TestRecipeRowId, ReturnedWorkOrderId),
		ReturnedWorkOrderId);
	TestCraftRejection(
		TEXT("None recipe row is rejected"),
		TryCraft(Workbench, RecipeTable.Get(), NAME_None, ReturnedWorkOrderId),
		ReturnedWorkOrderId);
	TestCraftRejection(
		TEXT("Missing recipe row is rejected"),
		TryCraft(
			Workbench,
			RecipeTable.Get(),
			FName(TEXT("AIRE.Test.MissingRecipe")),
			ReturnedWorkOrderId),
		ReturnedWorkOrderId);

	TStrongObjectPtr<UDataTable> WrongRowTable(NewObject<UDataTable>());
	WrongRowTable->RowStruct = FTableRowBase::StaticStruct();
	TestCraftRejection(
		TEXT("Wrong DataTable row type is rejected"),
		TryCraft(
			Workbench,
			WrongRowTable.Get(),
			TestRecipeRowId,
			ReturnedWorkOrderId),
		ReturnedWorkOrderId);

	Workbench->WorkbenchType = EWorkbenchType::None;
	TestCraftRejection(
		TEXT("None workbench type is rejected"),
		TryCraft(
			Workbench,
			RecipeTable.Get(),
			TestRecipeRowId,
			ReturnedWorkOrderId),
		ReturnedWorkOrderId);
	Workbench->WorkbenchType = EWorkbenchType::Basic;

	auto TestInvalidRecipe = [
		&RecipeTable,
		Workbench,
		&TryCraft,
		&TestCraftRejection](
		const TCHAR* What,
		const FAI_RECraftingRecipe& InvalidRecipe)
	{
		ReplaceRecipe(*RecipeTable, InvalidRecipe);
		FGuid WorkOrderId;
		return TestCraftRejection(
			What,
			TryCraft(
				Workbench,
				RecipeTable.Get(),
				TestRecipeRowId,
				WorkOrderId),
			WorkOrderId);
	};
	FAI_RECraftingRecipe InvalidRecipe = ValidRecipe;
	InvalidRecipe.RequiredWorkbench = EWorkbenchType::Blacksmith;
	TestInvalidRecipe(TEXT("Workbench type mismatch is rejected"), InvalidRecipe);
	InvalidRecipe = ValidRecipe;
	InvalidRecipe.RequiredWorkbench = EWorkbenchType::None;
	TestInvalidRecipe(TEXT("None required workbench is rejected"), InvalidRecipe);
	InvalidRecipe = ValidRecipe;
	InvalidRecipe.ResultItemId = NAME_None;
	TestInvalidRecipe(TEXT("None result Item ID is rejected"), InvalidRecipe);
	InvalidRecipe = ValidRecipe;
	InvalidRecipe.ResultAmount = 0;
	TestInvalidRecipe(TEXT("Non-positive result quantity is rejected"), InvalidRecipe);
	InvalidRecipe = ValidRecipe;
	InvalidRecipe.Ingredients.Empty();
	TestInvalidRecipe(TEXT("Empty ingredient list is rejected"), InvalidRecipe);
	InvalidRecipe = ValidRecipe;
	InvalidRecipe.Ingredients[0].ItemId = NAME_None;
	TestInvalidRecipe(TEXT("None ingredient Item ID is rejected"), InvalidRecipe);
	InvalidRecipe = ValidRecipe;
	InvalidRecipe.Ingredients[0].Amount = 0;
	TestInvalidRecipe(TEXT("Non-positive ingredient quantity is rejected"), InvalidRecipe);
	InvalidRecipe = ValidRecipe;
	InvalidRecipe.CraftingTime = -1.0f;
	TestInvalidRecipe(TEXT("Negative crafting time is rejected"), InvalidRecipe);
	InvalidRecipe = ValidRecipe;
	InvalidRecipe.CraftingTime = std::numeric_limits<float>::infinity();
	TestInvalidRecipe(TEXT("Non-finite crafting time is rejected"), InvalidRecipe);
	ReplaceRecipe(*RecipeTable, ValidRecipe);

	TestTrue(
		TEXT("Valid Basic workbench recipe creates one WorkOrder"),
		TryCraft(
			Workbench,
			RecipeTable.Get(),
			TestRecipeRowId,
			ReturnedWorkOrderId));
	const FAIRECompanionWorkOrderSnapshot CraftSnapshot =
		CraftWorkOrder->GetWorkOrderSnapshot();
	TestTrue(
		TEXT("Craft WorkOrder ID is returned unchanged"),
		ReturnedWorkOrderId.IsValid()
			&& ReturnedWorkOrderId == CraftSnapshot.WorkOrderId);
	TestTrue(
		TEXT("Craft WorkOrder snapshot is typed and Requested"),
		CraftSnapshot.WorkType == EAIRECompanionWorkOrderType::Crafting
			&& CraftSnapshot.TargetActor.Get() == Workbench
			&& CraftSnapshot.RecipeTable.Get() == RecipeTable.Get()
			&& CraftSnapshot.RecipeRowId == TestRecipeRowId
			&& CraftSnapshot.State == EAIRECompanionWorkOrderState::Requested);
	FGuid DuplicateCraftId;
	TestFalse(
		TEXT("A second active crafting request is rejected"),
		TryCraft(
			Workbench,
			RecipeTable.Get(),
			TestRecipeRowId,
			DuplicateCraftId));
	TestFalse(
		TEXT("Rejected active crafting request returns no ID"),
		DuplicateCraftId.IsValid());
	TestTrue(
		TEXT("Rejected active crafting request preserves snapshot"),
		AreSameWorkOrders(
			CraftSnapshot,
			CraftWorkOrder->GetWorkOrderSnapshot()));
	Workbench->Destroy();
	TestTrue(
		TEXT("Destroyed crafting target fails the active WorkOrder"),
		CraftWorkOrder->GetWorkOrderSnapshot().State
			== EAIRECompanionWorkOrderState::Failed);
	TestFalse(
		TEXT("Destroyed crafting target is rejected"),
		FAIRECompanionCraftingWorkRequest::IsValidRequestInputs(
			Workbench,
			RecipeTable.Get(),
			TestRecipeRowId));

	AAI_REHarvestableResourceActor* ResourceActor =
		TestWorld->SpawnActor<AAI_REHarvestableResourceActor>(
			FVector(500.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!TestNotNull(TEXT("Harvest test resource is spawned"), ResourceActor))
	{
		return false;
	}
	TStrongObjectPtr<UAI_REItemDataAsset> RewardItem(
		NewObject<UAI_REItemDataAsset>());
	RewardItem->ItemId = FName(TEXT("AIRE.Test.HarvestReward"));
	ResourceActor->ItemActorClass = AAI_REItemActor::StaticClass();
	UAI_REHarvestableResourceComponent* ResourceComponent =
		ResourceActor->GetHarvestableResourceComponent();
	if (!TestNotNull(TEXT("Harvest resource component exists"), ResourceComponent))
	{
		return false;
	}
	ResourceComponent->SetResourceDefaults(
		FGameplayTag(),
		RewardItem.Get(),
		1,
		0.0f);
	TestFalse(
		TEXT("Null Harvest target is rejected"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(nullptr));
	TestTrue(
		TEXT("Zero reward interval is a valid reward-per-hit resource"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor));
	ResourceActor->ItemActorClass = nullptr;
	TestFalse(
		TEXT("Resource without world-drop fallback is rejected"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor));
	ResourceActor->ItemActorClass = AAI_REItemActor::StaticClass();
	ResourceComponent->SetResourceDefaults(FGameplayTag(), nullptr, 1, 0.0f);
	TestFalse(
		TEXT("Resource without reward item is rejected"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor));
	RewardItem->ItemId = NAME_None;
	ResourceComponent->SetResourceDefaults(
		FGameplayTag(),
		RewardItem.Get(),
		1,
		0.0f);
	TestFalse(
		TEXT("Resource with None reward Item ID is rejected"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor));
	RewardItem->ItemId = FName(TEXT("AIRE.Test.HarvestReward"));
	ResourceComponent->SetResourceDefaults(
		FGameplayTag(),
		RewardItem.Get(),
		0,
		0.0f);
	TestFalse(
		TEXT("Resource with non-positive reward amount is rejected"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor));
	ResourceComponent->SetResourceDefaults(
		FGameplayTag(),
		RewardItem.Get(),
		1,
		std::numeric_limits<float>::infinity());
	TestFalse(
		TEXT("Resource with non-finite reward interval is rejected"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor));
	ResourceComponent->SetResourceDefaults(
		FGameplayTag(),
		RewardItem.Get(),
		1,
		0.0f);

	TStrongObjectPtr<UAIRECompanionWorkOrderComponent> HarvestWorkOrder(
		NewObject<UAIRECompanionWorkOrderComponent>(ResourceActor));
	FGuid HarvestWorkOrderId;
	TestFalse(
		TEXT("Null WorkOrder component rejects Harvest request"),
		FAIRECompanionHarvestWorkRequest::TryRequest(
			nullptr,
			ResourceActor,
			HarvestWorkOrderId));
	TestFalse(
		TEXT("Null Harvest component request returns no ID"),
		HarvestWorkOrderId.IsValid());
	TestTrue(
		TEXT("Valid resource creates one Harvest WorkOrder"),
		FAIRECompanionHarvestWorkRequest::TryRequest(
			HarvestWorkOrder.Get(),
			ResourceActor,
			HarvestWorkOrderId));
	const FAIRECompanionWorkOrderSnapshot HarvestSnapshot =
		HarvestWorkOrder->GetWorkOrderSnapshot();
	TestTrue(
		TEXT("Harvest WorkOrder snapshot is typed and Requested"),
		HarvestWorkOrderId.IsValid()
			&& HarvestWorkOrderId == HarvestSnapshot.WorkOrderId
			&& HarvestSnapshot.WorkType
				== EAIRECompanionWorkOrderType::Harvesting
			&& HarvestSnapshot.TargetActor.Get() == ResourceActor
			&& HarvestSnapshot.RecipeTable.Get() == nullptr
			&& HarvestSnapshot.RecipeRowId.IsNone()
			&& HarvestSnapshot.State
				== EAIRECompanionWorkOrderState::Requested);
	FGuid DuplicateHarvestId;
	TestFalse(
		TEXT("A second active Harvest request is rejected"),
		FAIRECompanionHarvestWorkRequest::TryRequest(
			HarvestWorkOrder.Get(),
			ResourceActor,
			DuplicateHarvestId));
	TestFalse(
		TEXT("Rejected active Harvest request returns no ID"),
		DuplicateHarvestId.IsValid());
	TestTrue(
		TEXT("Rejected active Harvest request preserves snapshot"),
		AreSameWorkOrders(
			HarvestSnapshot,
			HarvestWorkOrder->GetWorkOrderSnapshot()));
	ResourceComponent->SetResourceDefaults(
		FGameplayTag(),
		RewardItem.Get(),
		1,
		10000.0f);
	TestTrue(
		TEXT("Harvest damage depletes the test resource"),
		ResourceComponent->ApplyHarvestDamage(1000.0f, nullptr));
	TestFalse(
		TEXT("Depleted resource is rejected"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor));
	ResourceActor->Destroy();
	TestTrue(
		TEXT("Destroyed Harvest target fails the active WorkOrder"),
		HarvestWorkOrder->GetWorkOrderSnapshot().State
			== EAIRECompanionWorkOrderState::Failed);
	TestFalse(
		TEXT("Destroyed Harvest target is rejected"),
		FAIRECompanionHarvestWorkRequest::IsValidRequestInputs(ResourceActor));
	return true;
}

#endif

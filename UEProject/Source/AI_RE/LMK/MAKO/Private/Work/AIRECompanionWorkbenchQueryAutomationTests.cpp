#if WITH_DEV_AUTOMATION_TESTS

#include "Work/AIRECompanionWorkbenchQuery.h"

#include "AI_REWorkBenchBase.h"
#include "AI_REWorkbenchGameplayTags.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

namespace
{
	void AddWorkbenchCollision(
		AAI_REWorkBenchBase* Workbench,
		const ECollisionChannel ObjectType)
	{
		check(Workbench);

		UBoxComponent* Collision = NewObject<UBoxComponent>(
			Workbench,
			TEXT("WorkbenchQueryAutomationCollision"));
		check(Collision);
		Workbench->AddInstanceComponent(Collision);
		Collision->SetupAttachment(Workbench->GetRootComponent());
		Collision->SetBoxExtent(FVector(30.0f));
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->SetCollisionObjectType(ObjectType);
		Collision->SetCollisionResponseToAllChannels(ECR_Overlap);
		Collision->RegisterComponent();
	}

	AAI_REWorkBenchBase* SpawnWorkbench(
		UWorld& World,
		const FVector& Location,
		const ECollisionChannel ObjectType)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAI_REWorkBenchBase* Workbench = World.SpawnActor<AAI_REWorkBenchBase>(
			Location,
			FRotator::ZeroRotator,
			SpawnParameters);
		if (Workbench != nullptr)
		{
			AddWorkbenchCollision(Workbench, ObjectType);
		}
		return Workbench;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECompanionWorkbenchQueryAutomationTest,
	"AIRE.Companion.Work.WorkbenchQuery",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECompanionWorkbenchQueryAutomationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	if (!TestNotNull(TEXT("Engine is available"), GEngine))
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
	if (!TestNotNull(TEXT("Transient workbench query world is created"), TestWorld))
	{
		return false;
	}

	FWorldContext& WorldContext =
		GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		TestWorld->EndPlay(EEndPlayReason::Quit);
		GEngine->ShutdownWorldNetDriver(TestWorld);
		TestWorld->DestroyWorld(true);
		TestWorld->SetPhysicsScene(nullptr);
		GEngine->DestroyWorldContext(TestWorld);
	};

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Origin = TestWorld->SpawnActor<AActor>(
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!TestNotNull(TEXT("Companion query origin is spawned"), Origin))
	{
		return false;
	}

	AAI_REWorkBenchBase* NearestBasic = SpawnWorkbench(
		*TestWorld,
		FVector(100.0f, 0.0f, 0.0f),
		ECC_WorldStatic);
	AAI_REWorkBenchBase* NearestBlacksmith = SpawnWorkbench(
		*TestWorld,
		FVector(200.0f, 0.0f, 0.0f),
		ECC_WorldDynamic);
	if (!TestNotNull(TEXT("Nearest basic workbench is spawned"), NearestBasic)
		|| !TestNotNull(TEXT("Nearest blacksmith workbench is spawned"), NearestBlacksmith))
	{
		return false;
	}
	NearestBlacksmith->WorkbenchType = EWorkbenchType::Blacksmith;
	NearestBlacksmith->WorkbenchTags.AddTag(
		AI_REWorkbenchGameplayTags::Workbench_Blacksmith);

	for (int32 Index = 2; Index < 9; ++Index)
	{
		AAI_REWorkBenchBase* Workbench = SpawnWorkbench(
			*TestWorld,
			FVector(static_cast<float>((Index + 1) * 100), 0.0f, 0.0f),
			Index % 2 == 0 ? ECC_WorldStatic : ECC_WorldDynamic);
		if (!TestNotNull(TEXT("Additional workbench is spawned"), Workbench))
		{
			return false;
		}
	}
	AAI_REWorkBenchBase* OutsideRadius = SpawnWorkbench(
		*TestWorld,
		FVector(FAIRECompanionWorkbenchQuery::DefaultRadiusCentimeters + 100.0f, 0.0f, 0.0f),
		ECC_WorldStatic);
	if (!TestNotNull(TEXT("Outside-radius workbench is spawned"), OutsideRadius))
	{
		return false;
	}

	TestWorld->BeginPlay();
	TestWorld->GetWorldSettings()->NotifyBeginPlay();

	TArray<TWeakObjectPtr<AAI_REWorkBenchBase>> NearbyWorkbenches;
	TestTrue(
		TEXT("Bounded overlap query executes"),
		FAIRECompanionWorkbenchQuery::CollectNearbyWorkbenches(
			*Origin,
			NearbyWorkbenches));
	TestEqual(
		TEXT("Query returns at most eight workbenches"),
		NearbyWorkbenches.Num(),
		FAIRECompanionWorkbenchQuery::MaxNearbyWorkbenches);
	TestTrue(
		TEXT("Nearest workbench is first"),
		NearbyWorkbenches.Num() > 0
			&& NearbyWorkbenches[0].Get() == NearestBasic);
	TestTrue(
		TEXT("Outside-radius workbench is excluded"),
		!NearbyWorkbenches.ContainsByPredicate(
			[OutsideRadius](
				const TWeakObjectPtr<AAI_REWorkBenchBase>& Workbench)
			{
				return Workbench.Get() == OutsideRadius;
			}));

	TArray<FString> CapabilityIds;
	TestTrue(
		TEXT("Capability query executes"),
		FAIRECompanionWorkbenchQuery::GetNearbyCapabilityIds(
			*Origin,
			CapabilityIds));
	TestTrue(
		TEXT("Basic capability is mapped to its native tag"),
		CapabilityIds.Contains(TEXT("Workbench.Basic")));
	TestTrue(
		TEXT("Blacksmith capability is mapped to its native tag"),
		CapabilityIds.Contains(TEXT("Workbench.Blacksmith")));
	TestTrue(
		TEXT("Configured Forge tag is preserved"),
		CapabilityIds.Contains(TEXT("Workbench.Forge")));

	TestTrue(
		TEXT("Nearest compatible workbench is selected by type"),
		FAIRECompanionWorkbenchQuery::FindNearestCompatible(
			*Origin,
			EWorkbenchType::Blacksmith) == NearestBlacksmith);
	TestNull(
		TEXT("None workbench type is never selected"),
		FAIRECompanionWorkbenchQuery::FindNearestCompatible(
			*Origin,
			EWorkbenchType::None));

	NearestBlacksmith->Destroy();
	TestNull(
		TEXT("Destroyed workbench is not selected"),
		FAIRECompanionWorkbenchQuery::FindNearestCompatible(
			*Origin,
			EWorkbenchType::Blacksmith));

	TArray<TWeakObjectPtr<AAI_REWorkBenchBase>> InvalidRadiusOutput;
	TestFalse(
		TEXT("Non-positive radius is rejected"),
		FAIRECompanionWorkbenchQuery::CollectNearbyWorkbenches(
			*Origin,
			InvalidRadiusOutput,
			0.0f));
	TestTrue(
		TEXT("Rejected query clears output"),
		InvalidRadiusOutput.IsEmpty());
	TestFalse(
		TEXT("Radius above the bounded maximum is rejected"),
		FAIRECompanionWorkbenchQuery::CollectNearbyWorkbenches(
			*Origin,
			InvalidRadiusOutput,
			FAIRECompanionWorkbenchQuery::DefaultRadiusCentimeters + 1.0f));
	TestTrue(
		TEXT("Over-limit query clears output"),
		InvalidRadiusOutput.IsEmpty());

	return true;
}

#endif

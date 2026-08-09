#if WITH_DEV_AUTOMATION_TESTS

#include "AIRECombatEvadeComponent.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Core/AIRECompanionCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

namespace
{
	void AddSceneRoot(AActor* Actor)
	{
		USceneComponent* Root = NewObject<USceneComponent>(
			Actor,
			TEXT("EvadeTestRoot"));
		Actor->AddInstanceComponent(Root);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
	}

	void AddPawnBlockingBox(AActor* Actor)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(
			Actor,
			TEXT("EvadeTestBlocker"));
		Actor->AddInstanceComponent(Box);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(100.0f, 10.0f, 100.0f));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Box->RegisterComponent();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECompanionCombatEvadeComponentTest,
	"AIRE.Companion.Combat.AutonomousEvade.Component",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECompanionCombatEvadeComponentTest::RunTest(
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
	if (!TestNotNull(TEXT("Transient evade world is created"), TestWorld))
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
	AAIRECompanionCharacter* Companion =
		TestWorld->SpawnActor<AAIRECompanionCharacter>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
	AActor* Threat = TestWorld->SpawnActor<AActor>(
		FVector(500.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	AActor* RightBlocker = TestWorld->SpawnActor<AActor>(
		FVector(0.0f, 1000.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	AActor* LeftBlocker = TestWorld->SpawnActor<AActor>(
		FVector(0.0f, -1000.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!TestNotNull(TEXT("Companion is spawned"), Companion)
		|| !TestNotNull(TEXT("Threat is spawned"), Threat)
		|| !TestNotNull(TEXT("Right blocker is spawned"), RightBlocker)
		|| !TestNotNull(TEXT("Left blocker is spawned"), LeftBlocker))
	{
		return false;
	}
	AddSceneRoot(Threat);
	AddPawnBlockingBox(RightBlocker);
	AddPawnBlockingBox(LeftBlocker);
	Threat->SetActorLocation(FVector(500.0f, 0.0f, 0.0f));
	RightBlocker->SetActorLocation(FVector(0.0f, 1000.0f, 0.0f));
	LeftBlocker->SetActorLocation(FVector(0.0f, -1000.0f, 0.0f));
	TestWorld->BeginPlay();
	TestWorld->GetWorldSettings()->NotifyBeginPlay();

	UAIRECombatEvadeComponent* Evade =
		Companion->GetCombatEvadeComponent();
	if (!TestNotNull(TEXT("Companion evade component is available"), Evade))
	{
		return false;
	}
	const FGuid ExecutionId(11, 22, 33, 44);
	FAIRECombatEvadePlan Plan;
	TestTrue(
		TEXT("A clear lateral dash plan is available"),
		Evade->BuildLateralDashPlan(Threat, ExecutionId, Plan));
	TestEqual(
		TEXT("Equal clear sides select the right side"),
		Plan.Side,
		EAIRECombatEvadeSide::Right);
	TestTrue(
		TEXT("A clear side exposes the full 300 cm dash"),
		FMath::IsNearlyEqual(Plan.AvailableDistance, 300.0f));

	RightBlocker->SetActorLocation(FVector(0.0f, 150.0f, 0.0f));
	TestTrue(
		TEXT("A plan is still available with the right side obstructed"),
		Evade->BuildLateralDashPlan(Threat, ExecutionId, Plan));
	TestEqual(
		TEXT("A right-side obstruction selects the clear left side"),
		Plan.Side,
		EAIRECombatEvadeSide::Left);

	LeftBlocker->SetActorLocation(FVector(0.0f, -150.0f, 0.0f));
	TestTrue(
		TEXT("A plan is available with symmetric obstructions"),
		Evade->BuildLateralDashPlan(Threat, ExecutionId, Plan));
	TestEqual(
		TEXT("Equal obstructed sides still select the right side"),
		Plan.Side,
		EAIRECombatEvadeSide::Right);
	TestTrue(
		TEXT("The obstructed plan stops before the full dash distance"),
		Plan.AvailableDistance < 300.0f);

	RightBlocker->SetActorEnableCollision(false);
	LeftBlocker->SetActorEnableCollision(false);
	TestTrue(
		TEXT("A final clear dash plan is available"),
		Evade->BuildLateralDashPlan(Threat, ExecutionId, Plan));
	const EMovementMode PreviousMovementMode =
		Companion->GetCharacterMovement()->MovementMode;

	RightBlocker->SetActorEnableCollision(true);
	RightBlocker->SetActorLocation(FVector(0.0f, 150.0f, 0.0f));
	const FVector BlockedDashStart = Companion->GetActorLocation();
	TestTrue(
		TEXT("A newly introduced obstruction does not reject the prepared plan"),
		Evade->TryStartLateralDashPlan(Plan));
	for (int32 TickIndex = 0; TickIndex < 6; ++TickIndex)
	{
		TestWorld->Tick(LEVELTICK_All, 0.05f);
	}
	TestFalse(
		TEXT("Swept movement ends the dash on a new blocking contact"),
		Evade->IsEvading());
	TestTrue(
		TEXT("The new obstruction prevents the prepared full-distance dash"),
		FVector::Dist2D(
			BlockedDashStart,
			Companion->GetActorLocation()) < 300.0f);
	RightBlocker->SetActorEnableCollision(false);

	for (int32 RepeatIndex = 0; RepeatIndex < 3; ++RepeatIndex)
	{
		Companion->SetActorLocation(
			FVector::ZeroVector,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		TestTrue(
			TEXT("A repeated clear dash plan is available"),
			Evade->BuildLateralDashPlan(Threat, ExecutionId, Plan));
		const FVector StartLocation = Companion->GetActorLocation();
		TestTrue(
			TEXT("The prepared dash starts"),
			Evade->TryStartLateralDashPlan(Plan));
		TestTrue(
			TEXT("The active dash retains its threat and execution context"),
			Evade->IsEvadingFrom(Threat, ExecutionId));
		for (int32 TickIndex = 0; TickIndex < 6; ++TickIndex)
		{
			TestWorld->Tick(LEVELTICK_All, 0.05f);
		}
		TestFalse(TEXT("The 0.25 second dash finishes"), Evade->IsEvading());
		TestTrue(
			TEXT("The code-driven dash travels 300 cm without root motion"),
			FMath::IsNearlyEqual(
				FVector::Dist2D(
					StartLocation,
					Companion->GetActorLocation()),
				300.0f,
				1.0f));
		TestEqual(
			TEXT("The prior movement mode is restored"),
			Companion->GetCharacterMovement()->MovementMode,
			PreviousMovementMode);
	}
	return true;
}

#endif

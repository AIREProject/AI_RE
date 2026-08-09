#if WITH_DEV_AUTOMATION_TESTS

#include "AIRECombatMeleeTraceResolver.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

namespace
{
	void AddRootComponent(AActor* Actor)
	{
		check(Actor);
		USceneComponent* Root = NewObject<USceneComponent>(
			Actor,
			TEXT("MeleeTraceResolverTestRoot"));
		check(Root);
		Actor->AddInstanceComponent(Root);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
	}

	void AddPawnCollision(AActor* Actor)
	{
		check(Actor);
		USphereComponent* Collision = NewObject<USphereComponent>(
			Actor,
			TEXT("MeleeTraceResolverTestTarget"));
		check(Collision);
		Actor->AddInstanceComponent(Collision);
		Actor->SetRootComponent(Collision);
		Collision->SetSphereRadius(40.0f);
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->SetCollisionObjectType(ECC_Pawn);
		Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
		Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Collision->RegisterComponent();
	}

	void AddBlockingCollision(AActor* Actor)
	{
		check(Actor);
		UBoxComponent* Collision = NewObject<UBoxComponent>(
			Actor,
			TEXT("MeleeTraceResolverTestBlocker"));
		check(Collision);
		Actor->AddInstanceComponent(Collision);
		Actor->SetRootComponent(Collision);
		Collision->SetBoxExtent(FVector(10.0f, 40.0f, 40.0f));
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->SetCollisionObjectType(ECC_WorldStatic);
		Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
		Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Collision->RegisterComponent();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECombatMeleeTraceResolverTest,
	"AIRE.Combat.MeleeTrace.Resolver",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECombatMeleeTraceResolverTest::RunTest(const FString& Parameters)
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
	if (!TestNotNull(TEXT("Transient trace world is created"), TestWorld))
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
	AActor* Source = TestWorld->SpawnActor<AActor>(
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	AActor* Target = TestWorld->SpawnActor<AActor>(
		FVector(200.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	AActor* Blocker = TestWorld->SpawnActor<AActor>(
		FVector(100.0f, 300.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	AActor* PawnBlocker = TestWorld->SpawnActor<AActor>(
		FVector(100.0f, -300.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	AActor* AttachmentParent = TestWorld->SpawnActor<AActor>(
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	AActor* AttachedBlocker = TestWorld->SpawnActor<AActor>(
		FVector(100.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!TestNotNull(TEXT("Source is spawned"), Source)
		|| !TestNotNull(TEXT("Target is spawned"), Target)
		|| !TestNotNull(TEXT("Blocker is spawned"), Blocker)
		|| !TestNotNull(TEXT("Pawn blocker is spawned"), PawnBlocker)
		|| !TestNotNull(TEXT("Attachment parent is spawned"), AttachmentParent)
		|| !TestNotNull(TEXT("Attached blocker is spawned"), AttachedBlocker))
	{
		return false;
	}

	AddRootComponent(Source);
	AddRootComponent(AttachmentParent);
	AddPawnCollision(Target);
	AddBlockingCollision(Blocker);
	AddPawnCollision(PawnBlocker);
	AddBlockingCollision(AttachedBlocker);
	Source->SetActorLocation(FVector::ZeroVector);
	Target->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	Blocker->SetActorLocation(FVector(100.0f, 300.0f, 0.0f));
	PawnBlocker->SetActorLocation(FVector(100.0f, -300.0f, 0.0f));
	AttachedBlocker->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	AttachmentParent->AttachToActor(
		Source,
		FAttachmentTransformRules::KeepWorldTransform);
	AttachedBlocker->AttachToActor(
		AttachmentParent,
		FAttachmentTransformRules::KeepWorldTransform);
	TestWorld->BeginPlay();
	TestWorld->GetWorldSettings()->NotifyBeginPlay();

	FAIRECombatMeleeTraceRequest Request;
	Request.World = TestWorld;
	Request.Source = Source;
	Request.Target = Target;
	Request.Radius = 10.0f;
	Request.TraceChannel = ECC_Pawn;

	FAIRECombatMeleeTraceResolution Resolution =
		FAIRECombatMeleeTraceResolver::Resolve(Request);
	TestTrue(
		TEXT("An empty segment list is invalid"),
		Resolution.Result == EAIRECombatMeleeTraceResult::Invalid);

	Request.Segments.Emplace(
		FVector(0.0f, 600.0f, 0.0f),
		FVector(200.0f, 600.0f, 0.0f));
	Resolution = FAIRECombatMeleeTraceResolver::Resolve(Request);
	TestTrue(
		TEXT("A clear segment outside the target is a miss"),
		Resolution.Result == EAIRECombatMeleeTraceResult::NoHit);

	Request.Segments.Reset();
	Request.Segments.Emplace(
		FVector(0.0f, 300.0f, 0.0f),
		FVector(200.0f, 300.0f, 0.0f));
	Resolution = FAIRECombatMeleeTraceResolver::Resolve(Request);
	TestTrue(
		TEXT("A non-target blocking hit is classified as blocked"),
		Resolution.Result == EAIRECombatMeleeTraceResult::Blocked);

	Request.Segments.Reset();
	Request.Segments.Emplace(
		FVector::ZeroVector,
		FVector(200.0f, 0.0f, 0.0f));
	Resolution = FAIRECombatMeleeTraceResolver::Resolve(Request);
	TestTrue(
		TEXT("An attached blocker is ignored recursively"),
		Resolution.Result == EAIRECombatMeleeTraceResult::TargetHit);
	TestTrue(
		TEXT("The target hit result is returned"),
		Resolution.HitResult.GetActor() == Target);

	PawnBlocker->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	Resolution = FAIRECombatMeleeTraceResolver::Resolve(Request);
	TestTrue(
		TEXT("An intervening non-target Pawn blocks the target"),
		Resolution.Result == EAIRECombatMeleeTraceResult::Blocked);
	PawnBlocker->SetActorLocation(FVector(100.0f, -300.0f, 0.0f));

	AttachedBlocker->DetachFromActor(
		FDetachmentTransformRules::KeepWorldTransform);
	Resolution = FAIRECombatMeleeTraceResolver::Resolve(Request);
	TestTrue(
		TEXT("The same actor blocks after it is detached"),
		Resolution.Result == EAIRECombatMeleeTraceResult::Blocked);
	AttachedBlocker->AttachToActor(
		AttachmentParent,
		FAttachmentTransformRules::KeepWorldTransform);

	Request.Segments.Reset();
	Request.Segments.Reserve(4);
	Request.Segments.Emplace(
		FVector(0.0f, -100.0f, 0.0f),
		FVector(0.0f, 100.0f, 0.0f));
	Request.Segments.Emplace(
		FVector(200.0f, -100.0f, 0.0f),
		FVector(200.0f, 100.0f, 0.0f));
	Request.Segments.Emplace(
		FVector(0.0f, -100.0f, 0.0f),
		FVector(200.0f, -100.0f, 0.0f));
	Request.Segments.Emplace(
		FVector(0.0f, 100.0f, 0.0f),
		FVector(200.0f, 100.0f, 0.0f));
	Resolution = FAIRECombatMeleeTraceResolver::Resolve(Request);
	TestTrue(
		TEXT("Endpoint motion catches tunnelling while both blade spans miss"),
		Resolution.Result == EAIRECombatMeleeTraceResult::TargetHit);

	Request.Radius = 0.0f;
	Resolution = FAIRECombatMeleeTraceResolver::Resolve(Request);
	TestTrue(
		TEXT("A non-positive sphere radius is invalid"),
		Resolution.Result == EAIRECombatMeleeTraceResult::Invalid);

	return true;
}

#endif

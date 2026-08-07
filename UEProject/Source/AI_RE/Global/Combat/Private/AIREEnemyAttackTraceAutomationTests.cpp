#if WITH_DEV_AUTOMATION_TESTS

#include "AIREEnemyAttackComponent.h"

#include "AIREBossEnemy.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Testing/AIRECompanionCombatTestTarget.h"

namespace
{
constexpr float TestTargetHealth = 100.0f;

void AddPartyCollision(AAIRECompanionCombatTestTarget* Target)
{
	check(Target);
	USphereComponent* Collision = NewObject<USphereComponent>(
		Target,
		TEXT("AttackTraceTestCollision"));
	check(Collision);
	Target->AddInstanceComponent(Collision);
	Target->SetRootComponent(Collision);
	Collision->SetSphereRadius(50.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_Pawn);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->RegisterComponent();
}

void AddWorldStaticCollision(AActor* Occluder)
{
	check(Occluder);
	UBoxComponent* Collision = NewObject<UBoxComponent>(
		Occluder,
		TEXT("AttackTraceTestOccluder"));
	check(Collision);
	Occluder->AddInstanceComponent(Collision);
	Occluder->SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(10.0f, 80.0f, 100.0f));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldStatic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->RegisterComponent();
}

float GetHealth(const AAIRECompanionCombatTestTarget* Target)
{
	const UAbilitySystemComponent* AbilitySystem =
		Target ? Target->GetAbilitySystemComponent() : nullptr;
	return IsValid(AbilitySystem)
		? AbilitySystem->GetNumericAttribute(
			UAIRECompanionAttributeSet::GetHealthAttribute())
		: -1.0f;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREEnemyAttackFallbackTraceTest,
	"AIRE.Combat.Enemy.Attack.FallbackTrace",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREEnemyAttackFallbackTraceTest::RunTest(const FString& Parameters)
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
	if (!TestNotNull(TEXT("Transient attack trace world is created"), TestWorld))
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
	auto SpawnBoss = [&](const FVector& Location)
	{
		return TestWorld->SpawnActor<AAIREBossEnemy>(
			Location,
			FRotator::ZeroRotator,
			SpawnParameters);
	};
	auto SpawnPartyTarget = [&](const FVector& Location)
	{
		AAIRECompanionCombatTestTarget* Target =
			TestWorld->SpawnActor<AAIRECompanionCombatTestTarget>(
				Location,
				FRotator::ZeroRotator,
				SpawnParameters);
		if (IsValid(Target))
		{
			Target->SetHostileForTesting(false);
			AddPartyCollision(Target);
		}
		return Target;
	};

	AAIREBossEnemy* FrontBoss = SpawnBoss(FVector::ZeroVector);
	AAIRECompanionCombatTestTarget* FrontTarget =
		SpawnPartyTarget(FVector(150.0f, 0.0f, 0.0f));
	AAIREBossEnemy* RearBoss = SpawnBoss(FVector(0.0f, 1000.0f, 0.0f));
	AAIRECompanionCombatTestTarget* RearTarget =
		SpawnPartyTarget(FVector(150.0f, 1000.0f, 0.0f));
	AAIREBossEnemy* SideBoss = SpawnBoss(FVector(0.0f, 2000.0f, 0.0f));
	AAIRECompanionCombatTestTarget* SideTarget =
		SpawnPartyTarget(FVector(150.0f, 2000.0f, 0.0f));
	AAIREBossEnemy* OccludedBoss = SpawnBoss(FVector(0.0f, 3000.0f, 0.0f));
	AAIRECompanionCombatTestTarget* OccludedTarget =
		SpawnPartyTarget(FVector(150.0f, 3000.0f, 0.0f));
	AActor* Occluder = TestWorld->SpawnActor<AActor>(
		FVector(90.0f, 3000.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (IsValid(Occluder))
	{
		AddWorldStaticCollision(Occluder);
	}
	AAIREBossEnemy* CancelBoss = SpawnBoss(FVector(0.0f, 4000.0f, 0.0f));
	AAIRECompanionCombatTestTarget* CancelTarget =
		SpawnPartyTarget(FVector(150.0f, 4000.0f, 0.0f));
	AAIREBossEnemy* DestroyBoss = SpawnBoss(FVector(0.0f, 5000.0f, 0.0f));
	AAIRECompanionCombatTestTarget* DestroyTarget =
		SpawnPartyTarget(FVector(150.0f, 5000.0f, 0.0f));
	if (!TestNotNull(TEXT("Front boss is spawned"), FrontBoss)
		|| !TestNotNull(TEXT("Front party target is spawned"), FrontTarget)
		|| !TestNotNull(TEXT("Rear boss is spawned"), RearBoss)
		|| !TestNotNull(TEXT("Rear party target is spawned"), RearTarget)
		|| !TestNotNull(TEXT("Side boss is spawned"), SideBoss)
		|| !TestNotNull(TEXT("Side party target is spawned"), SideTarget)
		|| !TestNotNull(TEXT("Occluded boss is spawned"), OccludedBoss)
		|| !TestNotNull(TEXT("Occluded party target is spawned"), OccludedTarget)
		|| !TestNotNull(TEXT("World-static occluder is spawned"), Occluder)
		|| !TestNotNull(TEXT("Cancel boss is spawned"), CancelBoss)
		|| !TestNotNull(TEXT("Cancel party target is spawned"), CancelTarget)
		|| !TestNotNull(TEXT("Destroy boss is spawned"), DestroyBoss)
		|| !TestNotNull(TEXT("Destroy party target is spawned"), DestroyTarget))
	{
		return false;
	}

	TestWorld->BeginPlay();
	AAIREBossEnemy* TestBosses[] = {
		FrontBoss,
		RearBoss,
		SideBoss,
		OccludedBoss,
		CancelBoss,
		DestroyBoss
	};
	for (AAIREBossEnemy* Boss : TestBosses)
	{
		if (AController* Controller = Boss->GetController())
		{
			Controller->SetActorTickEnabled(false);
		}
	}

	FAIREEnemyMeleeTraceSettings TraceSettings;
	TraceSettings.TraceRadius = 20.0f;
	TraceSettings.FallbackTraceDistance = 220.0f;
	TraceSettings.TraceChannel = ECC_Pawn;
	auto ConfigureFallbackAttack = [&](AAIREBossEnemy* Boss)
	{
		UAIREEnemyAttackComponent* Attack = Boss
			? Boss->GetEnemyAttackComponent()
			: nullptr;
		if (IsValid(Attack))
		{
			Attack->ConfigureDefaults(
				250.0f,
				25.0f,
				0.0f,
				0.0f,
				0.01f,
				0.2f,
				TraceSettings);
		}
		return Attack;
	};

	UAIREEnemyAttackComponent* FrontAttack = ConfigureFallbackAttack(FrontBoss);
	UAIREEnemyAttackComponent* RearAttack = ConfigureFallbackAttack(RearBoss);
	UAIREEnemyAttackComponent* SideAttack = ConfigureFallbackAttack(SideBoss);
	UAIREEnemyAttackComponent* OccludedAttack =
		ConfigureFallbackAttack(OccludedBoss);
	UAIREEnemyAttackComponent* CancelAttack = ConfigureFallbackAttack(CancelBoss);
	UAIREEnemyAttackComponent* DestroyAttack = ConfigureFallbackAttack(DestroyBoss);
	if (!TestNotNull(TEXT("Front attack component is available"), FrontAttack)
		|| !TestNotNull(TEXT("Rear attack component is available"), RearAttack)
		|| !TestNotNull(TEXT("Side attack component is available"), SideAttack)
		|| !TestNotNull(TEXT("Occluded attack component is available"), OccludedAttack)
		|| !TestNotNull(TEXT("Cancel attack component is available"), CancelAttack)
		|| !TestNotNull(TEXT("Destroy attack component is available"), DestroyAttack))
	{
		return false;
	}

	TestTrue(TEXT("Forward fallback attack starts"),
		FrontAttack->TryStartMeleeAttack(FrontTarget));
	TestTrue(TEXT("Rear fallback attack starts for a spatial miss"),
		RearAttack->TryStartMeleeAttack(RearTarget));
	TestTrue(TEXT("Side fallback attack starts for a spatial miss"),
		SideAttack->TryStartMeleeAttack(SideTarget));
	TestTrue(TEXT("Occluded fallback attack starts"),
		OccludedAttack->TryStartMeleeAttack(OccludedTarget));
	TestTrue(TEXT("Cancellable fallback attack starts"),
		CancelAttack->TryStartMeleeAttack(CancelTarget));
	TestTrue(TEXT("Destroyable-target fallback attack starts"),
		DestroyAttack->TryStartMeleeAttack(DestroyTarget));

	RearTarget->SetActorLocation(FVector(-150.0f, 1000.0f, 0.0f));
	SideTarget->SetActorLocation(FVector(0.0f, 2150.0f, 0.0f));

	CancelAttack->CancelCurrentAttack();
	DestroyTarget->Destroy();
	TestFalse(TEXT("Target destruction closes the active attack immediately"),
		DestroyAttack->GetAttackSnapshot().bActive);

	TestWorld->Tick(LEVELTICK_All, 0.05f);
	TestTrue(TEXT("Forward sphere sweep applies damage"),
		FMath::IsNearlyEqual(GetHealth(FrontTarget), TestTargetHealth - 25.0f));
	TestTrue(TEXT("Rear target is missed by the forward sphere sweep"),
		FMath::IsNearlyEqual(GetHealth(RearTarget), TestTargetHealth));
	TestTrue(TEXT("Target outside the sphere sweep is missed"),
		FMath::IsNearlyEqual(GetHealth(SideTarget), TestTargetHealth));
	TestTrue(TEXT("World-static occluder blocks the fallback sphere sweep"),
		FMath::IsNearlyEqual(GetHealth(OccludedTarget), TestTargetHealth));
	TestTrue(TEXT("Cancellation clears the pending fallback hit"),
		FMath::IsNearlyEqual(GetHealth(CancelTarget), TestTargetHealth));
	TestFalse(TEXT("Destroyed target cannot leave an active attack behind"),
		DestroyAttack->GetAttackSnapshot().bActive);

	TestFalse(TEXT("Repeated resolution cannot apply the same attack twice"),
		FrontAttack->CommitActiveMeleeHit());
	TestTrue(TEXT("Repeated resolution preserves the first hit's health result"),
		FMath::IsNearlyEqual(GetHealth(FrontTarget), TestTargetHealth - 25.0f));

	return true;
}

#endif

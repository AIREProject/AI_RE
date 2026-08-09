#if WITH_DEV_AUTOMATION_TESTS

#include "AIREEnemyAttackComponent.h"

#include "AIREBossEnemy.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/WorldSettings.h"
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
		TestWorld->EndPlay(EEndPlayReason::Quit);
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
			Target->SetActorLocation(Location);
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
		Occluder->SetActorLocation(FVector(90.0f, 3000.0f, 0.0f));
	}
	AAIREBossEnemy* CancelBoss = SpawnBoss(FVector(0.0f, 4000.0f, 0.0f));
	AAIRECompanionCombatTestTarget* CancelTarget =
		SpawnPartyTarget(FVector(150.0f, 4000.0f, 0.0f));
	AAIREBossEnemy* DestroyBoss = SpawnBoss(FVector(0.0f, 5000.0f, 0.0f));
	AAIRECompanionCombatTestTarget* DestroyTarget =
		SpawnPartyTarget(FVector(150.0f, 5000.0f, 0.0f));
	AAIREBossEnemy* PatternBoss = SpawnBoss(FVector(0.0f, 6000.0f, 0.0f));
	AAIRECompanionCombatTestTarget* PatternTarget =
		SpawnPartyTarget(FVector(150.0f, 6000.0f, 0.0f));
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
		|| !TestNotNull(TEXT("Destroy party target is spawned"), DestroyTarget)
		|| !TestNotNull(TEXT("Pattern boss is spawned"), PatternBoss)
		|| !TestNotNull(TEXT("Pattern party target is spawned"), PatternTarget))
	{
		return false;
	}

	AAIREBossEnemy* TestBosses[] = {
		FrontBoss,
		RearBoss,
		SideBoss,
		OccludedBoss,
		CancelBoss,
		DestroyBoss,
		PatternBoss
	};
	for (AAIREBossEnemy* Boss : TestBosses)
	{
		Boss->AutoPossessAI = EAutoPossessAI::Disabled;
	}
	TestWorld->BeginPlay();
	TestWorld->GetWorldSettings()->NotifyBeginPlay();
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
	UAIREEnemyAttackComponent* PatternAttack = ConfigureFallbackAttack(PatternBoss);
	if (!TestNotNull(TEXT("Front attack component is available"), FrontAttack)
		|| !TestNotNull(TEXT("Rear attack component is available"), RearAttack)
		|| !TestNotNull(TEXT("Side attack component is available"), SideAttack)
		|| !TestNotNull(TEXT("Occluded attack component is available"), OccludedAttack)
		|| !TestNotNull(TEXT("Cancel attack component is available"), CancelAttack)
		|| !TestNotNull(TEXT("Destroy attack component is available"), DestroyAttack)
		|| !TestNotNull(TEXT("Pattern attack component is available"), PatternAttack))
	{
		return false;
	}
	UAnimMontage* PatternMontage = NewObject<UAnimMontage>(
		GetTransientPackage(),
		TEXT("AttackTracePatternMontage"));
	FAIREEnemyAttackPattern Pattern;
	Pattern.PatternId = TEXT("AutomationPattern");
	Pattern.Montage = PatternMontage;
	Pattern.MinRange = 0.0f;
	Pattern.MaxRange = 250.0f;
	Pattern.MinPlayRate = 1.35f;
	Pattern.MaxPlayRate = 1.35f;
	Pattern.DamageScale = 0.8f;
	Pattern.ForwardMoveDistance = 60.0f;
	TArray<FAIREEnemyAttackPattern> Patterns;
	Patterns.Add(Pattern);
	PatternAttack->ConfigureAttackPatterns(Patterns);

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
	TestTrue(TEXT("Data-driven pattern attack starts"),
		PatternAttack->TryStartMeleeAttack(PatternTarget));
	TestEqual(TEXT("Selected attack pattern is exposed in the snapshot"),
		PatternAttack->GetAttackSnapshot().PatternId,
		FName(TEXT("AutomationPattern")));
	TestTrue(TEXT("Selected attack play rate is snapshotted"),
		FMath::IsNearlyEqual(
			PatternAttack->GetAttackSnapshot().PlayRate,
			1.35f));
	TestTrue(TEXT("Gap-closer movement is exposed in the snapshot"),
		PatternAttack->GetAttackSnapshot().bGapCloser);

	RearTarget->SetActorLocation(FVector(-150.0f, 1000.0f, 0.0f));
	SideTarget->SetActorLocation(FVector(0.0f, 2150.0f, 0.0f));

	CancelAttack->CancelCurrentAttack();
	DestroyTarget->Destroy();
	TestFalse(TEXT("Target destruction closes the active attack immediately"),
		DestroyAttack->GetAttackSnapshot().bActive);

	TestTrue(TEXT("Forward fallback resolves through the spatial wrapper"),
		FrontAttack->CommitActiveMeleeHit());
	TestFalse(TEXT("Rear fallback misses through the spatial wrapper"),
		RearAttack->CommitActiveMeleeHit());
	TestFalse(TEXT("Side fallback misses through the spatial wrapper"),
		SideAttack->CommitActiveMeleeHit());
	TestFalse(TEXT("Occluded fallback is blocked through the spatial wrapper"),
		OccludedAttack->CommitActiveMeleeHit());
	TestTrue(TEXT("Pattern fallback resolves through the spatial wrapper"),
		PatternAttack->CommitActiveMeleeHit());
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
	TestTrue(TEXT("Pattern damage scale applies to the fallback strike"),
		FMath::IsNearlyEqual(GetHealth(PatternTarget), TestTargetHealth - 20.0f));

	TestFalse(TEXT("Repeated resolution cannot apply the same strike twice"),
		FrontAttack->CommitActiveMeleeHit());
	TestTrue(TEXT("Repeated resolution preserves the first hit's health result"),
		FMath::IsNearlyEqual(GetHealth(FrontTarget), TestTargetHealth - 25.0f));

	const FGuid FrontExecutionId =
		FrontAttack->GetAttackSnapshot().ExecutionId;
	FrontAttack->BeginMeleeTraceWindow(
		FrontExecutionId,
		1,
		0.5f,
		1.0f);
	FrontAttack->UpdateMeleeTraceWindow(FrontExecutionId, 1);
	TestTrue(TEXT("A distinct strike in the same attack applies scaled damage"),
		FMath::IsNearlyEqual(
			GetHealth(FrontTarget),
			TestTargetHealth - 37.5f));
	TestEqual(
		TEXT("The attack records both committed strike indices"),
		FrontAttack->GetAttackSnapshot().CommittedStrikeCount,
		2);
	FrontAttack->BeginMeleeTraceWindow(FrontExecutionId, 1, 0.5f, 1.0f);
	FrontAttack->UpdateMeleeTraceWindow(FrontExecutionId, 1);
	TestTrue(TEXT("A repeated callback for the second strike is exact-once"),
		FMath::IsNearlyEqual(
			GetHealth(FrontTarget),
			TestTargetHealth - 37.5f));
	TestFalse(TEXT("Aggro-swap cancellation stays closed after first contact"),
		FrontAttack->TryCancelDamageForAggroSwap(FrontExecutionId));

	PatternAttack->CancelCurrentAttack();
	UAnimMontage* AlternatePatternMontage = NewObject<UAnimMontage>(
		GetTransientPackage(),
		TEXT("AttackTraceAlternatePatternMontage"));
	FAIREEnemyAttackPattern AlternatePattern = Pattern;
	Pattern.PatternId = TEXT("AutomationPatternA");
	Pattern.Montage = PatternMontage;
	Pattern.ReuseCooldown = 100.0f;
	Pattern.ForwardMoveDistance = 0.0f;
	AlternatePattern.PatternId = TEXT("AutomationPatternB");
	AlternatePattern.Montage = AlternatePatternMontage;
	AlternatePattern.ReuseCooldown = 100.0f;
	AlternatePattern.ForwardMoveDistance = 0.0f;
	Patterns.Reset();
	Patterns.Add(Pattern);
	Patterns.Add(AlternatePattern);
	PatternAttack->ConfigureAttackPatterns(Patterns);
	TestTrue(TEXT("First attack with two eligible patterns starts"),
		PatternAttack->TryStartMeleeAttack(PatternTarget));
	const FName FirstPatternId =
		PatternAttack->GetAttackSnapshot().PatternId;
	PatternAttack->CancelCurrentAttack();
	TestTrue(TEXT("Second attack with two eligible patterns starts"),
		PatternAttack->TryStartMeleeAttack(PatternTarget));
	const FName SecondPatternId =
		PatternAttack->GetAttackSnapshot().PatternId;
	TestTrue(TEXT("The immediately previous pattern is not selected again"),
		FirstPatternId != SecondPatternId);
	PatternAttack->CancelCurrentAttack();
	TestTrue(TEXT("Base fallback remains available while patterns are locked"),
		PatternAttack->TryStartMeleeAttack(PatternTarget));
	TestTrue(TEXT("Pattern reuse cooldown locks both selected patterns"),
		PatternAttack->GetAttackSnapshot().PatternId.IsNone());
	PatternAttack->CancelCurrentAttack();

	PatternAttack->ConfigureDefaults(
		150.0f,
		25.0f,
		0.0f,
		0.0f,
		0.01f,
		0.2f,
		TraceSettings);
	FAIREEnemyAttackPattern GapCloserPattern = Pattern;
	GapCloserPattern.PatternId = TEXT("AutomationGapCloser");
	GapCloserPattern.MinRange = 200.0f;
	GapCloserPattern.MaxRange = 250.0f;
	GapCloserPattern.ForwardMoveDistance = 60.0f;
	GapCloserPattern.ReuseCooldown = 0.0f;
	FAIREEnemyAttackPattern MeleeFollowUpPattern = AlternatePattern;
	MeleeFollowUpPattern.PatternId = TEXT("AutomationMeleeFollowUp");
	MeleeFollowUpPattern.MinRange = 0.0f;
	MeleeFollowUpPattern.MaxRange = 150.0f;
	MeleeFollowUpPattern.ForwardMoveDistance = 0.0f;
	MeleeFollowUpPattern.ReuseCooldown = 0.0f;
	Patterns.Reset();
	Patterns.Add(GapCloserPattern);
	Patterns.Add(MeleeFollowUpPattern);
	PatternAttack->ConfigureAttackPatterns(Patterns);
	PatternTarget->SetActorLocation(FVector(300.0f, 6000.0f, 0.0f));
	TestTrue(TEXT("Gap closer starts in its outer range"),
		PatternAttack->TryStartMeleeAttack(PatternTarget));
	TestEqual(TEXT("Outer-range attack selects the gap closer"),
		PatternAttack->GetAttackSnapshot().PatternId,
		GapCloserPattern.PatternId);
	PatternAttack->CancelCurrentAttack();
	TestTrue(TEXT("Gap closer requires one non-gap follow-up"),
		PatternAttack->RequiresNonGapCloserFollowUp());
	TestFalse(TEXT("Gap closer cannot repeat before a melee follow-up"),
		PatternAttack->TryStartMeleeAttack(PatternTarget));
	PatternTarget->SetActorLocation(FVector(150.0f, 6000.0f, 0.0f));
	TestTrue(TEXT("Non-gap follow-up starts after closing distance"),
		PatternAttack->TryStartMeleeAttack(PatternTarget));
	TestEqual(TEXT("The required follow-up selects the melee pattern"),
		PatternAttack->GetAttackSnapshot().PatternId,
		MeleeFollowUpPattern.PatternId);
	TestFalse(TEXT("Starting the melee follow-up clears the requirement"),
		PatternAttack->RequiresNonGapCloserFollowUp());
	PatternAttack->CancelCurrentAttack();
	PatternTarget->SetActorLocation(FVector(300.0f, 6000.0f, 0.0f));
	TestTrue(TEXT("Gap closer becomes eligible after the melee follow-up"),
		PatternAttack->TryStartMeleeAttack(PatternTarget));
	PatternAttack->CancelCurrentAttack();

	return true;
}

#endif

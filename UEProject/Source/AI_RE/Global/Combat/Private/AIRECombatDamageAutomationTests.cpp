#if WITH_DEV_AUTOMATION_TESTS

#include "AIRECombatDamageSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AI_REAttributeSet.h"
#include "AIREBossEnemy.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "Testing/AIRECompanionCombatTestTarget.h"
#include "AIREEnemyReactionAttributeSet.h"
#include "AIREEnemyReactionComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECombatDamagePipelineTest,
	"AIRE.Combat.Damage.SharedPipeline",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECombatDamagePipelineTest::RunTest(const FString& Parameters)
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
	if (!TestNotNull(TEXT("Transient combat world is created"), TestWorld))
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
	AAIRECompanionCombatTestTarget* PartySource =
		TestWorld->SpawnActor<AAIRECompanionCombatTestTarget>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
	AAIREBossEnemy* FirstEnemy = TestWorld->SpawnActor<AAIREBossEnemy>(
		FVector(500.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	AAIREBossEnemy* SecondEnemy = TestWorld->SpawnActor<AAIREBossEnemy>(
		FVector(1000.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!TestNotNull(TEXT("Party source is spawned"), PartySource)
		|| !TestNotNull(TEXT("First enemy is spawned"), FirstEnemy)
		|| !TestNotNull(TEXT("Second enemy is spawned"), SecondEnemy))
	{
		return false;
	}
	PartySource->SetHostileForTesting(false);
	TestWorld->BeginPlay();

	UAIRECombatDamageSubsystem* DamageSubsystem =
		TestWorld->GetSubsystem<UAIRECombatDamageSubsystem>();
	UAbilitySystemComponent* FirstEnemyAbilitySystem =
		FirstEnemy->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Damage subsystem is available"), DamageSubsystem)
		|| !TestNotNull(
			TEXT("Enemy ability system is available"),
			FirstEnemyAbilitySystem))
	{
		return false;
	}

	FAIRECombatDamageRequest Request;
	Request.Source = PartySource;
	Request.Target = FirstEnemy;
	Request.Damage = 10.0f;
	Request.StaggerValue = 25.0f;
	Request.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("Opposing affiliation request applies"),
		DamageSubsystem->ApplyDamageRequest(Request),
		EAIRECombatDamageResult::Applied);
	TestTrue(
		TEXT("Damage modifies the target-owned health attribute"),
		FMath::IsNearlyEqual(
			FirstEnemyAbilitySystem->GetNumericAttribute(
				UAI_REAttributeSet::GetHPAttribute()),
			490.0f));
	TestTrue(
		TEXT("Stagger modifies the enemy flinch gauge"),
		FMath::IsNearlyEqual(
			FirstEnemyAbilitySystem->GetNumericAttribute(
				UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute()),
			25.0f));

	TestEqual(
		TEXT("A duplicate target/execution pair is rejected"),
		DamageSubsystem->ApplyDamageRequest(Request),
		EAIRECombatDamageResult::DuplicateExecution);
	TestTrue(
		TEXT("Duplicate rejection preserves health"),
		FMath::IsNearlyEqual(
			FirstEnemyAbilitySystem->GetNumericAttribute(
				UAI_REAttributeSet::GetHPAttribute()),
			490.0f));

	Request.Target = SecondEnemy;
	TestEqual(
		TEXT("The same execution ID may apply once to another target"),
		DamageSubsystem->ApplyDamageRequest(Request),
		EAIRECombatDamageResult::Applied);

	const float PartyHealthBeforeSelfRequest =
		PartySource->GetAbilitySystemComponent()->GetNumericAttribute(
			UAIRECompanionAttributeSet::GetHealthAttribute());
	FAIRECombatDamageRequest SelfRequest;
	SelfRequest.Source = PartySource;
	SelfRequest.Target = PartySource;
	SelfRequest.Damage = 10.0f;
	SelfRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("Self damage is rejected"),
		DamageSubsystem->ApplyDamageRequest(SelfRequest),
		EAIRECombatDamageResult::SelfTarget);
	TestTrue(
		TEXT("Self damage rejection preserves health"),
		FMath::IsNearlyEqual(
			PartySource->GetAbilitySystemComponent()->GetNumericAttribute(
				UAIRECompanionAttributeSet::GetHealthAttribute()),
			PartyHealthBeforeSelfRequest));

	FAIRECombatDamageRequest FlinchRequest;
	FlinchRequest.Source = PartySource;
	FlinchRequest.Target = FirstEnemy;
	FlinchRequest.StaggerValue = 25.0f;
	FlinchRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("The flinch-threshold stagger request applies"),
		DamageSubsystem->ApplyDamageRequest(FlinchRequest),
		EAIRECombatDamageResult::Applied);
	TestWorld->Tick(LEVELTICK_All, 0.01f);
	TestEqual(
		TEXT("Flinch starts at 50 accumulated stagger"),
		FirstEnemy->GetEnemyReactionComponent()->GetReactionSnapshot().State,
		EAIREEnemyReactionState::Flinching);

	for (int32 StaggerIndex = 0; StaggerIndex < 7; ++StaggerIndex)
	{
		FAIRECombatDamageRequest StunBuildRequest;
		StunBuildRequest.Source = PartySource;
		StunBuildRequest.Target = SecondEnemy;
		StunBuildRequest.StaggerValue = 25.0f;
		StunBuildRequest.ExecutionId = FGuid::NewGuid();
		TestEqual(
			*FString::Printf(
				TEXT("Stun build request %d applies"),
				StaggerIndex),
			DamageSubsystem->ApplyDamageRequest(StunBuildRequest),
			EAIRECombatDamageResult::Applied);
	}
	TestWorld->Tick(LEVELTICK_All, 0.01f);
	TestEqual(
		TEXT("Stun takes priority when both gauges cross on one evaluation"),
		SecondEnemy->GetEnemyReactionComponent()->GetReactionSnapshot().State,
		EAIREEnemyReactionState::Stunned);
	UAbilitySystemComponent* SecondEnemyAbilitySystem =
		SecondEnemy->GetAbilitySystemComponent();
	const float StunGaugeBeforeIgnoredStagger =
		SecondEnemyAbilitySystem->GetNumericAttribute(
			UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute());
	const float FlinchGaugeBeforeIgnoredStagger =
		SecondEnemyAbilitySystem->GetNumericAttribute(
			UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute());
	FAIRECombatDamageRequest StunnedDamageRequest;
	StunnedDamageRequest.Source = PartySource;
	StunnedDamageRequest.Target = SecondEnemy;
	StunnedDamageRequest.Damage = 1.0f;
	StunnedDamageRequest.StaggerValue = 25.0f;
	StunnedDamageRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("Damage still applies while stunned"),
		DamageSubsystem->ApplyDamageRequest(StunnedDamageRequest),
		EAIRECombatDamageResult::Applied);
	TestTrue(
		TEXT("A stunned target ignores additional flinch stagger"),
		FMath::IsNearlyEqual(
			SecondEnemyAbilitySystem->GetNumericAttribute(
				UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute()),
			FlinchGaugeBeforeIgnoredStagger));
	TestTrue(
		TEXT("A stunned target ignores additional stun stagger"),
		FMath::IsNearlyEqual(
			SecondEnemyAbilitySystem->GetNumericAttribute(
				UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute()),
			StunGaugeBeforeIgnoredStagger));

	FAIRECombatDamageRequest FriendlyRequest;
	FriendlyRequest.Source = FirstEnemy;
	FriendlyRequest.Target = SecondEnemy;
	FriendlyRequest.Damage = 10.0f;
	FriendlyRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("Same-affiliation damage is rejected"),
		DamageSubsystem->ApplyDamageRequest(FriendlyRequest),
		EAIRECombatDamageResult::UnsupportedTarget);
	const float FirstHealthBeforeInvalidRequests =
		FirstEnemyAbilitySystem->GetNumericAttribute(
			UAI_REAttributeSet::GetHPAttribute());

	FAIRECombatDamageRequest MissingExecutionRequest;
	MissingExecutionRequest.Source = PartySource;
	MissingExecutionRequest.Target = FirstEnemy;
	MissingExecutionRequest.Damage = 1.0f;
	TestEqual(
		TEXT("A missing execution ID is rejected"),
		DamageSubsystem->ApplyDamageRequest(MissingExecutionRequest),
		EAIRECombatDamageResult::InvalidExecutionId);

	FAIRECombatDamageRequest NegativeMagnitudeRequest;
	NegativeMagnitudeRequest.Source = PartySource;
	NegativeMagnitudeRequest.Target = FirstEnemy;
	NegativeMagnitudeRequest.Damage = -1.0f;
	NegativeMagnitudeRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("A negative magnitude is rejected"),
		DamageSubsystem->ApplyDamageRequest(NegativeMagnitudeRequest),
		EAIRECombatDamageResult::InvalidMagnitude);

	FAIRECombatDamageRequest NonFiniteMagnitudeRequest;
	NonFiniteMagnitudeRequest.Source = PartySource;
	NonFiniteMagnitudeRequest.Target = FirstEnemy;
	NonFiniteMagnitudeRequest.Damage =
		std::numeric_limits<float>::quiet_NaN();
	NonFiniteMagnitudeRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("A non-finite magnitude is rejected"),
		DamageSubsystem->ApplyDamageRequest(NonFiniteMagnitudeRequest),
		EAIRECombatDamageResult::InvalidMagnitude);

	FAIRECombatDamageRequest InvalidMagnitudeRequest;
	InvalidMagnitudeRequest.Source = PartySource;
	InvalidMagnitudeRequest.Target = FirstEnemy;
	InvalidMagnitudeRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("A zero damage and zero stagger request is rejected"),
		DamageSubsystem->ApplyDamageRequest(InvalidMagnitudeRequest),
		EAIRECombatDamageResult::InvalidMagnitude);
	TestTrue(
		TEXT("All invalid magnitude/ID requests preserve target health"),
		FMath::IsNearlyEqual(
			FirstEnemyAbilitySystem->GetNumericAttribute(
				UAI_REAttributeSet::GetHPAttribute()),
			FirstHealthBeforeInvalidRequests));

	FAIRECombatDamageRequest KillRequest;
	KillRequest.Source = PartySource;
	KillRequest.Target = SecondEnemy;
	KillRequest.Damage = 1000.0f;
	KillRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("Lethal damage applies once"),
		DamageSubsystem->ApplyDamageRequest(KillRequest),
		EAIRECombatDamageResult::Applied);
	const float DeadTargetHealth = SecondEnemyAbilitySystem->GetNumericAttribute(
		UAI_REAttributeSet::GetHPAttribute());
	FAIRECombatDamageRequest DeadTargetRequest = KillRequest;
	DeadTargetRequest.Damage = 1.0f;
	DeadTargetRequest.ExecutionId = FGuid::NewGuid();
	TestEqual(
		TEXT("A dead target rejects later damage"),
		DamageSubsystem->ApplyDamageRequest(DeadTargetRequest),
		EAIRECombatDamageResult::TargetDead);
	TestTrue(
		TEXT("Dead-target rejection preserves zero health"),
		FMath::IsNearlyEqual(
			SecondEnemyAbilitySystem->GetNumericAttribute(
				UAI_REAttributeSet::GetHPAttribute()),
			DeadTargetHealth));

	FAIRECombatDamageRequest DeadSourceRequest;
	DeadSourceRequest.Source = SecondEnemy;
	DeadSourceRequest.Target = PartySource;
	DeadSourceRequest.Damage = 1.0f;
	DeadSourceRequest.ExecutionId = FGuid::NewGuid();
	const float PartyHealthBeforeDeadSourceRequest =
		PartySource->GetAbilitySystemComponent()->GetNumericAttribute(
			UAIRECompanionAttributeSet::GetHealthAttribute());
	TestEqual(
		TEXT("A dead source cannot deliver a late hit"),
		DamageSubsystem->ApplyDamageRequest(DeadSourceRequest),
		EAIRECombatDamageResult::SourceDead);
	TestTrue(
		TEXT("Dead-source rejection preserves target health"),
		FMath::IsNearlyEqual(
			PartySource->GetAbilitySystemComponent()->GetNumericAttribute(
				UAIRECompanionAttributeSet::GetHealthAttribute()),
			PartyHealthBeforeDeadSourceRequest));

	return true;
}

#endif

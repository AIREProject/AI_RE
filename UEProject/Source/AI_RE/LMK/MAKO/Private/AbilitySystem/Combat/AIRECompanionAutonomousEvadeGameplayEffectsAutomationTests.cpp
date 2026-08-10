#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Combat/Effects/AIRECompanionAutonomousEvadeGameplayEffects.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "AIRECombatGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Testing/AIRECompanionCombatTestTarget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECompanionAutonomousEvadeGameplayEffectsTest,
	"AIRE.Companion.Combat.AutonomousEvade.GameplayEffects",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECompanionAutonomousEvadeGameplayEffectsTest::RunTest(
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
	AAIRECompanionCombatTestTarget* Target =
		TestWorld->SpawnActor<AAIRECompanionCombatTestTarget>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!TestNotNull(TEXT("ASC test target is spawned"), Target))
	{
		return false;
	}
	TestWorld->BeginPlay();
	TestWorld->GetWorldSettings()->NotifyBeginPlay();
	UAbilitySystemComponent* AbilitySystem =
		Target->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Target ability system is available"), AbilitySystem))
	{
		return false;
	}

	AbilitySystem->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetMaxStaminaAttribute(),
		100.0f);
	AbilitySystem->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetStaminaAttribute(),
		25.0f);
	const auto ApplyEffect = [AbilitySystem](
		const TSubclassOf<UGameplayEffect> EffectClass,
		const FGameplayTag DataTag,
		const float Magnitude)
	{
		FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
			EffectClass,
			1.0f,
			AbilitySystem->MakeEffectContext());
		if (!Spec.IsValid())
		{
			return FActiveGameplayEffectHandle();
		}
		Spec.Data->SetSetByCallerMagnitude(DataTag, Magnitude);
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	};
	const auto TickWorld = [TestWorld](const float Duration)
	{
		constexpr float TickInterval = 0.05f;
		const int32 TickCount = FMath::CeilToInt(Duration / TickInterval);
		for (int32 TickIndex = 0; TickIndex < TickCount; ++TickIndex)
		{
			TestWorld->Tick(LEVELTICK_All, TickInterval);
		}
	};

	const FActiveGameplayEffectHandle RegenHandle = ApplyEffect(
		UAIRECompanionStaminaRegenGameplayEffect::StaticClass(),
		AIRECompanionGameplayTags::DataStaminaRegenPerTick,
		1.5f);
	TestTrue(
		TEXT("Persistent stamina regeneration applies"),
		RegenHandle.WasSuccessfullyApplied());
	const FActiveGameplayEffectHandle CooldownHandle = ApplyEffect(
		UAIRECompanionAutonomousEvadeCooldownGameplayEffect::StaticClass(),
		AIRECompanionGameplayTags::DataAutonomousEvadeCooldownDuration,
		5.0f);
	const FActiveGameplayEffectHandle RegenBlockHandle = ApplyEffect(
		UAIRECompanionAutonomousEvadeRegenBlockGameplayEffect::StaticClass(),
		AIRECompanionGameplayTags::DataAutonomousEvadeRegenBlockDuration,
		1.5f);
	const FActiveGameplayEffectHandle CostHandle = ApplyEffect(
		UAIRECompanionAutonomousEvadeCostGameplayEffect::StaticClass(),
		AIRECompanionGameplayTags::DataAutonomousEvadeStaminaCost,
		-25.0f);
	TestTrue(
		TEXT("Autonomous evade cooldown applies"),
		CooldownHandle.WasSuccessfullyApplied());
	TestTrue(
		TEXT("Autonomous evade regeneration delay applies"),
		RegenBlockHandle.WasSuccessfullyApplied());
	TestTrue(
		TEXT("Autonomous evade stamina cost applies"),
		CostHandle.WasSuccessfullyApplied());
	TestTrue(
		TEXT("An exact 25 stamina balance is reduced to zero"),
		FMath::IsNearlyEqual(
			AbilitySystem->GetNumericAttribute(
				UAIRECompanionAttributeSet::GetStaminaAttribute()),
			0.0f));
	TestTrue(
		TEXT("Cooldown tag is active"),
		AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::CooldownAutonomousEvade));
	TestTrue(
		TEXT("Regeneration block tag is active"),
		AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateStaminaRegenBlocked));

	TickWorld(1.0f);
	TestTrue(
		TEXT("Stamina does not regenerate during the 1.5 second delay"),
		FMath::IsNearlyEqual(
			AbilitySystem->GetNumericAttribute(
				UAIRECompanionAttributeSet::GetStaminaAttribute()),
			0.0f));
	TickWorld(0.65f);
	TestFalse(
		TEXT("Regeneration block expires after 1.5 seconds"),
		AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateStaminaRegenBlocked));
	const float StaminaAfterResume = AbilitySystem->GetNumericAttribute(
		UAIRECompanionAttributeSet::GetStaminaAttribute());
	TestTrue(
		TEXT("Stamina regeneration resumes after the delay"),
		StaminaAfterResume > 0.0f);
	TickWorld(1.0f);
	const float OneSecondRegen =
		AbilitySystem->GetNumericAttribute(
			UAIRECompanionAttributeSet::GetStaminaAttribute())
		- StaminaAfterResume;
	TestTrue(
		TEXT("Periodic regeneration is approximately 15 stamina per second"),
		OneSecondRegen >= 13.5f && OneSecondRegen <= 16.5f);

	AbilitySystem->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetStaminaAttribute(),
		99.5f);
	TickWorld(0.2f);
	TestTrue(
		TEXT("Periodic regeneration clamps to MaxStamina"),
		FMath::IsNearlyEqual(
			AbilitySystem->GetNumericAttribute(
				UAIRECompanionAttributeSet::GetStaminaAttribute()),
			100.0f));

	const FActiveGameplayEffectHandle InvulnerabilityHandle = ApplyEffect(
		UAIRECompanionAutonomousEvadeInvulnerabilityGameplayEffect::StaticClass(),
		AIRECompanionGameplayTags::DataAutonomousEvadeInvulnerabilityDuration,
		0.12f);
	TestTrue(
		TEXT("Short invulnerability effect applies"),
		InvulnerabilityHandle.WasSuccessfullyApplied());
	TestTrue(
		TEXT("Short invulnerability grants the shared combat tag"),
		AbilitySystem->HasMatchingGameplayTag(
			AIRECombatGameplayTags::StateInvulnerable));
	TickWorld(0.15f);
	TestFalse(
		TEXT("Short invulnerability expires after 0.12 seconds"),
		AbilitySystem->HasMatchingGameplayTag(
			AIRECombatGameplayTags::StateInvulnerable));

	TickWorld(2.5f);
	TestFalse(
		TEXT("Autonomous evade cooldown expires after five seconds"),
		AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::CooldownAutonomousEvade));
	AbilitySystem->RemoveActiveGameplayEffect(RegenHandle);
	return true;
}

#endif

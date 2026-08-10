#if WITH_DEV_AUTOMATION_TESTS

#include "LocalAI/Combat/AIRECompanionAutonomousEvadePolicy.h"

#include "Core/AIRECompanionConfigDataAsset.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECompanionAutonomousEvadePolicyTest,
	"AIRE.Companion.Combat.AutonomousEvade.Policy",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECompanionAutonomousEvadePolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FGuid ExecutionId(1, 2, 3, 4);
	const FAIRECompanionAutonomousEvadeDecision First =
		FAIRECompanionAutonomousEvadePolicy::Evaluate(
			ExecutionId,
			0.5f,
			0.15f,
			0.28f);
	const FAIRECompanionAutonomousEvadeDecision Second =
		FAIRECompanionAutonomousEvadePolicy::Evaluate(
			ExecutionId,
			0.5f,
			0.15f,
			0.28f);
	TestEqual(
		TEXT("The same execution produces the same selection"),
		First.bSelected,
		Second.bSelected);
	TestTrue(
		TEXT("The same execution produces the same reaction delay"),
		FMath::IsNearlyEqual(First.ReactionDelay, Second.ReactionDelay));

	const FAIRECompanionAutonomousEvadeDecision Always =
		FAIRECompanionAutonomousEvadePolicy::Evaluate(
			ExecutionId,
			1.0f,
			0.15f,
			0.28f);
	TestTrue(TEXT("A one-hundred-percent decision is selected"), Always.bSelected);
	TestTrue(
		TEXT("Selected reaction delay stays inside the configured interval"),
		Always.ReactionDelay >= 0.15f && Always.ReactionDelay <= 0.28f);

	const FAIRECompanionAutonomousEvadeDecision Never =
		FAIRECompanionAutonomousEvadePolicy::Evaluate(
			ExecutionId,
			0.0f,
			0.15f,
			0.28f);
	TestFalse(TEXT("A zero-percent decision is not selected"), Never.bSelected);
	TestEqual(TEXT("A rejected decision has no delay"), Never.ReactionDelay, 0.0f);

	const FAIRECompanionAutonomousEvadeDecision Invalid =
		FAIRECompanionAutonomousEvadePolicy::Evaluate(
			FGuid(),
			1.0f,
			0.15f,
			0.28f);
	TestFalse(TEXT("An invalid execution is rejected"), Invalid.bSelected);

	int32 SelectedCount = 0;
	for (int32 Index = 1; Index <= 256; ++Index)
	{
		const FGuid SampleExecution(
			Index,
			Index * 17 + 3,
			Index * 31 + 7,
			Index * 47 + 11);
		SelectedCount +=
			FAIRECompanionAutonomousEvadePolicy::Evaluate(
				SampleExecution,
				0.5f,
				0.15f,
				0.28f).bSelected
			? 1
			: 0;
	}
	TestTrue(
		TEXT("The fixed execution sample exercises both halves of the fifty-percent policy"),
		SelectedCount >= 90 && SelectedCount <= 166);

	FAIRECompanionAutonomousEvadeSettings Settings;
	TestTrue(TEXT("Default autonomous evade settings are valid"), Settings.IsValid());
	TestEqual(TEXT("Default autonomous evade stamina cost is 25"), Settings.StaminaCost, 25.0f);
	TestEqual(TEXT("Default autonomous evade cooldown is five seconds"), Settings.CooldownDuration, 5.0f);
	TestEqual(TEXT("Default autonomous evade minimum clearance is 100 cm"), Settings.MinimumClearance, 100.0f);
	Settings.ReactionDelayMax = 0.1f;
	TestFalse(
		TEXT("An inverted autonomous reaction interval is rejected"),
		Settings.IsValid());

	UAIRECompanionConfigDataAsset* Config =
		NewObject<UAIRECompanionConfigDataAsset>();
	FText ValidationError;
	TestTrue(
		TEXT("The default companion config accepts autonomous evade"),
		Config->IsConfigurationValid(ValidationError));
	Config->InitialStamina = 24.99f;
	Config->MaxStamina = 24.99f;
	TestFalse(
		TEXT("Autonomous evade cost cannot exceed MaxStamina"),
		Config->IsConfigurationValid(ValidationError));
	return true;
}

#endif

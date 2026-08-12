#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI_REAbilitySetDataAsset.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECompanionWeaponTraceDefinitionTest,
	"AIRE.Companion.Combat.WeaponTraceDefinition",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECompanionWeaponTraceDefinitionTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition =
		NewObject<UAIRECompanionWeaponDefinitionDataAsset>();
	UAI_REAbilitySetDataAsset* AbilitySet =
		NewObject<UAI_REAbilitySetDataAsset>(WeaponDefinition);
	WeaponDefinition->WeaponTag =
		AIRECompanionGameplayTags::WeaponCompanionMeleeBasic;
	WeaponDefinition->AbilitySet = AbilitySet;

	TestEqual(
		TEXT("Default melee trace radius is 25 cm"),
		WeaponDefinition->TraceRadius,
		25.0f);
	TestEqual(
		TEXT("Default MAKO capsule radius is 35 cm"),
		WeaponDefinition->TraceCapsuleRadius,
		35.0f);
	TestEqual(
		TEXT("Default MAKO capsule half height is 160 cm"),
		WeaponDefinition->TraceCapsuleHalfHeight,
		160.0f);
	TestEqual(
		TEXT("Harvest point-hit range is 75 cm"),
		WeaponDefinition->HarvestAttackRange,
		75.0f);
	TestEqual(
		TEXT("Left blade uses the project-owned handle socket"),
		WeaponDefinition->LeftTraceSockets.TraceStartSocket,
		FName(TEXT("weapon_l")));
	TestEqual(
		TEXT("Right blade uses the project-owned tip socket"),
		WeaponDefinition->RightTraceSockets.TraceEndSocket,
		FName(TEXT("weapon_trace_tip_r")));
	TestTrue(
		TEXT("Basic trace Begin routes through the basic trace root"),
		AIRECompanionGameplayTags::EventAttackTraceBegin.GetTag().MatchesTag(
			AIRECompanionGameplayTags::EventAttackTrace.GetTag()));
	TestTrue(
		TEXT("Basic trace Sample routes through the basic trace root"),
		AIRECompanionGameplayTags::EventAttackTraceSample.GetTag().MatchesTag(
			AIRECompanionGameplayTags::EventAttackTrace.GetTag()));
	TestTrue(
		TEXT("Basic trace End routes through the basic trace root"),
		AIRECompanionGameplayTags::EventAttackTraceEnd.GetTag().MatchesTag(
			AIRECompanionGameplayTags::EventAttackTrace.GetTag()));
	TestTrue(
		TEXT("Skill trace Begin routes through the skill trace root"),
		AIRECompanionGameplayTags::EventCombatSkillTraceBegin.GetTag().MatchesTag(
			AIRECompanionGameplayTags::EventCombatSkillTrace.GetTag()));
	TestTrue(
		TEXT("Skill trace Sample routes through the skill trace root"),
		AIRECompanionGameplayTags::EventCombatSkillTraceSample.GetTag().MatchesTag(
			AIRECompanionGameplayTags::EventCombatSkillTrace.GetTag()));
	TestTrue(
		TEXT("Skill trace End routes through the skill trace root"),
		AIRECompanionGameplayTags::EventCombatSkillTraceEnd.GetTag().MatchesTag(
			AIRECompanionGameplayTags::EventCombatSkillTrace.GetTag()));

	FText ValidationError;
	TestTrue(
		TEXT("Default trace contract is valid"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));

	WeaponDefinition->TraceRadius = 0.0f;
	TestFalse(
		TEXT("A zero trace radius is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	WeaponDefinition->TraceRadius = 25.0f;
	WeaponDefinition->TraceCapsuleHalfHeight = 30.0f;
	TestFalse(
		TEXT("A capsule shorter than its radius is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	WeaponDefinition->TraceCapsuleHalfHeight = 160.0f;
	WeaponDefinition->TraceChannel = ECC_MAX;
	TestFalse(
		TEXT("An invalid trace channel is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	WeaponDefinition->TraceChannel = ECC_Pawn;

	WeaponDefinition->HarvestAttackRange = -1.0f;
	TestFalse(
		TEXT("A negative harvest range is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	WeaponDefinition->HarvestAttackRange = 75.0f;

	FAIREWeaponComboStepDefinition& ComboStep =
		WeaponDefinition->ComboSteps.AddDefaulted_GetRef();
	ComboStep.MontageSection = FName(TEXT("Attack_01"));
	ComboStep.TraceSocketOverride.TraceStartSocket =
		FName(TEXT("weapon_l"));
	TestFalse(
		TEXT("A partial combo socket override is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));

	ComboStep.TraceSocketOverride.TraceEndSocket =
		FName(TEXT("weapon_trace_tip_l"));
	TestTrue(
		TEXT("A complete combo socket override is accepted"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));

	const FAIREWeaponTraceSocketPair ResolvedSockets =
		WeaponDefinition->ResolveTraceSockets(
			EAIRECompanionWeaponTraceSide::Right,
			ComboStep.TraceSocketOverride);
	TestEqual(
		TEXT("A configured combo pair overrides the selected default side"),
		ResolvedSockets.TraceEndSocket,
		ComboStep.TraceSocketOverride.TraceEndSocket);

	WeaponDefinition->CombatSkill.bEnabled = true;
	WeaponDefinition->CombatSkill.TraceSocketOverride.TraceStartSocket =
		FName(TEXT("weapon_r"));
	TestFalse(
		TEXT("A partial combat skill socket override is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	WeaponDefinition->CombatSkill.TraceSocketOverride.TraceEndSocket =
		FName(TEXT("weapon_trace_tip_r"));
	TestTrue(
		TEXT("A complete combat skill socket override is accepted"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));

	return true;
}

#endif

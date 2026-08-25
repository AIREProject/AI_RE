#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI_REAbilitySetDataAsset.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
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
		TEXT("Harvest range accepts the movement arrival tolerance"),
		AIRECompanionWeaponDefinition::HarvestRangeAcceptanceTolerance,
		25.0f);
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
	for (int32 StepIndex = 1; StepIndex < 4; ++StepIndex)
	{
		FAIREWeaponComboStepDefinition& AdditionalStep =
			WeaponDefinition->ComboSteps.AddDefaulted_GetRef();
		AdditionalStep.MontageSection = FName(
			*FString::Printf(TEXT("Attack_%02d"), StepIndex + 1));
	}

	FAIREWeaponComboMontageVariantDefinition& FirstVariant =
		WeaponDefinition->ComboMontageVariants.AddDefaulted_GetRef();
	TestFalse(
		TEXT("An empty combo variant is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	FirstVariant.MontageSections = {
		FName(TEXT("Combo01_Attack_01")),
		FName(TEXT("Combo01_Attack_02")),
		FName(TEXT("Combo01_Attack_03"))};
	TestTrue(
		TEXT("A shorter combo variant is accepted"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));

	FAIREWeaponComboMontageVariantDefinition& DuplicateVariant =
		WeaponDefinition->ComboMontageVariants.AddDefaulted_GetRef();
	DuplicateVariant.MontageSections = {
		FName(TEXT("Combo02_Attack_01")),
		FName(TEXT("Combo02_Attack_02")),
		FName(TEXT("Combo02_Attack_03")),
		FName(TEXT("Combo02_Attack_04"))};
	TestTrue(
		TEXT("A combo variant using every shared step is accepted"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	DuplicateVariant.MontageSections.Add(
		FName(TEXT("Combo02_Attack_05")));
	TestFalse(
		TEXT("A combo variant exceeding the shared step data is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	DuplicateVariant.MontageSections.Pop();
	DuplicateVariant.MontageSections[0] =
		FName(TEXT("Combo01_Attack_01"));
	TestFalse(
		TEXT("A duplicate combo variant section is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	DuplicateVariant.MontageSections[0] =
		FName(TEXT("Combo02_Attack_01"));

	int32 ResolvedVariantIndex = INDEX_NONE;
	int32 ResolvedStepIndex = INDEX_NONE;
	TestTrue(
		TEXT("A cumulative montage step resolves into a later variable-length variant"),
		AIRECompanionWeaponDefinition::ResolveComboVariantStepIndex(
			WeaponDefinition->ComboMontageVariants,
			3,
			ResolvedVariantIndex,
			ResolvedStepIndex));
	TestEqual(TEXT("The cumulative step resolves variant one"), ResolvedVariantIndex, 1);
	TestEqual(TEXT("The cumulative step resets its local index"), ResolvedStepIndex, 0);
	TestTrue(
		TEXT("The final cumulative montage step is valid"),
		AIRECompanionWeaponDefinition::ResolveComboVariantStepIndex(
			WeaponDefinition->ComboMontageVariants,
			6,
			ResolvedVariantIndex,
			ResolvedStepIndex));
	TestEqual(TEXT("The final cumulative step stays in variant one"), ResolvedVariantIndex, 1);
	TestEqual(TEXT("The final cumulative step resolves local index three"), ResolvedStepIndex, 3);
	TestFalse(
		TEXT("A montage step past all variable-length variants is rejected"),
		AIRECompanionWeaponDefinition::ResolveComboVariantStepIndex(
			WeaponDefinition->ComboMontageVariants,
			7,
			ResolvedVariantIndex,
			ResolvedStepIndex));
	TestTrue(
		TEXT("Unique combo variant sections are accepted"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	DuplicateVariant.MontageSections[0] = NAME_None;
	TestFalse(
		TEXT("An empty combo variant section is rejected"),
		WeaponDefinition->IsWeaponDefinitionValid(ValidationError));
	DuplicateVariant.MontageSections[0] =
		FName(TEXT("Combo02_Attack_01"));

	FRandomStream VariantRandomStream(1337);
	const int32 FirstSelectedVariantIndex =
		AIRECompanionWeaponDefinition::
			SelectNonRepeatingComboVariantIndex(
				3,
				INDEX_NONE,
				VariantRandomStream);
	TestTrue(
		TEXT("A first combo variant selection can use the complete valid range"),
		FirstSelectedVariantIndex >= 0 && FirstSelectedVariantIndex < 3);
	int32 PreviousVariantIndex = 0;
	TSet<int32> ObservedVariantIndices;
	for (int32 SelectionIndex = 0; SelectionIndex < 100; ++SelectionIndex)
	{
		const int32 SelectedVariantIndex =
			AIRECompanionWeaponDefinition::
				SelectNonRepeatingComboVariantIndex(
					3,
					PreviousVariantIndex,
					VariantRandomStream);
		TestTrue(
			TEXT("A selected combo variant index is valid"),
			SelectedVariantIndex >= 0 && SelectedVariantIndex < 3);
		TestNotEqual(
			TEXT("A selected combo variant does not repeat immediately"),
			SelectedVariantIndex,
			PreviousVariantIndex);
		ObservedVariantIndices.Add(SelectedVariantIndex);
		PreviousVariantIndex = SelectedVariantIndex;
	}
	TestEqual(
		TEXT("Seeded selection can reach every combo variant"),
		ObservedVariantIndices.Num(),
		3);
	TestEqual(
		TEXT("An empty combo variant set has no selection"),
		AIRECompanionWeaponDefinition::
			SelectNonRepeatingComboVariantIndex(
				0,
				INDEX_NONE,
				VariantRandomStream),
		INDEX_NONE);
	TestEqual(
		TEXT("A single combo variant selects index zero"),
		AIRECompanionWeaponDefinition::
			SelectNonRepeatingComboVariantIndex(
				1,
				0,
				VariantRandomStream),
		0);

	UAIRECompanionEquipmentComponent* EquipmentComponent =
		NewObject<UAIRECompanionEquipmentComponent>();
	EquipmentComponent->SetLastBasicComboVariantIndex(WeaponDefinition, 2);
	TestEqual(
		TEXT("The equipment component retains the last variant across ability removal"),
		EquipmentComponent->GetLastBasicComboVariantIndex(WeaponDefinition),
		2);
	UAIRECompanionWeaponDefinitionDataAsset* OtherWeaponDefinition =
		NewObject<UAIRECompanionWeaponDefinitionDataAsset>();
	TestEqual(
		TEXT("The retained variant is scoped to its weapon definition"),
		EquipmentComponent->GetLastBasicComboVariantIndex(
			OtherWeaponDefinition),
		INDEX_NONE);

	const FAIREWeaponTraceSocketPair ResolvedSockets =
		WeaponDefinition->ResolveTraceSockets(
			EAIRECompanionWeaponTraceSide::Right,
			WeaponDefinition->ComboSteps[0].TraceSocketOverride);
	TestEqual(
		TEXT("A configured combo pair overrides the selected default side"),
		ResolvedSockets.TraceEndSocket,
		WeaponDefinition->ComboSteps[0].TraceSocketOverride.TraceEndSocket);

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

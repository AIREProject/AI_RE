#include "AIRECombatGameplayTags.h"

namespace AIRECombatGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		DataDamage,
		"Data.Combat.Damage",
		"Non-negative damage magnitude supplied to the shared combat execution.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		DataStagger,
		"Data.Combat.Stagger",
		"Non-negative stagger magnitude supplied to the shared combat execution.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		StateDead,
		"State.Combat.Dead",
		"The combatant has no remaining health.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		StateFlinching,
		"State.Combat.Reaction.Flinching",
		"The combatant is in a short flinch reaction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		StateStunned,
		"State.Combat.Reaction.Stunned",
		"The combatant is stunned.");
}

#include "AIRECompanionGameplayTags.h"

namespace AIRECompanionGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityCombatBasicAttack, "Ability.Companion.Combat.BasicAttack", "Companion basic melee attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventAttackRequest, "Event.Companion.Attack.Request", "Requests an attack against the target in the gameplay event payload.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventAttackHit, "Event.Companion.Attack.Hit", "Signals the hit frame for the active companion attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateAction, "State.Companion.Action", "Companion is executing a GAS-owned action.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateActionAttacking, "State.Companion.Action.Attacking", "Companion attack ability is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateDisabled, "State.Companion.Disabled", "Companion cannot select normal behavior.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateDisabledDead, "State.Companion.Disabled.Dead", "Companion health is depleted.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CooldownBasicAttack, "Cooldown.Companion.BasicAttack", "Companion basic attack cooldown is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponCompanion, "Weapon.Companion", "Root tag for companion weapon identities.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponCompanionMelee, "Weapon.Companion.Melee", "Root tag for companion melee weapon identities.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponCompanionMeleeBasic, "Weapon.Companion.Melee.Basic", "Stable identity for the basic melee weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataAttackStaminaCost, "Data.Companion.Attack.StaminaCost", "Negative stamina modifier supplied to an attack cost Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataAttackCooldownDuration, "Data.Companion.Attack.CooldownDuration", "Duration supplied to an attack cooldown Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataDamage, "Data.Companion.Damage", "Negative health delta supplied by a damage Gameplay Effect spec.");
}

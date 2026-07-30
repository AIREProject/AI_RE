#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"

namespace AIRECompanionGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityCombatBasicAttack, "Ability.Companion.Combat.BasicAttack", "Companion basic melee attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilityCombatSkill, "Ability.Companion.Combat.Skill", "Companion weapon combat skill ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AbilitySupportHealingItem, "Ability.Companion.Support.HealingItem", "Companion healing consumable support ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventAttackRequest, "Event.Companion.Attack.Request", "Requests an attack against the target in the gameplay event payload.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventAttackHit, "Event.Companion.Attack.Hit", "Requests hit resolution at the active companion attack frame.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventAttackComboWindow, "Event.Companion.Attack.ComboWindow", "Root tag for companion combo window events.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventAttackComboWindowBegin, "Event.Companion.Attack.ComboWindow.Begin", "Opens the transition window for the active companion combo step.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventAttackComboWindowEnd, "Event.Companion.Attack.ComboWindow.End", "Closes the transition window for the active companion combo step.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventCombatSkillRequest, "Event.Companion.CombatSkill.Request", "Requests the equipped companion combat skill.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventCombatSkillHit, "Event.Companion.CombatSkill.Hit", "Requests hit resolution for the active companion combat skill.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventCombatSkillTransition, "Event.Companion.CombatSkill.Transition", "Root tag for combat skill transition events.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventCombatSkillStarted, "Event.Companion.CombatSkill.Transition.Started", "Suspends the current basic combo after the combat skill has committed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventCombatSkillEnded, "Event.Companion.CombatSkill.Transition.Ended", "Allows a suspended basic combo to continue after the combat skill ends.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventSupportHealRequest, "Event.Companion.Support.Heal.Request", "Requests treatment of the support target with a healing consumable.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateAction, "State.Companion.Action", "Companion is executing a GAS-owned action.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateActionAttacking, "State.Companion.Action.Attacking", "Companion attack ability is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateActionAttackingBasic, "State.Companion.Action.Attacking.Basic", "Companion basic attack ability is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateActionAttackingSkill, "State.Companion.Action.Attacking.Skill", "Companion combat skill ability is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateActionAttackingSkillCancelable, "State.Companion.Action.Attacking.SkillCancelable", "The active basic combo is at a safe combat skill insertion window.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateActionSupporting, "State.Companion.Action.Supporting", "Companion is applying a support consumable.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateDisabled, "State.Companion.Disabled", "Companion cannot select normal behavior.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateDisabledDead, "State.Companion.Disabled.Dead", "Companion health is depleted.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CooldownBasicAttack, "Cooldown.Companion.BasicAttack", "Companion basic attack cooldown is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CooldownCombatSkill, "Cooldown.Companion.CombatSkill", "Companion combat skill cooldown is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CooldownSupportHealingItem, "Cooldown.Companion.Support.HealingItem", "Companion healing item cooldown is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponCompanion, "Weapon.Companion", "Root tag for companion weapon identities.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponCompanionMelee, "Weapon.Companion.Melee", "Root tag for companion melee weapon identities.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponCompanionMeleeBasic, "Weapon.Companion.Melee.Basic", "Stable identity for the basic melee weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataAttackStaminaCost, "Data.Companion.Attack.StaminaCost", "Legacy attack cost data tag retained for asset compatibility; basic attacks no longer use it.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataAttackCooldownDuration, "Data.Companion.Attack.CooldownDuration", "Duration supplied to an attack cooldown Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataCombatSkillCooldownDuration, "Data.Companion.CombatSkill.CooldownDuration", "Duration supplied to a combat skill cooldown Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataDamage, "Data.Companion.Damage", "Negative health delta supplied by a damage Gameplay Effect spec.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataHealing, "Data.Companion.Healing", "Positive health delta supplied by a healing Gameplay Effect spec.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DataSupportCooldownDuration, "Data.Companion.Support.CooldownDuration", "Duration supplied to a support healing item cooldown Gameplay Effect.");
}

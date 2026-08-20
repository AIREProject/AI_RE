// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECombatGameplayTags.h"

namespace AI_RECombatGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_MeleeAttack, "Event.Combat.MeleeAttack", "근접 공격 스킬 실행 이벤트");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_MeleeHit, "Event.Combat.MeleeHit", "무기 타격 성공 이벤트 (대미지 적용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ComboWindowOpen, "Event.Combat.ComboWindowOpen", "콤보 입력 유예 기간 시작");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ComboWindowClose, "Event.Combat.ComboWindowClose", "콤보 입력 유예 기간 종료");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ComboInput, "Event.Combat.ComboInput", "콤보 입력(마우스 클릭) 발생 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ActiveHit_Start, "Event.Combat.ActiveHit.Start", "대검 등 다단/지속 타격 시작 이벤트");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ActiveHit_End, "Event.Combat.ActiveHit.End", "대검 등 다단/지속 타격 종료 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Player_Melee, "Weapon.Player.Melee", "플레이어용 근접 무기 베이스 카테고리 태그");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Player_Melee_Sword, "Weapon.Player.Melee.Sword", "플레이어용 대검/검 무기 태그");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Player_Melee_Fist, "Weapon.Player.Melee.Fist", "플레이어용 맨손/주먹 무기 태그");
}

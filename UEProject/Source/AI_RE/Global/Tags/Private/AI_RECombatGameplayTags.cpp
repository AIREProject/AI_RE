// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECombatGameplayTags.h"

namespace AI_RECombatGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_MeleeAttack, "Event.Combat.MeleeAttack", "근접 공격 스킬 실행 이벤트");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_MeleeHit, "Event.Combat.MeleeHit", "무기 타격 성공 이벤트 (대미지 적용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ComboWindowOpen, "Event.Combat.ComboWindowOpen", "콤보 입력 유예 기간 시작");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ComboWindowClose, "Event.Combat.ComboWindowClose", "콤보 입력 유예 기간 종료");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ComboInput, "Event.Combat.ComboInput", "콤보 입력(마우스 클릭) 발생 이벤트");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Player_Melee, "Weapon.Player.Melee", "플레이어용 근접 무기 태그");
}

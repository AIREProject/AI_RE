// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

namespace AI_RECombatGameplayTags
{
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_MeleeAttack);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_MeleeHit);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_ComboWindowOpen);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_ComboWindowClose);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_ComboInput);

	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_ActiveHit_Start);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_ActiveHit_End);

	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Attack_Trace);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Attack_TraceBegin);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Attack_TraceSample);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Attack_TraceEnd);

	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Player_Melee);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Player_Melee_Sword);
	AI_RE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Player_Melee_Fist);
}

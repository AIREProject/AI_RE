// Copyright MixUpProject. All Rights Reserved.

#include "AI_REItemEffect_Consumable.h"
#include "AI_RECharacterBase.h"
#include "AI_REStatusComponent.h"
#include "Engine/Engine.h"

bool UAI_REItemEffect_Consumable::ApplyEffect_Implementation(AAI_RECharacterBase* TargetCharacter)
{
	if (!TargetCharacter) return false;

	UAI_REStatusComponent* StatusComp = TargetCharacter->GetStatusComponent();
	if (!StatusComp) return false;

	bool bEffectApplied = false;

	if (HealAmount > 0.0f)
	{
		StatusComp->RecoverHP(HealAmount);
		bEffectApplied = true;
	}

	if (SPRecoveryAmount > 0.0f)
	{
		StatusComp->RecoverSP(SPRecoveryAmount);
		bEffectApplied = true;
	}

	if (HungerRecoveryAmount > 0.0f)
	{
		StatusComp->RecoverHunger(HungerRecoveryAmount);
		bEffectApplied = true;
	}

	if (ThirstyRecoveryAmount > 0.0f)
	{
		StatusComp->RecoverThirsty(ThirstyRecoveryAmount);
		bEffectApplied = true;
	}

	return bEffectApplied;
}

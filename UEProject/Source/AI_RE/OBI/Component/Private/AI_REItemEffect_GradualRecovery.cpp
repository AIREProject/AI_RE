// Copyright MixUpProject. All Rights Reserved.

#include "AI_REItemEffect_GradualRecovery.h"
#include "AI_RECharacterBase.h"
#include "AI_REStatusComponent.h"
#include "Engine/Engine.h"

bool UAI_REItemEffect_GradualRecovery::ApplyEffect_Implementation(AAI_RECharacterBase* TargetCharacter)
{
	if (!TargetCharacter) return false;

	UAI_REStatusComponent* StatusComp = TargetCharacter->GetStatusComponent();
	if (!StatusComp) return false;

	if (Duration > 0.0f && (TotalHealAmount > 0.f || TotalSPAmount > 0.f || TotalHungerAmount > 0.f || TotalThirstyAmount > 0.f))
	{
		StatusComp->AddGradualRecovery(TotalHealAmount, TotalSPAmount, TotalHungerAmount, TotalThirstyAmount, Duration);
		return true;
	}

	return false;
}

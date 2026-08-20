// Copyright MixUpProject. All Rights Reserved.

#include "AI_REItemEffect.h"

bool UAI_REItemEffect::ApplyEffect_Implementation(AAI_RECharacterBase* TargetCharacter)
{
	// Default implementation does nothing but returns true to consume the item.
	// Override this in child Blueprints or C++ classes.
	return true;
}

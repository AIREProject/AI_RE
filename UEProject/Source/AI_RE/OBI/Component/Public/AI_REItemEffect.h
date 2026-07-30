// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AI_REItemEffect.generated.h"

class AAI_RECharacterBase;

/**
 * Base class for all custom Item Effects (Hybrid GAS).
 * Can be instanced and edited inline within DataAssets, or subclassed in Blueprints.
 */
UCLASS(Blueprintable, BlueprintType, Abstract, EditInlineNew)
class AI_RE_API UAI_REItemEffect : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Applies the effect to the target character.
	 * @return true if the effect was successfully applied (and the item should be consumed).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item Effect")
	bool ApplyEffect(AAI_RECharacterBase* TargetCharacter);
	virtual bool ApplyEffect_Implementation(AAI_RECharacterBase* TargetCharacter);
};

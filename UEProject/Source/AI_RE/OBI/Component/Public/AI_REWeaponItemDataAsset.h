// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI_REItemDataAsset.h"
#include "AI_REWeaponItemDataAsset.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;

/**
 * Player specific Weapon Item DataAsset.
 * Inherits from base ItemDataAsset and adds weapon-specific combat definition.
 */
UCLASS(BlueprintType)
class AI_RE_API UAI_REWeaponItemDataAsset : public UAI_REItemDataAsset
{
	GENERATED_BODY()

public:
	// 무기 전투 스펙 (대미지, 몽타주 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> WeaponDefinition;
};

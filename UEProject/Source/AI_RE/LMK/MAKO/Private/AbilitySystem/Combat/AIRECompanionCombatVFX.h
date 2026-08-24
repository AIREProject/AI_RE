#pragma once

#include "CoreMinimal.h"

class UAIRECompanionWeaponDefinitionDataAsset;

namespace AIRECompanionCombatVFX
{
	void SpawnBossHitSlash(
		const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
		const AActor* SourceActor,
		const AActor* TargetActor,
		const FHitResult& HitResult);
}

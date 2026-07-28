// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI_REItemTypes.generated.h"

/**
 * Broad categorization of items in the game.
 */
UENUM(BlueprintType)
enum class EAI_REItemType : uint8
{
	Resource	UMETA(DisplayName = "Resource / Material"),
	Consumable	UMETA(DisplayName = "Consumable (Potion / Food)"),
	Weapon		UMETA(DisplayName = "Weapon"),
	Armor		UMETA(DisplayName = "Armor")
};

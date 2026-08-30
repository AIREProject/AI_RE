#pragma once

#include "CoreMinimal.h"

class AI_RE_API FAIRELocalCraftFallback final
{
public:
	static bool TryParseRecipeId(
		const FString& UserMessage,
		FString& OutRecipeId);
};

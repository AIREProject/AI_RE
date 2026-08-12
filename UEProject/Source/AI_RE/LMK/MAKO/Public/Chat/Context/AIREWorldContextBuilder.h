#pragma once

#include "CoreMinimal.h"
#include "Chat/Context/AIREWorldContextTypes.h"

class AAIRECompanionCharacter;

class AI_RE_API FAIREWorldContextBuilder final
{
public:
	static FAIREWorldContextV1 Build(
		const AAIRECompanionCharacter* Companion,
		const FString& LocationId);
};

#pragma once

#include "CoreMinimal.h"
#include "Command/AIRECompanionCommandTypes.h"

class AI_RE_API FAIRELocalGatherFallback final
{
public:
	static bool TryParseResource(
		const FString& UserMessage,
		EAIREGatherResourceKind& OutResource);
};

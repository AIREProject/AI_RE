#pragma once

#include "CoreMinimal.h"

class AI_RE_API FAIREChatPresentationGuard final
{
public:
	static bool IsVerbatimPlayerEcho(
		const FString& Response,
		const FString& UserMessage);

	static FString GuardDisplayText(
		const FString& Response,
		const FString& UserMessage,
		bool& bOutReplaced);
};

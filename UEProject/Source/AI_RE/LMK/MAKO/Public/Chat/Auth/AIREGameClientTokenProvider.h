#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AIREGameClientTokenProvider.generated.h"

UINTERFACE(BlueprintType)
class AI_RE_API UAIREGameClientTokenProvider : public UInterface
{
	GENERATED_BODY()
};

class AI_RE_API IAIREGameClientTokenProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "AIRE|Companion|Chat",
		meta = (DeprecatedFunction, DeprecationMessage = "AIRE_GAME uses no token provider."))
	bool GetGameClientToken(FString& OutToken);

	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "AIRE|Companion|Chat",
		meta = (DeprecatedFunction, DeprecationMessage = "AIRE_GAME uses no token provider."))
	void HandleGameClientTokenRejected();
};


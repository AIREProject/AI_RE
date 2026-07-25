#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AIRECompanionAIController.h"
#include "AIRECompanionTestingBlueprintLibrary.generated.h"

/** Temporary PIE fixture helpers. Replace these with feature-owned inputs as each behavior is implemented. */
UCLASS()
class AI_RE_API UAIRECompanionTestingBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool SetFirstCompanionTestBehaviorRequest(
		const UObject* WorldContextObject,
		EAIRECompanionTestBehaviorRequest Request,
		bool bIsRequested);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool ClearFirstCompanionTestBehaviorRequests(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool ApplyDamageToFirstCompanion(const UObject* WorldContextObject, float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool ResetFirstCompanionAttributes(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool LogFirstCompanionAbilityState(const UObject* WorldContextObject);
};

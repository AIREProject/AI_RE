#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AIREThreatTargetInterface.generated.h"

class AActor;

UINTERFACE(BlueprintType, Blueprintable)
class AI_RE_API UAIREThreatTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class AI_RE_API IAIREThreatTargetInterface
{
	GENERATED_BODY()

public:
	/** Returns whether this Actor is currently a hostile threat for the supplied observer. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AIRE|Companion|Threat")
	bool IsHostileThreatFor(const AActor* Observer) const;
	virtual bool IsHostileThreatFor_Implementation(const AActor* Observer) const;

	/** Returns whether this target can currently participate in threat selection. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AIRE|Companion|Threat")
	bool IsAliveThreatTarget() const;
	virtual bool IsAliveThreatTarget_Implementation() const;
};

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIREGameplayGuidanceSubsystem.generated.h"

UCLASS()
class AI_RE_API UAIREGameplayGuidanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void QueueDeathReturnGuidance();
	bool ConsumeDeathReturnGuidance();

private:
	bool bDeathReturnGuidancePending = false;
};

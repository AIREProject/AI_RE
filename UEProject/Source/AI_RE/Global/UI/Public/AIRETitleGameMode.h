#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AIRETitleGameMode.generated.h"

UCLASS(Abstract, Blueprintable)
class AI_RE_API AAIRETitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAIRETitleGameMode();
};

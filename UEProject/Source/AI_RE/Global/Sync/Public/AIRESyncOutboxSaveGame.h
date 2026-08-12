#pragma once

#include "CoreMinimal.h"
#include "AIRESyncOutboxTypes.h"
#include "GameFramework/SaveGame.h"
#include "AIRESyncOutboxSaveGame.generated.h"

UCLASS()
class AI_RE_API UAIRESyncOutboxSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UAIRESyncOutboxSaveGame();

	UPROPERTY(SaveGame)
	FAIRESyncOutboxSaveEnvelope Envelope;
};

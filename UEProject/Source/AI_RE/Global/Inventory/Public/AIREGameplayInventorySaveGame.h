#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryPersistenceTypes.h"
#include "GameFramework/SaveGame.h"
#include "AIREGameplayInventorySaveGame.generated.h"

UCLASS()
class AI_RE_API UAIREGameplayInventorySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UAIREGameplayInventorySaveGame();

	UPROPERTY(SaveGame)
	FAIREInventorySaveEnvelope Envelope;
};

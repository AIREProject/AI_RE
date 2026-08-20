#include "AIREGameplayInventorySaveGame.h"

UAIREGameplayInventorySaveGame::UAIREGameplayInventorySaveGame()
{
	Envelope.FormatVersion =
		AIREGameplayInventoryPersistence::SaveFormatVersion;
	Envelope.ContentVersion =
		AIREGameplayInventoryPersistence::ItemContentVersion;
}

#pragma once

#include "CoreMinimal.h"
#include "AIREChatTypes.h"
#include "AIREChatSettings.generated.h"

UCLASS(Config = Game, DefaultConfig)
class AI_RE_API UAIREChatSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Companion|Chat")
	FString BackendBaseUrl = TEXT("https://api.mtvs2026.work");

	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Companion|Chat")
	FString GameWebSocketPath = TEXT("/api/v1/game/chat");

	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Companion|Chat")
	FString HttpChatPath = TEXT("/api/v1/chat");

	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Companion|Chat")
	FString RegisterGamePath = TEXT("/api/v1/devices/register-game");

	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Companion|Chat")
	EAIREChatTransportMode TransportMode = EAIREChatTransportMode::WebSocket;

	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Companion|Chat", meta = (ClampMin = "1.0"))
	float ConnectionTimeoutSeconds = 10.0f;

	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Companion|Chat", meta = (ClampMin = "1.0"))
	float ResponseTimeoutSeconds = 35.0f;
};


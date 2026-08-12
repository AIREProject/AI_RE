#pragma once

#include "CoreMinimal.h"
#include "AIREOfflineTaskSettings.generated.h"

UCLASS(Config = Game, DefaultConfig)
class AI_RE_API UAIREOfflineTaskSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Offline")
	FString BackendBaseUrl = TEXT("https://traip.mtvs2026.work");

	UPROPERTY(Config, EditAnywhere, Category = "AIRE|Offline")
	FString TasksPath = TEXT("/api/v1/tasks");

	UPROPERTY(
		Config,
		EditAnywhere,
		Category = "AIRE|Offline",
		meta = (ClampMin = "1.0"))
	float ResponseTimeoutSeconds = 35.0f;
};

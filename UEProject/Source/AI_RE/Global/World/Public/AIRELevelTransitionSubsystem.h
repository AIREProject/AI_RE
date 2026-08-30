#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIRELevelTransitionSubsystem.generated.h"

class UUserWidget;
struct FStreamableHandle;

UCLASS()
class AI_RE_API UAIRELevelTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void PreloadLevel(FName LevelPackageName);
	void CancelPreload(FName LevelPackageName);
	bool RequestTravel(UObject* WorldContextObject, FName LevelPackageName);

private:
	FSoftObjectPath MakeWorldPath(FName LevelPackageName) const;
	void PrepareLoadingScreen();
	bool ExecutePendingTravel(float DeltaSeconds);

	TSharedPtr<FStreamableHandle> PreloadHandle;
	FName PreloadedLevelPackage;
	FName PendingLevelPackage;
	TWeakObjectPtr<UObject> PendingWorldContext;
	FTSTicker::FDelegateHandle TravelTickerHandle;
	bool bTravelRequested = false;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingWidget;
};

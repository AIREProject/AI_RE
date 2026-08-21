#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AIRETitlePlayerController.generated.h"

class UAIRETitleScreenWidget;

UCLASS(Abstract, Blueprintable)
class AI_RE_API AAIRETitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void RequestStartGame();
	void RequestExitGame();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Title|UI")
	TSubclassOf<UAIRETitleScreenWidget> TitleScreenClass;

private:
	void CreateTitleScreen();
	void RemoveTitleScreen();
	void ApplyTitleInputMode();
	void RestoreGameInputMode();

	UPROPERTY(Transient)
	TObjectPtr<UAIRETitleScreenWidget> TitleScreen;

	FName GameplayLevelName = TEXT("/Game/Levels/MainLevel_Top1");
	bool bTransitionRequested = false;
};

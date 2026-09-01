#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AIRETitlePlayerController.generated.h"

class UAIRETitleScreenWidget;
class UAIREQuitConfirmationWidget;

UCLASS(Abstract, Blueprintable)
class AI_RE_API AAIRETitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAIRETitlePlayerController();

	void RequestStartGame();
	void RequestExitGame();
	void RequestDeleteGameplayProgress();
	void ConfirmDeleteGameplayProgress();
	void CancelDeleteGameplayProgress();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Title|UI")
	TSubclassOf<UAIRETitleScreenWidget> TitleScreenClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Title|UI")
	TSubclassOf<UAIREQuitConfirmationWidget>
		DeleteSaveConfirmationWidgetClass;

private:
	void CreateTitleScreen();
	void RemoveTitleScreen();
	void ApplyTitleInputMode();
	void RestoreGameInputMode();

	UPROPERTY(Transient)
	TObjectPtr<UAIRETitleScreenWidget> TitleScreen;

	UPROPERTY(Transient)
	TObjectPtr<UAIREQuitConfirmationWidget>
		DeleteSaveConfirmationWidget;

	FName GameplayLevelName = TEXT("/Game/Levels/MainLevel_Top");
	bool bTransitionRequested = false;
};

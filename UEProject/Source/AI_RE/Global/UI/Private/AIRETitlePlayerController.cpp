#include "AIRETitlePlayerController.h"

#include "AIRETitleScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRETitle, Log, All);

void AAIRETitlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	CreateTitleScreen();
}

void AAIRETitlePlayerController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	RemoveTitleScreen();
	Super::EndPlay(EndPlayReason);
}

void AAIRETitlePlayerController::RequestStartGame()
{
	if (!IsLocalPlayerController() || bTransitionRequested)
	{
		return;
	}

	if (GameplayLevelName.IsNone())
	{
		UE_LOG(
			LogAIRETitle,
			Error,
			TEXT("Cannot start the game because the gameplay level is not configured."));
		return;
	}

	bTransitionRequested = true;
	if (IsValid(TitleScreen))
	{
		TitleScreen->SetIsEnabled(false);
	}

	RestoreGameInputMode();
	UGameplayStatics::OpenLevel(this, GameplayLevelName);
}

void AAIRETitlePlayerController::RequestExitGame()
{
	if (!IsLocalPlayerController() || bTransitionRequested)
	{
		return;
	}

	bTransitionRequested = true;
	if (IsValid(TitleScreen))
	{
		TitleScreen->SetIsEnabled(false);
	}

	UKismetSystemLibrary::QuitGame(
		this,
		this,
		EQuitPreference::Quit,
		false);
}

void AAIRETitlePlayerController::CreateTitleScreen()
{
	if (IsValid(TitleScreen))
	{
		ApplyTitleInputMode();
		return;
	}

	if (!TitleScreenClass)
	{
		UE_LOG(
			LogAIRETitle,
			Error,
			TEXT("TitleScreenClass is not assigned on the title PlayerController."));
		return;
	}

	TitleScreen = CreateWidget<UAIRETitleScreenWidget>(
		this,
		TitleScreenClass);
	if (!IsValid(TitleScreen))
	{
		UE_LOG(
			LogAIRETitle,
			Error,
			TEXT("Failed to create the title screen widget."));
		return;
	}

	TitleScreen->AddToPlayerScreen(0);
	ApplyTitleInputMode();
}

void AAIRETitlePlayerController::RemoveTitleScreen()
{
	if (!IsValid(TitleScreen))
	{
		return;
	}

	TitleScreen->RemoveFromParent();
	TitleScreen = nullptr;
}

void AAIRETitlePlayerController::ApplyTitleInputMode()
{
	if (!IsValid(TitleScreen))
	{
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(TitleScreen->TakeWidget());
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
	TitleScreen->SetUserFocus(this);
}

void AAIRETitlePlayerController::RestoreGameInputMode()
{
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);
}

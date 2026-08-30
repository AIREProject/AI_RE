#include "AIRETitlePlayerController.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AIREQuitConfirmationWidget.h"
#include "AIRELevelTransitionSubsystem.h"
#include "AIRETitleScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRETitle, Log, All);

AAIRETitlePlayerController::AAIRETitlePlayerController()
{
	static ConstructorHelpers::FClassFinder<UAIRETitleScreenWidget>
		TitleScreenFinder(
			TEXT("/Game/Work/Global/UI/Title/Blueprints/WBP_AIRETitleScreen"));
	if (TitleScreenFinder.Succeeded())
	{
		TitleScreenClass = TitleScreenFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<UAIREQuitConfirmationWidget>
		DeleteSaveConfirmationFinder(
			TEXT("/Game/Work/Global/UI/Title/WBP_AIREDeleteSaveConfirmation"));
	if (DeleteSaveConfirmationFinder.Succeeded())
	{
		DeleteSaveConfirmationWidgetClass =
			DeleteSaveConfirmationFinder.Class;
	}
}

void AAIRETitlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	CreateTitleScreen();
	if (UAIRELevelTransitionSubsystem* Transition =
		GetGameInstance()->GetSubsystem<UAIRELevelTransitionSubsystem>())
	{
		Transition->PreloadLevel(GameplayLevelName);
	}
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
	if (UAIRELevelTransitionSubsystem* Transition =
		GetGameInstance()->GetSubsystem<UAIRELevelTransitionSubsystem>())
	{
		Transition->RequestTravel(this, GameplayLevelName);
	}
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

void AAIRETitlePlayerController::RequestDeleteGameplayProgress()
{
	if (!DeleteSaveConfirmationWidgetClass)
	{
		DeleteSaveConfirmationWidgetClass = LoadClass<UAIREQuitConfirmationWidget>(
			nullptr,
			TEXT("/Game/Work/Global/UI/Title/WBP_AIREDeleteSaveConfirmation.WBP_AIREDeleteSaveConfirmation_C"));
	}
	if (!IsLocalPlayerController() || bTransitionRequested
		|| IsValid(DeleteSaveConfirmationWidget)
		|| !DeleteSaveConfirmationWidgetClass)
	{
		if (!DeleteSaveConfirmationWidgetClass)
		{
			UE_LOG(
				LogAIRETitle,
				Error,
				TEXT("Delete-save confirmation widget class is unavailable."));
		}
		return;
	}

	DeleteSaveConfirmationWidget =
		CreateWidget<UAIREQuitConfirmationWidget>(
			this, DeleteSaveConfirmationWidgetClass);
	if (!IsValid(DeleteSaveConfirmationWidget))
	{
		UE_LOG(
			LogAIRETitle,
			Error,
			TEXT("Failed to create the delete-save confirmation widget."));
		return;
	}

	DeleteSaveConfirmationWidget->AddToPlayerScreen(10);
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(DeleteSaveConfirmationWidget->TakeWidget());
	SetInputMode(InputMode);
	DeleteSaveConfirmationWidget->SetUserFocus(this);
}

void AAIRETitlePlayerController::ConfirmDeleteGameplayProgress()
{
	UGameInstance* GameInstance = GetGameInstance();
	UAIREGameplayInventorySubsystem* Inventory = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
		: nullptr;
	if (!IsValid(Inventory))
	{
		if (IsValid(DeleteSaveConfirmationWidget))
		{
			DeleteSaveConfirmationWidget->SetConfirmationMessage(
				FText::FromString(
					TEXT("세이브 삭제에 실패했습니다. 다시 시도해 주세요.")));
		}
		return;
	}

	const FAIREInventoryPersistenceResult Result =
		Inventory->DeleteGameplayProgress();
	if (Result.Code != EAIREInventoryPersistenceResultCode::Succeeded)
	{
		UE_LOG(LogAIRETitle, Error, TEXT("Failed to delete gameplay progress."));
		if (IsValid(DeleteSaveConfirmationWidget))
		{
			DeleteSaveConfirmationWidget->SetConfirmationMessage(
				FText::FromString(
					TEXT("세이브 삭제에 실패했습니다. 다시 시도해 주세요.")));
		}
		return;
	}
	CancelDeleteGameplayProgress();
}

void AAIRETitlePlayerController::CancelDeleteGameplayProgress()
{
	if (IsValid(DeleteSaveConfirmationWidget))
	{
		DeleteSaveConfirmationWidget->RemoveFromParent();
		DeleteSaveConfirmationWidget = nullptr;
	}
	ApplyTitleInputMode();
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
	if (IsValid(DeleteSaveConfirmationWidget))
	{
		DeleteSaveConfirmationWidget->RemoveFromParent();
		DeleteSaveConfirmationWidget = nullptr;
	}

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

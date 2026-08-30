#include "AIRETitleScreenWidget.h"

#include "AIRETitlePlayerController.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"

UAIRETitleScreenWidget::UAIRETitleScreenWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UAIRETitleScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(StartSurfaceButton))
	{
		StartSurfaceButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRETitleScreenWidget::HandleStartClicked);
	}

	if (IsValid(ExitButton))
	{
		ExitButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRETitleScreenWidget::HandleExitClicked);
	}

	if (IsValid(DeleteSaveButton))
	{
		DeleteSaveButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRETitleScreenWidget::HandleDeleteSaveClicked);
	}
}

void UAIRETitleScreenWidget::NativeDestruct()
{
	if (IsValid(StartSurfaceButton))
	{
		StartSurfaceButton->OnClicked.RemoveDynamic(
			this,
			&UAIRETitleScreenWidget::HandleStartClicked);
	}

	if (IsValid(ExitButton))
	{
		ExitButton->OnClicked.RemoveDynamic(
			this,
			&UAIRETitleScreenWidget::HandleExitClicked);
	}

	if (IsValid(DeleteSaveButton))
	{
		DeleteSaveButton->OnClicked.RemoveDynamic(
			this,
			&UAIRETitleScreenWidget::HandleDeleteSaveClicked);
	}

	Super::NativeDestruct();
}

FReply UAIRETitleScreenWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	(void)InGeometry;

	const FKey Key = InKeyEvent.GetKey();
	const bool bRequestsExit =
		Key == EKeys::Escape
		|| Key == EKeys::Gamepad_Special_Left
		|| Key == EKeys::Virtual_Gamepad_Back.GetVirtualKey();

	if (bRequestsExit)
	{
		HandleExitClicked();
	}
	else
	{
		HandleStartClicked();
	}

	return FReply::Handled();
}

void UAIRETitleScreenWidget::HandleStartClicked()
{
	if (AAIRETitlePlayerController* TitleController =
		Cast<AAIRETitlePlayerController>(GetOwningPlayer()))
	{
		TitleController->RequestStartGame();
	}
}

void UAIRETitleScreenWidget::HandleExitClicked()
{
	if (AAIRETitlePlayerController* TitleController =
		Cast<AAIRETitlePlayerController>(GetOwningPlayer()))
	{
		TitleController->RequestExitGame();
	}
}

void UAIRETitleScreenWidget::HandleDeleteSaveClicked()
{
	if (AAIRETitlePlayerController* TitleController =
		Cast<AAIRETitlePlayerController>(GetOwningPlayer()))
	{
		TitleController->RequestDeleteGameplayProgress();
	}
}

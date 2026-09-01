#include "AIREQuitConfirmationWidget.h"

#include "AI_REPlayerController.h"
#include "AIRETitlePlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

UAIREQuitConfirmationWidget::UAIREQuitConfirmationWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

UWidget* UAIREQuitConfirmationWidget::GetInitialFocusWidget() const
{
	return CancelButton;
}

void UAIREQuitConfirmationWidget::SetConfirmationMessage(
	const FText& Message)
{
	if (IsValid(ConfirmationMessage))
	{
		ConfirmationMessage->SetText(Message);
	}
}

void UAIREQuitConfirmationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(ConfirmButton))
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIREQuitConfirmationWidget::HandleConfirmClicked);
	}
	if (IsValid(CancelButton))
	{
		CancelButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIREQuitConfirmationWidget::HandleCancelClicked);
	}
}

void UAIREQuitConfirmationWidget::NativeDestruct()
{
	if (IsValid(ConfirmButton))
	{
		ConfirmButton->OnClicked.RemoveDynamic(
			this,
			&UAIREQuitConfirmationWidget::HandleConfirmClicked);
	}
	if (IsValid(CancelButton))
	{
		CancelButton->OnClicked.RemoveDynamic(
			this,
			&UAIREQuitConfirmationWidget::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void UAIREQuitConfirmationWidget::HandleConfirmClicked()
{
	if (AAI_REPlayerController* PlayerController =
		Cast<AAI_REPlayerController>(GetOwningPlayer()))
	{
		PlayerController->ConfirmExitGame();
		return;
	}
	if (AAIRETitlePlayerController* TitleController =
		Cast<AAIRETitlePlayerController>(GetOwningPlayer()))
	{
		TitleController->ConfirmDeleteGameplayProgress();
	}
}

void UAIREQuitConfirmationWidget::HandleCancelClicked()
{
	if (AAI_REPlayerController* PlayerController =
		Cast<AAI_REPlayerController>(GetOwningPlayer()))
	{
		PlayerController->CancelExitConfirmation();
		return;
	}
	if (AAIRETitlePlayerController* TitleController =
		Cast<AAIRETitlePlayerController>(GetOwningPlayer()))
	{
		TitleController->CancelDeleteGameplayProgress();
	}
}

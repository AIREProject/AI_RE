#include "AIREGameOverWidget.h"

#include "AI_REPlayerController.h"
#include "Components/Button.h"

UWidget* UAIREGameOverWidget::GetInitialFocusWidget() const
{
	return ReturnToVillageButton;
}

void UAIREGameOverWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(ReturnToVillageButton))
	{
		ReturnToVillageButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIREGameOverWidget::HandleReturnToVillageClicked);
	}
}

void UAIREGameOverWidget::NativeDestruct()
{
	if (IsValid(ReturnToVillageButton))
	{
		ReturnToVillageButton->OnClicked.RemoveDynamic(
			this,
			&UAIREGameOverWidget::HandleReturnToVillageClicked);
	}
	Super::NativeDestruct();
}

void UAIREGameOverWidget::HandleReturnToVillageClicked()
{
	if (AAI_REPlayerController* PlayerController =
		Cast<AAI_REPlayerController>(GetOwningPlayer()))
	{
		PlayerController->ReturnToVillageAfterDeath();
	}
}

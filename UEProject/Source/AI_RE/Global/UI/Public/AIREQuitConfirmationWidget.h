#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIREQuitConfirmationWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

UCLASS(Abstract)
class AI_RE_API UAIREQuitConfirmationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAIREQuitConfirmationWidget(const FObjectInitializer& ObjectInitializer);

	UWidget* GetInitialFocusWidget() const;
	void SetConfirmationMessage(const FText& Message);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmationMessage;
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIRETitleScreenWidget.generated.h"

class UButton;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIRETitleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAIRETitleScreenWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartSurfaceButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ExitButton;

private:
	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleExitClicked();
};

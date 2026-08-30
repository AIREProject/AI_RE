#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIREGameOverWidget.generated.h"

class UButton;
class UWidget;

UCLASS(Abstract)
class AI_RE_API UAIREGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWidget* GetInitialFocusWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleReturnToVillageClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ReturnToVillageButton;
};

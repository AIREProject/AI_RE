#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIRETargetLockMarkerWidget.generated.h"

UCLASS(Abstract)
class AI_RE_API UAIRETargetLockMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLockedTarget(AActor* TargetActor);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void UpdateMarkerPosition();

	TWeakObjectPtr<AActor> LockedTarget;
};

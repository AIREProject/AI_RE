#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIREResponseCardWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREResponseCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRuntimeResponseText(const FString& DisplayText);
};

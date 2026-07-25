#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIREResponseStackWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREResponseStackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ApplyRuntimeResponses(const TArray<FString>& ResponseTexts);
};

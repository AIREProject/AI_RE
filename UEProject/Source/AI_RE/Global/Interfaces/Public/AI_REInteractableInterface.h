#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AI_REInteractableInterface.generated.h"

UINTERFACE(MinimalAPI)
class UAI_REInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class AI_RE_API IAI_REInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);
};

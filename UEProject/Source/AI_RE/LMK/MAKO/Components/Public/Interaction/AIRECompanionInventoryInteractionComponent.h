#pragma once

#include "CoreMinimal.h"
#include "AI_REInteractableInterface.h"
#include "Components/SphereComponent.h"
#include "AIRECompanionInventoryInteractionComponent.generated.h"

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionInventoryInteractionComponent final
	: public USphereComponent
	, public IAI_REInteractableInterface
{
	GENERATED_BODY()

public:
	explicit UAIRECompanionInventoryInteractionComponent(
		const FObjectInitializer& ObjectInitializer);

	virtual void Interact_Implementation(AActor* Interactor) override;
};

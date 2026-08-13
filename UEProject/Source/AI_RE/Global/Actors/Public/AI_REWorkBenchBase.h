// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../Global/Interfaces/Public/AI_REInteractableInterface.h"
#include "../../OBI/Component/Public/AI_RECraftingTypes.h"
#include "GameplayTagContainer.h"
#include "AI_REWorkBenchBase.generated.h"

class UStaticMeshComponent;

/**
 * Base class for all crafting workbenches in the world.
 */
UCLASS()
class AI_RE_API AAI_REWorkBenchBase : public AActor, public IAI_REInteractableInterface
{
	GENERATED_BODY()
	
public:	
	AAI_REWorkBenchBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// The type of workbench this represents (used for filtering recipes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workbench")
	EWorkbenchType WorkbenchType;

	// Tags to identify this workbench for AI or other systems
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTagContainer WorkbenchTags;

	// IAI_REInteractableInterface Implementation
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
};

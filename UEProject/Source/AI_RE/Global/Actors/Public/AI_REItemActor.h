#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../Global/Interfaces/Public/AI_REInteractableInterface.h"
#include "AI_REItemActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class AI_RE_API AAI_REItemActor : public AActor, public IAI_REInteractableInterface
{
	GENERATED_BODY()
	
public:	
	AAI_REItemActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 ItemCount = 1;

protected:
	virtual void BeginPlay() override;

public:	
	// IAI_REInteractableInterface override
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> SphereComponent;
};

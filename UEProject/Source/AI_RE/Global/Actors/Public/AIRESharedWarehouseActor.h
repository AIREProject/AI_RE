#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "AI_REInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "AIRESharedWarehouseActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Physical world access point for the shared warehouse container.
 * Inventory values remain owned by UAIREGameplayInventorySubsystem.
 */
UCLASS(Blueprintable)
class AI_RE_API AAIRESharedWarehouseActor
	: public AActor
	, public IAI_REInteractableInterface
{
	GENERATED_BODY()

public:
	AAIRESharedWarehouseActor();

	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory|Warehouse")
	bool GetWarehouseSnapshot(
		FAIREInventoryContainerSnapshot& OutSnapshot) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|Warehouse")
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|Warehouse")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

protected:
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "AIRE|Inventory|Warehouse",
		meta = (DisplayName = "On Warehouse Opened"))
	void OnWarehouseOpened(
		AActor* Interactor,
		const FAIREInventoryContainerSnapshot& WarehouseSnapshot);
};

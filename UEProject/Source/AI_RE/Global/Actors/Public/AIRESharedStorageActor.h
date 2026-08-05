#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "AI_REInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "AIRESharedStorageActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Physical world access point for the shared storage container.
 * Inventory values remain owned by UAIREGameplayInventorySubsystem.
 */
UCLASS(Blueprintable)
class AI_RE_API AAIRESharedStorageActor
	: public AActor
	, public IAI_REInteractableInterface
{
	GENERATED_BODY()

public:
	AAIRESharedStorageActor();

	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory|Storage")
	bool GetStorageSnapshot(
		FAIREInventoryContainerSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory|Storage")
	FTransform GetCompanionInteractionTransform() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|Storage")
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|Storage")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory|Storage")
	TObjectPtr<USceneComponent> CompanionInteractionPoint;

protected:
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "AIRE|Inventory|Storage",
		meta = (DisplayName = "On Storage Opened"))
	void OnStorageOpened(
		AActor* Interactor,
		const FAIREInventoryContainerSnapshot& StorageSnapshot);
};

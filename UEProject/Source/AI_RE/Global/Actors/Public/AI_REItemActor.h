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

	virtual void Tick(float DeltaSeconds) override;

	/** Enables delayed proximity collection for a harvested reward. Call before FinishSpawning. */
	bool InitializeHarvestAutoPickup(
		const FGuid& InDeliveryId,
		AActor* InPreferredReceiver);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TObjectPtr<class UAI_REItemDataAsset> ItemAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 ItemCount = 1;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:	
	// IAI_REInteractableInterface override
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> SphereComponent;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Item|Companion Auto Pickup",
		meta = (ClampMin = "0.0", Units = "cm"))
	float CompanionAutoPickupRadius = 150.0f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Item|Companion Auto Pickup",
		meta = (ClampMin = "0.0", Units = "s"))
	float CompanionAutoPickupDelay = 0.4f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Item|Companion Auto Pickup",
		meta = (ClampMin = "0.05", Units = "s"))
	float CompanionAutoPickupRetryInterval = 0.5f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Item|Companion Auto Pickup",
		meta = (ClampMin = "0.0", Units = "s"))
	float CompanionPickupTravelDuration = 0.25f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Item|Companion Auto Pickup",
		meta = (Units = "cm"))
	float CompanionPickupTargetHeight = 80.0f;

private:
	void PollHarvestAutoPickup();
	bool IsPreferredReceiverWithinRange() const;
	void StartCompanionPickupPresentation(AActor& ReceiverActor);

	UPROPERTY(Transient)
	FGuid HarvestDeliveryId;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PreferredAutoPickupReceiver;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PickupPresentationTarget;

	FTimerHandle HarvestAutoPickupTimerHandle;
	FVector PickupPresentationStartLocation = FVector::ZeroVector;
	FVector PickupPresentationStartScale = FVector::OneVector;
	float PickupPresentationElapsedTime = 0.0f;
	bool bHarvestAutoPickupEnabled = false;
	bool bPickupClaimed = false;
};

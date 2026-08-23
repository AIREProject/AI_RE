#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "AIRELevelTravelPortal.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class UWorld;

UCLASS(Blueprintable)
class AI_RE_API AAIRELevelTravelPortal : public AActor
{
	GENERATED_BODY()

public:
	AAIRELevelTravelPortal();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Level Portal")
	TObjectPtr<UBoxComponent> TriggerVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Level Portal")
	TObjectPtr<UNiagaraComponent> PortalEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Level Portal")
	TSoftObjectPtr<UWorld> DestinationLevel;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "AIRE|Level Portal",
		meta = (ClampMin = "0.1", UIMin = "0.1"))
	float HoldDuration = 3.0f;

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void HandleHoldCompleted();
	void ClearHoldTimer();
	bool IsLocalPlayerPawn(const AActor* Actor) const;

	FTimerHandle HoldTimerHandle;
	bool bTravelRequested = false;
};

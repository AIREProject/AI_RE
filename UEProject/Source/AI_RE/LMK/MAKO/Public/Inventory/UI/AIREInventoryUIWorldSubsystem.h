#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIREInventoryUIWorldSubsystem.generated.h"

class AAIRECompanionCharacter;
class AAIRESharedStorageActor;
class AActor;
class APlayerController;
class UAIRECompanionInventoryPanelWidget;
class UAIREStorageInventoryPanelWidget;

UCLASS()
class AI_RE_API UAIREInventoryUIWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory|UI")
	void OpenStorageInventory(
		AAIRESharedStorageActor* Storage,
		AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory|UI")
	void OpenCompanionInventory(
		AAIRECompanionCharacter* Companion,
		AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory|UI")
	void CloseInventoryUI();

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory|UI")
	bool IsInventoryUIOpen() const;

private:
	void RestoreGameInput();
	void ApplyInventoryInput(APlayerController* PlayerController);

	UFUNCTION()
	void HandleTrackedActorDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TObjectPtr<UAIREStorageInventoryPanelWidget> StoragePanel;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionInventoryPanelWidget> CompanionPanel;

	TWeakObjectPtr<APlayerController> InputPlayerController;
	TWeakObjectPtr<AActor> TrackedActor;
	TWeakObjectPtr<AActor> InteractionActor;
	bool bOwnsInputSuppression = false;
	bool bPreviousShowMouseCursor = false;
};

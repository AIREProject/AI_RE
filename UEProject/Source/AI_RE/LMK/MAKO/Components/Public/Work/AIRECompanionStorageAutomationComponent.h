#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Work/AIRECompanionWorkOrderTypes.h"
#include "AIRECompanionStorageAutomationComponent.generated.h"

class AAIRESharedStorageActor;
class UAIRECompanionConfigDataAsset;
class UAIRECompanionInventoryComponent;
class UAIRECompanionWorkOrderComponent;
struct FAIREInventoryContainerSnapshot;

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionStorageAutomationComponent final
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionStorageAutomationComponent();

	bool InitializeAutomation(
		UAIRECompanionInventoryComponent* InInventoryComponent,
		UAIRECompanionWorkOrderComponent* InWorkOrderComponent,
		const UAIRECompanionConfigDataAsset* InCompanionConfig);
	void ShutdownAutomation();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Storage")
	void SetPreferredStorage(AAIRESharedStorageActor* InPreferredStorage);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleContainerChanged(FName ContainerId, int64 Revision);

	UFUNCTION()
	void HandleWorkOrderChanged(
		FAIRECompanionWorkOrderSnapshot PreviousSnapshot,
		FAIRECompanionWorkOrderSnapshot CurrentSnapshot);

	void ScheduleEvaluation();
	void EvaluateStorageRules();
	void CaptureFailedStorageState();
	void ClearFailedStorageState();
	bool IsFailedStorageStateUnchanged(
		const FAIREInventoryContainerSnapshot& MakoSnapshot,
		const FAIREInventoryContainerSnapshot& StorageSnapshot) const;

	UPROPERTY(EditInstanceOnly, Category = "AIRE|Companion|Storage")
	TWeakObjectPtr<AAIRESharedStorageActor> PreferredStorage;

	TWeakObjectPtr<UAIRECompanionInventoryComponent> InventoryComponent;
	TWeakObjectPtr<UAIRECompanionWorkOrderComponent> WorkOrderComponent;
	TWeakObjectPtr<UAIRECompanionConfigDataAsset> CompanionConfig;
	FGuid FailedStorageSessionId;
	int64 FailedMakoRevision = INDEX_NONE;
	int64 FailedStorageRevision = INDEX_NONE;
	bool bIsInitialized = false;
	bool bEvaluationScheduled = false;
};

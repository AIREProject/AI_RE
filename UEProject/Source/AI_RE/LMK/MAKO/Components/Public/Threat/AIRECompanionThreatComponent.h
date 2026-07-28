#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "AIRECompanionThreatComponent.generated.h"

class AAIRECompanionCharacter;
class AActor;
class APawn;
class UAIRECompanionConfigDataAsset;
class UAISenseConfig_Sight;

UCLASS(ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionThreatComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionThreatComponent();

	void StartThreatDetection(AAIRECompanionCharacter* InCompanionCharacter);
	void StopThreatDetection();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Threat")
	AActor* GetSelectedThreatTarget() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Threat")
	bool IsCombatRequested() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum class EAIREThreatCleanupReason : uint8
	{
		None,
		PerceptionLost,
		NotHostile,
		TargetInvalid,
		NoPlayer,
		OutsideDetectionDistance,
		OutsidePlayerChaseDistance,
		CompanionDisabled,
		DetectionStopped
	};

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandlePerceivedActorDestroyed(AActor* DestroyedActor);

	void ConfigureSight(const UAIRECompanionConfigDataAsset& CompanionConfig);
	void RefreshThreatSelection();
	bool IsActorHostile(AActor* Actor) const;
	bool IsActorEligible(
		AActor* Actor,
		const APawn* PlayerPawn,
		const UAIRECompanionConfigDataAsset& CompanionConfig,
		EAIREThreatCleanupReason& OutFailureReason) const;
	void AddPerceivedHostile(AActor* Actor);
	void RemovePerceivedHostile(AActor* Actor, EAIREThreatCleanupReason Reason);
	void SelectClosestEligibleTarget(
		const APawn& PlayerPawn,
		const UAIRECompanionConfigDataAsset& CompanionConfig);
	void ClearSelectedTarget(EAIREThreatCleanupReason Reason);
	void ClearPerceivedHostiles(EAIREThreatCleanupReason Reason);
	static const TCHAR* GetCleanupReasonName(EAIREThreatCleanupReason Reason);

	UPROPERTY(Transient)
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	TWeakObjectPtr<AAIRECompanionCharacter> CompanionCharacter;
	TWeakObjectPtr<AActor> SelectedThreatTarget;
	TArray<TWeakObjectPtr<AActor>> PerceivedHostiles;
	bool bIsThreatDetectionActive = false;
};

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "AIREEnemyAggroComponent.generated.h"

class AAIREEnemyBase;
class UAISenseConfig_Sight;

USTRUCT(BlueprintType)
struct AI_RE_API FAIREEnemyAggroEntrySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Aggro")
	TObjectPtr<AActor> Actor;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Aggro")
	float Threat = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Aggro")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Aggro")
	bool bHasDamageEvidence = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Aggro")
	FVector LastKnownLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREEnemyAggroSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Aggro")
	TObjectPtr<AActor> SelectedTarget;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Aggro")
	TArray<FAIREEnemyAggroEntrySnapshot> Entries;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Aggro")
	int64 TargetRevision = 0;
};

UCLASS(ClassGroup = (AIRE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIREEnemyAggroComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:
	UAIREEnemyAggroComponent();

	bool StartAggroTracking(AAIREEnemyBase* InEnemy);
	void StopAggroTracking();
	void RefreshSelection();
	void ReportDamage(AActor* Attacker, float Damage);
	bool PromoteTargetAboveCurrentMaximum(AActor* Target);
	void ResetAggro();

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Aggro")
	AActor* GetSelectedTarget() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Aggro")
	FAIREEnemyAggroSnapshot GetAggroSnapshot() const;

	bool IsSelectedTargetVisible() const;
	bool SelectedTargetHasRecentDamageEvidence() const;
	FVector GetSelectedTargetLastKnownLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FAggroEntry
	{
		TWeakObjectPtr<AActor> Actor;
		float Threat = 0.0f;
		bool bVisible = false;
		bool bHasDamageEvidence = false;
		FVector LastKnownLocation = FVector::ZeroVector;
		double LastDamageTime = -1.0;
	};

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleCandidateDestroyed(AActor* DestroyedActor);

	bool IsEligiblePartyParticipant(AActor* Actor) const;
	FAggroEntry* FindEntry(AActor* Actor);
	const FAggroEntry* FindEntry(const AActor* Actor) const;
	FAggroEntry& FindOrAddEntry(AActor* Actor);
	void RemoveEntry(AActor* Actor);
	void SelectTarget(AActor* Target);
	void ConfigureSight();

	UPROPERTY(VisibleAnywhere, Category = "AIRE|Enemy|Aggro")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Aggro", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialSightThreat = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Aggro", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageThreatMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Aggro", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RetargetMargin = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Aggro", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SwapLeadMargin = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Aggro", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float DamageEvidenceDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Aggro", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float SightRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Enemy|Aggro", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float LoseSightRadius = 2100.0f;

	TWeakObjectPtr<AAIREEnemyBase> Enemy;
	TWeakObjectPtr<AActor> SelectedTarget;
	TArray<FAggroEntry> Entries;
	int64 TargetRevision = 0;
	bool bTracking = false;
};

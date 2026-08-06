#pragma once

#include "CoreMinimal.h"
#include "AI_RECharacterBase.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "AIREEnemyReactionComponent.h"
#include "AIREEnemyBase.generated.h"

class UAIPerceptionStimuliSourceComponent;
class UAIREEnemyAttackComponent;
class UAIREEnemyConfigDataAsset;
class UAIREEnemyReactionAttributeSet;
class UAIREEnemyReactionComponent;
class UAIREEnemyVitalityComponent;

UCLASS(Abstract, Blueprintable)
class AI_RE_API AAIREEnemyBase
	: public AAI_RECharacterBase
	, public IAIREThreatTargetInterface
{
	GENERATED_BODY()

public:
	AAIREEnemyBase();

	virtual EAIRECombatAffiliation GetCombatAffiliation() const override;
	virtual FGameplayAttribute GetCombatFlinchAttribute() const override;
	virtual FGameplayAttribute GetCombatStunAttribute() const override;
	virtual bool IsCombatTargetAlive() const override;
	virtual void NotifyCombatDamageApplied(
		const FAIRECombatDamageRequest& Request) override;

	virtual bool IsHostileThreatFor_Implementation(
		const AActor* Observer) const override;
	virtual bool IsAliveThreatTarget_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy")
	UAIREEnemyVitalityComponent* GetEnemyVitalityComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy")
	UAIREEnemyReactionComponent* GetEnemyReactionComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy")
	UAIREEnemyAttackComponent* GetEnemyAttackComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Enemy|Configuration")
	const UAIREEnemyConfigDataAsset* GetEnemyConfig() const;

protected:
	virtual void BeginPlay() override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void ApplyEnemyConfig();

	UFUNCTION()
	void HandleEnemyDeath(AActor* EnemyActor);

	UFUNCTION()
	void HandleReactionStateChanged(
		EAIREEnemyReactionState PreviousState,
		EAIREEnemyReactionState CurrentState);

	void RemoveDeadEnemy();

	/** Runtime value copied from the validated Enemy Config. */
	float DeathRemovalDelay = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Vitality")
	TObjectPtr<UAIREEnemyVitalityComponent> EnemyVitalityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	TObjectPtr<UAIREEnemyReactionComponent> EnemyReactionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	TObjectPtr<UAIREEnemyAttackComponent> EnemyAttackComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	TObjectPtr<UAIREEnemyReactionAttributeSet> EnemyReactionAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Perception")
	TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSource;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Enemy|Configuration")
	TObjectPtr<UAIREEnemyConfigDataAsset> EnemyConfig;

	FTimerHandle DeathRemovalTimerHandle;
};

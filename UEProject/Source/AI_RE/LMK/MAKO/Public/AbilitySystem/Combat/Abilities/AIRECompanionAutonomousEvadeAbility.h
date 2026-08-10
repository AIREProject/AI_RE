#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/Core/AIRECompanionGameplayAbility.h"
#include "AIRECompanionAutonomousEvadeAbility.generated.h"

class UAIRECombatEvadeComponent;
struct FAIRECompanionAutonomousEvadeSettings;
struct FAIREEnemyAttackSnapshot;

UCLASS()
class AI_RE_API UAIRECompanionAutonomousEvadeAbility
	: public UAIRECompanionGameplayAbility
{
	GENERATED_BODY()

public:
	UAIRECompanionAutonomousEvadeAbility();

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	bool IsAttackOpportunityCurrent(
		const AActor* ThreatActor,
		const FGuid& ExecutionId,
		FAIREEnemyAttackSnapshot* OutSnapshot = nullptr) const;
	bool ApplySuccessfulEvadeEffects(
		const FAIRECompanionAutonomousEvadeSettings& Settings);
	void ScheduleInvulnerability(
		const FAIRECompanionAutonomousEvadeSettings& Settings);
	void ApplyInvulnerability();
	void FinishAbility(bool bWasCancelled);
	void HandleDisabledStateChanged(FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void HandleEvadeFinished();

	UFUNCTION()
	void HandleThreatDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TObjectPtr<UAIRECombatEvadeComponent> ActiveEvadeComponent;

	TWeakObjectPtr<AActor> ActiveThreatActor;
	FGuid ActiveTriggerExecutionId;
	FActiveGameplayEffectHandle InvulnerabilityEffectHandle;
	FDelegateHandle DisabledStateChangedDelegateHandle;
	FTimerHandle InvulnerabilityStartTimerHandle;
	float ActiveInvulnerabilityDuration = 0.0f;
	bool bEvadeStarted = false;
	bool bIsEnding = false;
};

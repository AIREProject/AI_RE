#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Core/AIRECompanionGameplayAbility.h"
#include "AIRECompanionUseHealingItemAbility.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECompanionItemDefinitionDataAsset;

UCLASS()
class AI_RE_API UAIRECompanionUseHealingItemAbility
	: public UAIRECompanionGameplayAbility
{
	GENERATED_BODY()

public:
	UAIRECompanionUseHealingItemAbility();

protected:
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
	const UAIRECompanionConfigDataAsset* GetCompanionConfig(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
	const UAIRECompanionItemDefinitionDataAsset* GetHealingItem() const;
	bool IsTargetInSupportRange(
		const AActor* SourceActor,
		const AActor* TargetActor,
		float SupportRange) const;
	void ApplyHealingItem();
	void FinishAbility(bool bWasCancelled);

	TWeakObjectPtr<UAIRECompanionConfigDataAsset> ActiveConfig;
	FTimerHandle TreatmentTimerHandle;
	bool bIsEnding = false;
};

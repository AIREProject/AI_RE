#include "AbilitySystem/Support/Effects/AIRECompanionHealingExecution.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "LocalAI/Support/AIREHealingTargetInterface.h"

void UAIRECompanionHealingExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* TargetAbilitySystem =
		ExecutionParams.GetTargetAbilitySystemComponent();
	const UAbilitySystemComponent* SourceAbilitySystem =
		ExecutionParams.GetSourceAbilitySystemComponent();
	AActor* TargetActor = IsValid(TargetAbilitySystem)
		? TargetAbilitySystem->GetAvatarActor()
		: nullptr;
	const AActor* Healer = IsValid(SourceAbilitySystem)
		? SourceAbilitySystem->GetAvatarActor()
		: nullptr;
	if (!IsValid(TargetActor)
		|| !TargetActor->GetClass()->ImplementsInterface(
			UAIREHealingTargetInterface::StaticClass()))
	{
		return;
	}

	float MissingHealth = 0.0f;
	if (!AIREHealingTarget::GetMissingHealth(
			TargetActor,
			Healer,
			MissingHealth))
	{
		return;
	}

	const IAIREHealingTargetInterface* HealingTarget =
		Cast<IAIREHealingTargetInterface>(TargetActor);
	if (!HealingTarget)
	{
		return;
	}

	const float RequestedHealing =
		ExecutionParams.GetOwningSpec().GetSetByCallerMagnitude(
			AIRECompanionGameplayTags::DataHealing,
			false,
			0.0f);
	const float AppliedHealing = FMath::Min(
		FMath::Max(0.0f, RequestedHealing),
		MissingHealth);
	if (AppliedHealing <= 0.0f)
	{
		return;
	}

	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(
			HealingTarget->GetHealingHealthAttribute(),
			EGameplayModOp::Additive,
			AppliedHealing));
}

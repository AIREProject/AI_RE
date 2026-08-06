#include "AIRECombatDamageExecution.h"

#include "AbilitySystemComponent.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AIRECombatGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

void UAIRECombatDamageExecution::Execute_Implementation(
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
	const AActor* SourceActor = IsValid(SourceAbilitySystem)
		? SourceAbilitySystem->GetAvatarActor()
		: nullptr;
	if (!IsValid(TargetActor)
		|| !IsValid(SourceActor)
		|| TargetActor == SourceActor
		|| !TargetActor->GetClass()->ImplementsInterface(
			UAIRECombatDamageTargetInterface::StaticClass())
		|| !SourceActor->GetClass()->ImplementsInterface(
			UAIRECombatDamageTargetInterface::StaticClass()))
	{
		return;
	}

	const IAIRECombatDamageTargetInterface* CombatTarget =
		Cast<IAIRECombatDamageTargetInterface>(TargetActor);
	const IAIRECombatDamageTargetInterface* SourceCombatant =
		Cast<IAIRECombatDamageTargetInterface>(SourceActor);
	if (!CombatTarget
		|| !SourceCombatant
		|| !SourceCombatant->IsCombatTargetAlive()
		|| !CombatTarget->CanReceiveCombatDamageFrom(SourceActor)
		|| !CombatTarget->IsCombatTargetAlive())
	{
		return;
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const float Damage = Spec.GetSetByCallerMagnitude(
		AIRECombatGameplayTags::DataDamage,
		false,
		0.0f);
	const float Stagger = Spec.GetSetByCallerMagnitude(
		AIRECombatGameplayTags::DataStagger,
		false,
		0.0f);
	if (!FMath::IsFinite(Damage)
		|| !FMath::IsFinite(Stagger)
		|| Damage < 0.0f
		|| Stagger < 0.0f)
	{
		return;
	}

	const FGameplayAttribute HealthAttribute =
		CombatTarget->GetCombatHealthAttribute();
	if (Damage > 0.0f
		&& HealthAttribute.IsValid()
		&& TargetAbilitySystem->HasAttributeSetForAttribute(HealthAttribute))
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				HealthAttribute,
				EGameplayModOp::Additive,
				-Damage));
	}

	if (Stagger <= 0.0f)
	{
		return;
	}

	const FGameplayAttribute FlinchAttribute =
		CombatTarget->GetCombatFlinchAttribute();
	if (FlinchAttribute.IsValid()
		&& TargetAbilitySystem->HasAttributeSetForAttribute(FlinchAttribute))
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				FlinchAttribute,
				EGameplayModOp::Additive,
				Stagger));
	}

	const FGameplayAttribute StunAttribute =
		CombatTarget->GetCombatStunAttribute();
	if (StunAttribute.IsValid()
		&& StunAttribute != FlinchAttribute
		&& TargetAbilitySystem->HasAttributeSetForAttribute(StunAttribute))
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				StunAttribute,
				EGameplayModOp::Additive,
				Stagger));
	}
}

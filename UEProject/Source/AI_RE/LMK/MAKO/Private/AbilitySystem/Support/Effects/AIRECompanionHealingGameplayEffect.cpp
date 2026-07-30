#include "AbilitySystem/Support/Effects/AIRECompanionHealingGameplayEffect.h"

#include "AbilitySystem/Support/Effects/AIRECompanionHealingExecution.h"

UAIRECompanionHealingGameplayEffect::UAIRECompanionHealingGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition& ExecutionDefinition =
		Executions.AddDefaulted_GetRef();
	ExecutionDefinition.CalculationClass =
		UAIRECompanionHealingExecution::StaticClass();
}

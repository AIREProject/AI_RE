#include "AIRECombatDamageGameplayEffect.h"

#include "AIRECombatDamageExecution.h"

UAIRECombatDamageGameplayEffect::UAIRECombatDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition& ExecutionDefinition =
		Executions.AddDefaulted_GetRef();
	ExecutionDefinition.CalculationClass =
		UAIRECombatDamageExecution::StaticClass();
}

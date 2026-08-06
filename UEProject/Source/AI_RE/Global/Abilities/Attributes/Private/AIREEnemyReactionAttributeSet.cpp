#include "AIREEnemyReactionAttributeSet.h"

#include "GameplayEffectExtension.h"

UAIREEnemyReactionAttributeSet::UAIREEnemyReactionAttributeSet()
{
	InitFlinchGauge(0.0f);
	InitStunGauge(0.0f);
}

void UAIREEnemyReactionAttributeSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute ModifiedAttribute =
		Data.EvaluatedData.Attribute;
	if (ModifiedAttribute == GetFlinchGaugeAttribute())
	{
		SetFlinchGauge(FMath::Max(0.0f, GetFlinchGauge()));
	}
	else if (ModifiedAttribute == GetStunGaugeAttribute())
	{
		SetStunGauge(FMath::Max(0.0f, GetStunGauge()));
	}
}

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "AIREEnemyReactionAttributeSet.generated.h"

#define AIRE_REACTION_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class AI_RE_API UAIREEnemyReactionAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAIREEnemyReactionAttributeSet();

	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	FGameplayAttributeData FlinchGauge;
	AIRE_REACTION_ATTRIBUTE_ACCESSORS(
		UAIREEnemyReactionAttributeSet,
		FlinchGauge)

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Enemy|Reaction")
	FGameplayAttributeData StunGauge;
	AIRE_REACTION_ATTRIBUTE_ACCESSORS(
		UAIREEnemyReactionAttributeSet,
		StunGauge)
};

#undef AIRE_REACTION_ATTRIBUTE_ACCESSORS

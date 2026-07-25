#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AIRECompanionAttributeSet.generated.h"

#define AIRE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class AI_RE_API UAIRECompanionAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	AIRE_ATTRIBUTE_ACCESSORS(UAIRECompanionAttributeSet, Health)
	AIRE_ATTRIBUTE_ACCESSORS(UAIRECompanionAttributeSet, MaxHealth)
	AIRE_ATTRIBUTE_ACCESSORS(UAIRECompanionAttributeSet, Stamina)
	AIRE_ATTRIBUTE_ACCESSORS(UAIRECompanionAttributeSet, MaxStamina)

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	void ClampDependentAttribute(const FGameplayAttribute& MaxAttribute, float NewMaxValue);
	float ClampAttributeValue(const FGameplayAttribute& Attribute, float NewValue) const;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Stamina;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Attributes", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxStamina;
};

#undef AIRE_ATTRIBUTE_ACCESSORS

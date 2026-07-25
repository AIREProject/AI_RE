#include "AIRECompanionAttributeSet.h"

#include "GameplayEffectExtension.h"

void UAIRECompanionAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	NewValue = ClampAttributeValue(Attribute, NewValue);
}

void UAIRECompanionAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	NewValue = ClampAttributeValue(Attribute, NewValue);
}

void UAIRECompanionAttributeSet::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	const float OldValue,
	const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (!FMath::IsNearlyEqual(OldValue, NewValue))
	{
		ClampDependentAttribute(Attribute, NewValue);
	}
}

void UAIRECompanionAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	if (Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(0.0f, GetMaxHealth()));
		ClampDependentAttribute(Attribute, GetMaxHealth());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Max(0.0f, GetMaxStamina()));
		ClampDependentAttribute(Attribute, GetMaxStamina());
	}
}

void UAIRECompanionAttributeSet::ClampDependentAttribute(
	const FGameplayAttribute& MaxAttribute,
	const float NewMaxValue)
{
	UAbilitySystemComponent* AbilitySystem = GetOwningAbilitySystemComponent();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	if (MaxAttribute == GetMaxHealthAttribute() && GetHealth() > NewMaxValue)
	{
		AbilitySystem->SetNumericAttributeBase(GetHealthAttribute(), NewMaxValue);
	}
	else if (MaxAttribute == GetMaxStaminaAttribute() && GetStamina() > NewMaxValue)
	{
		AbilitySystem->SetNumericAttributeBase(GetStaminaAttribute(), NewMaxValue);
	}
}

float UAIRECompanionAttributeSet::ClampAttributeValue(
	const FGameplayAttribute& Attribute,
	const float NewValue) const
{
	if (!FMath::IsFinite(NewValue))
	{
		return 0.0f;
	}

	if (Attribute == GetHealthAttribute())
	{
		return FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	if (Attribute == GetMaxHealthAttribute())
	{
		return FMath::Max(0.0f, NewValue);
	}
	if (Attribute == GetStaminaAttribute())
	{
		return FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	if (Attribute == GetMaxStaminaAttribute())
	{
		return FMath::Max(0.0f, NewValue);
	}

	return NewValue;
}

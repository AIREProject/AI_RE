// Copyright MixUpProject. All Rights Reserved.

#include "AI_REAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

UAI_REAttributeSet::UAI_REAttributeSet()
{
	// Initialize default values
	InitHP(100.f);
	InitMaxHP(100.f);
	InitSP(100.f);
	InitMaxSP(100.f);
	InitHunger(100.f);
	InitMaxHunger(100.f);
	InitThirsty(100.f);
	InitMaxThirsty(100.f);
	InitAttack(10.f);
	InitDefense(5.f);
	InitWorkSpeed(1.f);
}

void UAI_REAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Get the property that was modified
	FGameplayAttribute ModifiedAttribute = Data.EvaluatedData.Attribute;

	// Clamp HP
	if (ModifiedAttribute == GetHPAttribute())
	{
		SetHP(FMath::Clamp(GetHP(), 0.0f, GetMaxHP()));
	}
	// Clamp SP
	else if (ModifiedAttribute == GetSPAttribute())
	{
		SetSP(FMath::Clamp(GetSP(), 0.0f, GetMaxSP()));
	}
	// Clamp Hunger
	else if (ModifiedAttribute == GetHungerAttribute())
	{
		SetHunger(FMath::Clamp(GetHunger(), 0.0f, GetMaxHunger()));
	}
	// Clamp Thirsty
	else if (ModifiedAttribute == GetThirstyAttribute())
	{
		SetThirsty(FMath::Clamp(GetThirsty(), 0.0f, GetMaxThirsty()));
	}
}

void UAI_REAttributeSet::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();
	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilityComp)
	{
		const float CurrentValue = AffectedAttribute.GetCurrentValue();
		float NewDelta = (CurrentMaxValue > 0.f) ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;

		AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
	}
}

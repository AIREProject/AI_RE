// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AI_REAttributeSet.generated.h"

// Uses macros from AttributeSet.h
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Global Attribute Set for all Characters (Player, NPC, Enemies)
 * Handles Survival and Combat stats natively via GAS.
 */
UCLASS()
class AI_RE_API UAI_REAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAI_REAttributeSet();

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// ----------------------------------------------------
	// Survival Stats
	// ----------------------------------------------------
	
	// 체력
	UPROPERTY(BlueprintReadOnly, Category = "Status|HP")
	FGameplayAttributeData HP;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, HP)

	UPROPERTY(BlueprintReadOnly, Category = "Status|HP")
	FGameplayAttributeData MaxHP;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, MaxHP)

	// 기력
	UPROPERTY(BlueprintReadOnly, Category = "Status|SP")
	FGameplayAttributeData SP;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, SP)

	UPROPERTY(BlueprintReadOnly, Category = "Status|SP")
	FGameplayAttributeData MaxSP;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, MaxSP)

	// 허기
	UPROPERTY(BlueprintReadOnly, Category = "Status|Hunger")
	FGameplayAttributeData Hunger;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, Hunger)

	UPROPERTY(BlueprintReadOnly, Category = "Status|Hunger")
	FGameplayAttributeData MaxHunger;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, MaxHunger)

	// 수분
	UPROPERTY(BlueprintReadOnly, Category = "Status|Thirsty")
	FGameplayAttributeData Thirsty;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, Thirsty)

	UPROPERTY(BlueprintReadOnly, Category = "Status|Thirsty")
	FGameplayAttributeData MaxThirsty;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, MaxThirsty)

	// ----------------------------------------------------
	// Combat Stats
	// ----------------------------------------------------
	
	UPROPERTY(BlueprintReadOnly, Category = "Status|Combat")
	FGameplayAttributeData Attack;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, Attack)

	UPROPERTY(BlueprintReadOnly, Category = "Status|Combat")
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, Defense)
	
	// ----------------------------------------------------
	// Crafting Stats
	// ----------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Status|Work")
	FGameplayAttributeData WorkSpeed;
	ATTRIBUTE_ACCESSORS(UAI_REAttributeSet, WorkSpeed)

protected:
	// Helper function for proportional scaling when max attribute changes
	void AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty);
};

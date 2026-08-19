// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AIRECombatDamageTargetInterface.h"
#include "LocalAI/Support/AIREHealingTargetInterface.h"
#include "AI_RECharacterBase.generated.h"

class UAI_REStatusComponent;
class UAI_RESkillComponent;

/**
 * Base character class that provides common functionality for all characters (Player, NPC, Monster)
 */
UCLASS(abstract)
class AI_RE_API AAI_RECharacterBase
	: public ACharacter
	, public IAbilitySystemInterface
	, public IAIRECombatDamageTargetInterface
	, public IAIREHealingTargetInterface
{
	GENERATED_BODY()

public:
	AAI_RECharacterBase();

	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FGameplayAttribute GetCombatHealthAttribute() const override;
	virtual EAIRECombatAffiliation GetCombatAffiliation() const override;
	virtual FGameplayAttribute GetHealingHealthAttribute() const override;
	virtual FGameplayAttribute GetHealingMaxHealthAttribute() const override;
	virtual bool CanReceiveHealingFrom(const AActor* Healer) const override;
	virtual void PossessedBy(AController* NewController) override;

protected:
	virtual void BeginPlay() override;

protected:
	// 상태 컴포넌트 (체력, 스태미나 등 통합 관리)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Components")
	TObjectPtr<UAI_REStatusComponent> StatusComponent;

	// 스킬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Components")
	TObjectPtr<UAI_RESkillComponent> SkillComponent;

	// 어빌리티 시스템 컴포넌트 (GAS)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UAbilitySystemComponent> AbilitySystemComponent;

	// 어트리뷰트 셋 (GAS Data)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UAI_REAttributeSet> AttributeSet;

public:
	FORCEINLINE TObjectPtr<UAI_REStatusComponent> GetStatusComponent() const { return StatusComponent; }
	FORCEINLINE TObjectPtr<UAI_RESkillComponent> GetSkillComponent() const { return SkillComponent; }
};

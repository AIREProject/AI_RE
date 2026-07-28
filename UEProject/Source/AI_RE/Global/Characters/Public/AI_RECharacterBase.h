// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AI_RECharacterBase.generated.h"

class UAI_REStatusComponent;
class UAI_RESkillComponent;

/**
 * Base character class that provides common functionality for all characters (Player, NPC, Monster)
 */
UCLASS(abstract)
class AI_RE_API AAI_RECharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AAI_RECharacterBase();

protected:
	virtual void BeginPlay() override;

protected:
	// 상태 컴포넌트 (체력, 스태미나 등 통합 관리)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Components")
	TObjectPtr<UAI_REStatusComponent> StatusComponent;

	// 스킬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Components")
	TObjectPtr<UAI_RESkillComponent> SkillComponent;

public:
	FORCEINLINE TObjectPtr<UAI_REStatusComponent> GetStatusComponent() const { return StatusComponent; }
	FORCEINLINE TObjectPtr<UAI_RESkillComponent> GetSkillComponent() const { return SkillComponent; }
};

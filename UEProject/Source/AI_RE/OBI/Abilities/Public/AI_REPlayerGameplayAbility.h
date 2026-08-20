#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AI_REPlayerGameplayAbility.generated.h"

/**
 * AI_RE 프로젝트의 플레이어 전용 어빌리티 베이스 클래스입니다.
 */
UCLASS(Abstract)
class AI_RE_API UAI_REPlayerGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UAI_REPlayerGameplayAbility();
};

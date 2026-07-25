#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "AI_REDataTypes.generated.h"

USTRUCT(BlueprintType)
struct AI_RE_API FMixUpSkillData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    class UAnimMontage* SkillMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float MaxRange = 1000.0f;

    // 실제 스킬 이펙트나 투사체를 담당할 액터 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    TSubclassOf<class AActor> SkillActorClass; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Cooldown = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float CastTime = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Power = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Skill")
    float LastUseTime = -9999.0f;

    bool IsReady(float CurrentTime) const
    {
        return (CurrentTime - LastUseTime) >= Cooldown;
    }
};

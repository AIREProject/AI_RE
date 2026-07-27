#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AIRECompanionAnimInstance.generated.h"

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIRECompanionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AIRE|Companion|Locomotion")
	float GroundSpeed = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AIRE|Companion|Locomotion")
	float MovementDirection = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AIRE|Companion|Locomotion")
	bool bShouldMove = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AIRE|Companion|Locomotion")
	bool bIsFalling = false;
};

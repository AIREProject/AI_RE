#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIREAggroSwapComponent.generated.h"

class APlayerController;
class AAIREEnemyBase;
struct FAIREEnemyAttackSnapshot;

UENUM(BlueprintType)
enum class EAIREAggroSwapResult : uint8
{
	Applied,
	NoActiveOpportunity,
	AmbiguousOpportunity,
	OnCooldown,
	InvalidPartyState,
	CommitRejected,
	EvadeRejected,
	AggroRejected
};

/**
 * Coordinates the M03-E09 two-person Boss aggro swap.
 * Input assets remain an Editor-owned concern; this component owns runtime rules.
 */
UCLASS(ClassGroup = (AIRE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIREAggroSwapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIREAggroSwapComponent();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Combat|Aggro Swap")
	EAIREAggroSwapResult TryAggroSwap();

	UFUNCTION(BlueprintPure, Category = "AIRE|Combat|Aggro Swap")
	float GetRemainingCooldown() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Combat|Aggro Swap")
	bool IsOnCooldown() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	EAIREAggroSwapResult FindActiveOpportunity(
		TWeakObjectPtr<AAIREEnemyBase>& OutEnemy,
		FAIREEnemyAttackSnapshot& OutAttack) const;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Aggro Swap", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CooldownDuration = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Aggro Swap", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float RetreatDistance = 500.0f;

	TWeakObjectPtr<APlayerController> OwnerController;
	double NextAllowedSwapTime = 0.0;
};

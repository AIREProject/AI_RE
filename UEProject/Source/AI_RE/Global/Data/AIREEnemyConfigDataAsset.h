#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AIREEnemyConfigDataAsset.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EAIREEnemyEngagementPolicy : uint8
{
	AggressiveOnSight,
	Retaliatory
};

UENUM(BlueprintType)
enum class EAIREEnemyIdlePolicy : uint8
{
	Stationary,
	HomeWander
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREEnemyMeleeTraceSettings
{
	GENERATED_BODY()

	/** Bone or socket nearest the striking limb's base. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack|Trace")
	FName TraceStartSocket = NAME_None;

	/** Bone or socket nearest the striking limb's tip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack|Trace")
	FName TraceEndSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack|Trace", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "cm"))
	float TraceRadius = 35.0f;

	/** Maximum reach measured forward from the owner's capsule surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack|Trace", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "cm"))
	float FallbackTraceDistance = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREEnemyAttackPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	FName PatternId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float MinRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float MaxRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinHealthRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MaxHealthRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MinPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MaxPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StaggerScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CooldownScale = 1.0f;

	/** Pattern-specific lockout in addition to the shared attack cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float ReuseCooldown = 0.0f;

	/** Maximum collision-aware forward movement during the attack movement window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float ForwardMoveDistance = 0.0f;

	/** Surface distance preserved when clamping toward the target. Zero keeps the full configured move. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float ForwardMoveStopDistance = 0.0f;
};

UCLASS(BlueprintType)
class AI_RE_API UAIREEnemyConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool IsConfigurationValid(FText& OutValidationError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Movement", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm/s"))
	float MovementSpeed = 400.0f;

	/** Maximum 2D distance from the spawn point before returning home. Zero disables the distance leash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float HomeLeashRadius = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI")
	EAIREEnemyEngagementPolicy EngagementPolicy =
		EAIREEnemyEngagementPolicy::AggressiveOnSight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI")
	EAIREEnemyIdlePolicy IdlePolicy = EAIREEnemyIdlePolicy::Stationary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Wander", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm", EditCondition = "IdlePolicy == EAIREEnemyIdlePolicy::HomeWander"))
	float HomeWanderMinRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Wander", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm", EditCondition = "IdlePolicy == EAIREEnemyIdlePolicy::HomeWander"))
	float HomeWanderMaxRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Wander", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm/s", EditCondition = "IdlePolicy == EAIREEnemyIdlePolicy::HomeWander"))
	float HomeWanderSpeed = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Wander", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s", EditCondition = "IdlePolicy == EAIREEnemyIdlePolicy::HomeWander"))
	float HomeWanderWaitMin = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Wander", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s", EditCondition = "IdlePolicy == EAIREEnemyIdlePolicy::HomeWander"))
	float HomeWanderWaitMax = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Wander", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm", EditCondition = "IdlePolicy == EAIREEnemyIdlePolicy::HomeWander"))
	float HomeWanderAcceptanceRadius = 60.0f;

	/** Health ratio at or below which combat is cancelled and the enemy returns home. Zero disables retreat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Engagement", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RetreatHealthRatio = 0.0f;

	/** Enables sprint and lateral approach decisions outside the preferred melee range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Engagement")
	bool bUseCombatApproachActions = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Engagement", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm/s", EditCondition = "bUseCombatApproachActions"))
	float CombatSprintSpeed = 650.0f;

	/** Surface distance at which direct sprint changes to a tactical approach decision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Engagement", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm", EditCondition = "bUseCombatApproachActions"))
	float CombatSprintStartDistance = 500.0f;

	/** Radial distance used when choosing a point to either side of the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Engagement", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm", EditCondition = "bUseCombatApproachActions"))
	float TacticalApproachDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Engagement", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm", EditCondition = "bUseCombatApproachActions"))
	float TacticalLateralOffset = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|AI|Engagement", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s", EditCondition = "bUseCombatApproachActions"))
	float TacticalMoveDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Vitality", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxHealth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Vitality", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialHealth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Vitality", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DeathRemovalDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FlinchThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float FlinchDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float StunThreshold = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Reaction", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float StunDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackStaggerValue = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float AttackCooldownDuration = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float AttackFallbackHitDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float AttackFallbackRecoveryDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack")
	FAIREEnemyMeleeTraceSettings MeleeTrace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Enemy|Attack", meta = (TitleProperty = "PatternId"))
	TArray<FAIREEnemyAttackPattern> AttackPatterns;
};

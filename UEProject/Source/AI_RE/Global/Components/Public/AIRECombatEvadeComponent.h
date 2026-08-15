#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIRECombatEvadeComponent.generated.h"

class AActor;
class ACharacter;
class UAnimMontage;
class UGameplayAbility;
class UCurveFloat;

UENUM(BlueprintType)
enum class EAIRECombatEvadeSide : uint8
{
	Left,
	Right,
	Forward,
	Backward
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRECombatEvadePlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Combat|Evade")
	EAIRECombatEvadeSide Side = EAIRECombatEvadeSide::Right;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Combat|Evade")
	FVector Direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Combat|Evade")
	float AvailableDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Combat|Evade")
	TObjectPtr<AActor> ThreatActor;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Combat|Evade")
	FGuid TriggerExecutionId;

	bool IsValid() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAIRECombatEvadeSignature);

UCLASS(ClassGroup = (AIRE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECombatEvadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECombatEvadeComponent();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Combat|Evade")
	bool TryStartLateralDash(AActor* ThreatActor);

	/** Starts a collision-safe dash in an explicit world-space direction. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Combat|Evade")
	bool TryStartDirectionalDash(const FVector& WorldDirection);

	bool BuildLateralDashPlan(
		const AActor* ThreatActor,
		const FGuid& TriggerExecutionId,
		FAIRECombatEvadePlan& OutPlan) const;

	bool TryStartLateralDashPlan(
		const FAIRECombatEvadePlan& Plan,
		UGameplayAbility* IgnoredAbility = nullptr);

	bool BuildThreatRetreatDashPlan(
		const AActor* ThreatActor,
		const FGuid& TriggerExecutionId,
		float RetreatDistance,
		FAIRECombatEvadePlan& OutPlan) const;

	bool TryStartAggroSwapDashPlan(const FAIRECombatEvadePlan& Plan);

	UFUNCTION(BlueprintPure, Category = "AIRE|Combat|Evade")
	bool CanStartLateralDash(const AActor* ThreatActor) const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Combat|Evade")
	void CancelEvade();

	UFUNCTION(BlueprintPure, Category = "AIRE|Combat|Evade")
	bool IsEvading() const;

	bool IsEvadingFrom(
		const AActor* ThreatActor,
		const FGuid& TriggerExecutionId) const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Combat|Evade")
	FAIRECombatEvadeSignature OnEvadeStarted;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Combat|Evade")
	FAIRECombatEvadeSignature OnEvadeFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool TryStartDash(
		const FVector& Direction,
		float AvailableDistance,
		EAIRECombatEvadeSide PresentationSide,
		AActor* ThreatActor,
		const FGuid& TriggerExecutionId,
		UGameplayAbility* IgnoredAbility,
		float MaximumDistance,
		bool bAllowAirborne);
	float MeasureClearance(const FVector& Direction) const;
	void FinishEvade(bool bStopPresentation);
	void StopEvadePresentation();

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Evade", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float DashDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Evade", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float DashDuration = 0.25f;

	/** Must reference an in-place montage with Enable Root Motion disabled. */
	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Evade")
	TObjectPtr<UAnimMontage> EvadeMontage;

	/** Optional curve to control dash speed over time. If left empty, dash speed is linear. X=Time (0 to 1), Y=Distance Fraction (0 to 1) */
	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Evade")
	TObjectPtr<UCurveFloat> DashPacingCurve;

	TWeakObjectPtr<ACharacter> OwnerCharacter;
	TWeakObjectPtr<AActor> ActiveThreatActor;
	FGuid ActiveTriggerExecutionId;
	FVector DashDirection = FVector::ZeroVector;
	float ElapsedTime = 0.0f;
	float MovedDistance = 0.0f;
	float ActiveDashDistance = 0.0f;
	EMovementMode PreviousMovementMode = MOVE_Walking;
	uint8 PreviousCustomMovementMode = 0;
	bool bEvading = false;
	bool bRequiresActiveThreat = false;
};

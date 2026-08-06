#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIRECombatEvadeComponent.generated.h"

class ACharacter;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAIRECombatEvadeSignature);

UCLASS(ClassGroup = (AIRE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECombatEvadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECombatEvadeComponent();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Combat|Evade")
	bool TryStartLateralDash(AActor* ThreatActor);

	UFUNCTION(BlueprintPure, Category = "AIRE|Combat|Evade")
	bool CanStartLateralDash(const AActor* ThreatActor) const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Combat|Evade")
	void CancelEvade();

	UFUNCTION(BlueprintPure, Category = "AIRE|Combat|Evade")
	bool IsEvading() const;

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
	void FinishEvade();

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Evade", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float DashDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Evade", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float DashDuration = 0.25f;

	/** Must reference an in-place montage with Enable Root Motion disabled. */
	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Combat|Evade")
	TObjectPtr<UAnimMontage> EvadeMontage;

	TWeakObjectPtr<ACharacter> OwnerCharacter;
	FVector DashDirection = FVector::ZeroVector;
	float ElapsedTime = 0.0f;
	float MovedDistance = 0.0f;
	float ActiveDashDistance = 0.0f;
	EMovementMode PreviousMovementMode = MOVE_Walking;
	uint8 PreviousCustomMovementMode = 0;
	bool bEvading = false;
};

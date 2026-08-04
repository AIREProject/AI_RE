#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Work/AIRECompanionWorkOrderTypes.h"
#include "AIRECompanionWorkOrderComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIRECompanionWorkOrderChangedSignature,
	FAIRECompanionWorkOrderSnapshot,
	PreviousSnapshot,
	FAIRECompanionWorkOrderSnapshot,
	CurrentSnapshot);

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionWorkOrderComponent final
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionWorkOrderComponent();

	bool TryRequestWorkOrder(
		const FAIRECompanionWorkOrderRequest& Request,
		FGuid& OutWorkOrderId);
	bool TryStartMoving(const FGuid& WorkOrderId);
	bool TryStartWorking(const FGuid& WorkOrderId);
	bool TryPauseForCombat(const FGuid& WorkOrderId);
	bool TryResumeAfterCombat(const FGuid& WorkOrderId);
	bool TryCompleteWorkOrder(const FGuid& WorkOrderId);
	bool TryCancelWorkOrder(const FGuid& WorkOrderId);
	bool TryFailWorkOrder(const FGuid& WorkOrderId);
	void ShutdownWorkOrder();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Work")
	FAIRECompanionWorkOrderSnapshot GetWorkOrderSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Work")
	bool HasActiveWorkOrder() const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Work")
	FAIRECompanionWorkOrderChangedSignature OnWorkOrderChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	static bool IsActiveState(EAIRECompanionWorkOrderState State);
	static bool IsTerminalState(EAIRECompanionWorkOrderState State);
	static bool CanTransition(
		EAIRECompanionWorkOrderState CurrentState,
		EAIRECompanionWorkOrderState NewState);

	bool IsCurrentTargetUsable() const;
	bool TryTransitionTo(
		const FGuid& WorkOrderId,
		EAIRECompanionWorkOrderState NewState);
	void ApplyState(EAIRECompanionWorkOrderState NewState);
	void BindTargetDestroyed();
	void UnbindTargetDestroyed();

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	FAIRECompanionWorkOrderSnapshot CurrentWorkOrder;

	bool bIsShuttingDown = false;
};

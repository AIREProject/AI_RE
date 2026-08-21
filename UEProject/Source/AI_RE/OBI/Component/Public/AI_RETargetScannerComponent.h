// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI_RETargetScannerComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class AI_RE_API UAI_RETargetScannerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAI_RETargetScannerComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	/** 범용 전방 탐색 스캐너 (상호작용, 전투 타겟팅 등) */
	UFUNCTION(BlueprintCallable, Category = "Scanner")
	AActor* ScanForwardForTarget(float Radius, float Distance, ECollisionChannel TraceChannel, bool bDrawDebug = false);

	/** Returns only a living Enemy or an available harvestable resource. */
	UFUNCTION(BlueprintCallable, Category = "Scanner")
	AActor* ScanForwardForPlayerTarget(float Radius, float Distance, ECollisionChannel TraceChannel, bool bDrawDebug = false);

	/** 현재 캐싱된 상호작용 타겟 반환 */
	UFUNCTION(BlueprintCallable, Category = "Scanner")
	AActor* GetCachedInteractableTarget() const;

	/** Refreshes the interaction target immediately instead of waiting for the precheck timer. */
	void RefreshInteractableTarget();

	/** 캐싱된 타겟 초기화 (상호작용 완료 후 등에 호출) */
	UFUNCTION(BlueprintCallable, Category = "Scanner")
	void ResetCachedTarget();

	/** Toggles the lock-on state, targeting the nearest enemy in front using ScanForwardForPlayerTarget. */
	UFUNCTION(BlueprintCallable, Category="Scanner|Combat")
	void ToggleLockOn();

	/** Gets whether the player is currently locked on to a target. */
	UFUNCTION(BlueprintPure, Category="Scanner|Combat")
	bool GetIsLockedOn() const { return bIsLockedOn; }

	/** Gets the current lock-on target, if any. */
	UFUNCTION(BlueprintPure, Category="Scanner|Combat")
	AActor* GetLockOnTarget() const { return CurrentLockOnTarget.Get(); }

protected:
	AActor* ScanForward(
		float Radius,
		float Distance,
		ECollisionChannel TraceChannel,
		bool bDrawDebug,
		bool bRequirePlayerTarget,
		bool bRequireInteractable);

	/**
	 * Finds an interactable without relying on a specific collision trace channel.
	 * Used only as an explicit interaction fallback when the normal precheck misses.
	 */
	AActor* FindBestInteractableInFront(float MaxDistance) const;

	/** Updates the cached interaction target and its custom-depth outline. */
	void SetCachedInteractableTarget(AActor* NewTarget);

	/** Enables or disables the interaction outline on every mesh owned by the target. */
	static void SetInteractionOutlineEnabled(AActor* Target, bool bEnabled);

	/** 상호작용 프리체크 타이머 루프 */
	void PerformInteractionPrecheck();

	FTimerHandle InteractionScanTimerHandle;

	/** 캐싱된 상호작용 대상 (UI에 띄우고 즉시 상호작용하기 위함) */
	TWeakObjectPtr<AActor> CachedInteractableTarget;

	/** 상시 스캔 주기 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "Scanner")
	float ScanInterval;

	// 락온 상태 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scanner|Combat")
	bool bIsLockedOn = false;

	// 현재 락온된 타겟
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scanner|Combat")
	TWeakObjectPtr<AActor> CurrentLockOnTarget;
};

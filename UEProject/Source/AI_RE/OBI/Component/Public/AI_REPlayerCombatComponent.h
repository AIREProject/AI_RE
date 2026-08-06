// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI_REItemDataAsset.h"
#include "Engine/StreamableManager.h"
#include "AI_REPlayerCombatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrimaryActionHitSignature, AActor*, HitActor);

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAI_REPlayerCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAI_REPlayerCombatComponent();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TryStartPrimaryAction();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TryStopPrimaryAction();

	/** Stops only this component's active attack presentation for a forced evade. */
	void CancelPrimaryActionForEvade();

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void EquipWeapon(UAI_REItemDataAsset* WeaponData);

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void UnequipWeapon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Equipment")
	TObjectPtr<UAI_REItemDataAsset> EquippedWeapon;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnPrimaryActionHitSignature OnPrimaryActionHit;

	// 스태미나 소모량 (기획자 요청으로 보존, 0으로 설정하면 소모 안함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float AttackStaminaCost = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackStaggerValue = 25.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void PerformTraceHit();

	bool bIsActionActive = false;
	FGuid ActiveExecutionId;

	UPROPERTY()
	FTimerHandle ActionTimerHandle;
	
	// 비동기 로딩이 완료된 애니메이션 몽타주 캐싱
	UPROPERTY(Transient)
	TObjectPtr<class UAnimMontage> CachedAttackMontage;
	
	TSharedPtr<struct FStreamableHandle> MontageLoadHandle;
	
	void OnWeaponMontageLoaded();
};

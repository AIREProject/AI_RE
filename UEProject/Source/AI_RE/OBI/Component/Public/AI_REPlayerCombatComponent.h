// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../../Global/Components/Public/AI_REStatusComponent.h"
#include "../../OBI/Component/Public/AI_REItemDataAsset.h"
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

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void EquipWeapon(UAI_REItemDataAsset* WeaponData);

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void UnequipWeapon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Equipment")
	TObjectPtr<UAI_REItemDataAsset> EquippedWeapon;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnPrimaryActionHitSignature OnPrimaryActionHit;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float TraceDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float BaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float AttackStaminaCost = 15.f;

	// Montage to play on primary action
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Animation")
	TObjectPtr<class UAnimMontage> PrimaryAttackMontage;

protected:
	virtual void BeginPlay() override;

	void PerformTraceHit();

	bool bIsActionActive = false;

	UPROPERTY()
	FTimerHandle ActionTimerHandle;
};

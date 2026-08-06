// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI_REItemDataAsset.h"
#include "Engine/StreamableManager.h"
#include "AI_REPlayerCombatComponent.generated.h"

class UAnimMontage;
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAI_REPlayerCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAI_REPlayerCombatComponent();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TryStartPrimaryAction();

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void EquipWeapon(UAI_REItemDataAsset* WeaponData);

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void UnequipWeapon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Equipment")
	TObjectPtr<UAI_REItemDataAsset> EquippedWeapon;

	// 스태미나 소모량 (기획자 요청으로 보존, 0으로 설정하면 소모 안함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float AttackStaminaCost = 15.f;

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	UAnimMontage* GetCachedAttackMontage() const { return CachedAttackMontage; }

protected:
	virtual void BeginPlay() override;

	// 비동기 로딩이 완료된 애니메이션 몽타주 캐싱
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CachedAttackMontage;
	
	TSharedPtr<FStreamableHandle> MontageLoadHandle;
	
	void OnWeaponMontageLoaded();
};

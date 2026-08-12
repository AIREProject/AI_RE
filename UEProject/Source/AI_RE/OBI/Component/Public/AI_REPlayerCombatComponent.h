// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI_REItemDataAsset.h"
#include "GameplayAbilitySpecHandle.h"
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

	bool TryEquipWeapon(UAI_REItemDataAsset* WeaponData);

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void UnequipWeapon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Equipment")
	TObjectPtr<UAI_REItemDataAsset> EquippedWeapon;

	// 무기 장착 시 부여된 스킬 핸들 목록
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> GrantedWeaponAbilities;

	// 아무 무기도 장착하지 않았을 때 기본으로 장착될 무기 (맨주먹 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Equipment")
	TObjectPtr<UAI_REItemDataAsset> DefaultUnarmedWeapon;

	// 스태미나 소모량 (0으로 설정하면 소모 안함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float AttackStaminaCost = 15.f;

	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	UAnimMontage* GetCachedAttackMontage() const { return CachedAttackMontage; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 비동기 로딩이 완료된 애니메이션 몽타주 캐싱
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CachedAttackMontage;
	
	TSharedPtr<FStreamableHandle> MontageLoadHandle;
	
	void OnWeaponMontageLoaded(
		uint32 RequestId,
		TWeakObjectPtr<UAI_REItemDataAsset> ExpectedWeapon);

	/** 장착된 무기가 부여한 스킬(어빌리티)들을 회수합니다. */
	void ClearWeaponAbilities();

	/** 손에 쥐고 있는 무기 외형과 레이어드 애니메이션을 해제하여 맨손 상태로 되돌립니다. */
	void ClearWeaponVisuals();

	uint32 MontageLoadRequestId = 0;
};

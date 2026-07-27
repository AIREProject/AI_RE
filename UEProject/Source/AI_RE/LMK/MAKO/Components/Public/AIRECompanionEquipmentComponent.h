#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AIRECompanionEquipmentComponent.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;
class UAbilitySystemComponent;
class UAnimInstance;
struct FStreamableHandle;

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionEquipmentComponent();

	bool InitializeEquipment(UAbilitySystemComponent* InAbilitySystem, float BasicAttackCooldown);
	void ShutdownEquipment();

	/** Returns true when the asynchronous equip request is accepted. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Equipment")
	bool EquipWeapon(UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Equipment")
	void UnequipCurrentWeapon();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Equipment")
	const UAIRECompanionWeaponDefinitionDataAsset* GetCurrentWeaponDefinition() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Equipment")
	FGameplayTag GetCurrentWeaponTag() const;

	bool IsCurrentWeaponInCategory(FGameplayTag WeaponCategory) const;
	FGameplayAbilitySpecHandle FindGrantedAbilityHandle(FGameplayTag AbilityTag) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CompleteEquipWeapon(uint32 RequestId);
	void CancelPendingEquipmentLoad();
	void HandleDeadStateChanged(FGameplayTag Tag, int32 NewCount);
	void LinkCurrentAnimLayer();
	void ReleaseCurrentWeaponState();
	void UnlinkCurrentAnimLayer();
	UAnimInstance* GetOwnerAnimInstance() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> DefaultWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> CurrentWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> DesiredWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> PendingWeaponDefinition;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> CurrentLinkedAnimLayerClass;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
	TSharedPtr<FStreamableHandle> PendingEquipmentLoadHandle;
	TSharedPtr<FStreamableHandle> ActiveEquipmentLoadHandle;
	float ConfiguredBasicAttackCooldown = 0.0f;
	float PendingBasicAttackCooldown = 0.0f;
	uint32 EquipmentRequestId = 0;
	FDelegateHandle DeadStateChangedDelegateHandle;
};

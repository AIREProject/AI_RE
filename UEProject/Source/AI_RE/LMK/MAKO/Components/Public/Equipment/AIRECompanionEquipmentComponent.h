#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AIRECompanionEquipmentComponent.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;
class UAbilitySystemComponent;
class UAnimInstance;
class UNiagaraComponent;
class USkeletalMeshComponent;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FAIRECompanionWeaponEquipCompleted,
	UAIRECompanionWeaponDefinitionDataAsset*,
	bool);

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionEquipmentComponent();

	bool InitializeEquipment(
		UAbilitySystemComponent* InAbilitySystem,
		bool bEquipLegacyDefault = true);
	void ShutdownEquipment();
	FAIRECompanionWeaponEquipCompleted& OnWeaponEquipCompleted();

	/** Returns true when the asynchronous equip request is accepted. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Equipment")
	bool EquipWeapon(UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Equipment")
	void UnequipCurrentWeapon();

	UFUNCTION(BlueprintPure, Category = "AIRE|Equipment")
	const UAIRECompanionWeaponDefinitionDataAsset* GetCurrentWeaponDefinition() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Equipment")
	FGameplayTag GetCurrentWeaponTag() const;

	bool IsCurrentWeaponInCategory(FGameplayTag WeaponCategory) const;
	FGameplayAbilitySpecHandle FindGrantedAbilityHandle(FGameplayTag AbilityTag) const;
	int32 GetLastBasicComboVariantIndex(
		const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition) const;
	void SetLastBasicComboVariantIndex(
		UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
		int32 VariantIndex);
	void StartAttackTrail(
		USkeletalMeshComponent* MeshComponent,
		FName AttachSocket);
	void StopAttackTrail();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Equipment|Katana")
	void SetKatanaBladeDrawn(bool bDrawn);

	/** Keeps the Dual visuals hidden and the Katana hand weapon visible. */
	void SetCombatPresentationActive(bool bIsInCombat);

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

	bool IsKatanaWeapon(const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition) const;
	void DestroyKatanaVisuals();
	void SetDualWeaponVisualsVisible(bool bVisible);
	void SetKatanaEvadePresentation(bool bEnabled);

	// Legacy fallback for Companion assets without an inventory loadout.
	// Do not add DeprecatedProperty metadata: UE 5.8 PropertyEditor recursively
	// expands this native default subobject and overflows the editor stack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> DefaultWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> CurrentWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> DesiredWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> PendingWeaponDefinition;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> CurrentLinkedAnimLayerClass;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveAttackTrailComponent;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TMap<FSoftObjectPath, int32> LastBasicComboVariantIndices;

	bool bCombatPresentationActive = false;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
	TSharedPtr<FStreamableHandle> PendingEquipmentLoadHandle;
	TSharedPtr<FStreamableHandle> ActiveEquipmentLoadHandle;
	uint32 EquipmentRequestId = 0;
	FDelegateHandle DeadStateChangedDelegateHandle;
	FAIRECompanionWeaponEquipCompleted WeaponEquipCompleted;
};

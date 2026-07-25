#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AIRECompanionEquipmentComponent.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;
class UAbilitySystemComponent;

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionEquipmentComponent();

	bool InitializeEquipment(UAbilitySystemComponent* InAbilitySystem, float BasicAttackCooldown);
	void ShutdownEquipment();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Equipment")
	const UAIRECompanionWeaponDefinitionDataAsset* GetCurrentWeaponDefinition() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Equipment")
	FGameplayTag GetCurrentWeaponTag() const;

	bool IsCurrentWeaponInCategory(FGameplayTag WeaponCategory) const;
	FGameplayAbilitySpecHandle FindGrantedAbilityHandle(FGameplayTag AbilityTag) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool EquipWeapon(
		UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
		float BasicAttackCooldown);
	void UnequipCurrentWeapon();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> DefaultWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> CurrentWeaponDefinition;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
};

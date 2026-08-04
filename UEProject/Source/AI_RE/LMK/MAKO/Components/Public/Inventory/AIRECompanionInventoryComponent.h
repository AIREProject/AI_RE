#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "Components/ActorComponent.h"
#include "AIRECompanionInventoryComponent.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECompanionEquipmentComponent;
class UAIRECompanionItemDefinitionDataAsset;
class UAIRECompanionWeaponDefinitionDataAsset;
class UAIREGameplayInventorySubsystem;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAIRECompanionInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIRECompanionWeaponEquipResult,
	FName,
	WeaponItemId,
	bool,
	bSucceeded);

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionInventoryComponent();

	bool InitializeInventory(
		const UAIRECompanionConfigDataAsset* CompanionConfig,
		UAIRECompanionEquipmentComponent* InEquipmentComponent,
		UAbilitySystemComponent* InAbilitySystem);
	void ShutdownInventory();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Inventory")
	bool HasItem(FName ItemId, int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Inventory")
	bool TryAddItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Inventory")
	bool TryConsumeItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Inventory")
	bool EquipWeaponItem(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Inventory")
	FAIREInventoryMutationResult RequestEquipWeaponItem(
		const FAIREInventoryEquipRequest& Request);

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Inventory")
	FName GetEquippedWeaponItemId() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Inventory")
	FName GetPendingWeaponItemId() const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Inventory")
	bool GetInventorySnapshot(
		FAIREInventoryContainerSnapshot& OutSnapshot) const;

	bool CanCompleteMakoCraftWork(
		const FAIREMakoCraftWorkRequest& Request,
		FAIREInventoryWorkResult& OutResult) const;

	FAIREInventoryWorkResult TryCompleteMakoCraftWork(
		const FAIREMakoCraftWorkRequest& Request);

	FAIREInventoryWorkResult TryStoreMakoWorkReward(
		const FAIREMakoWorkRewardRequest& Request);

	const UAIRECompanionItemDefinitionDataAsset* FindItemDefinition(
		FName ItemId) const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Inventory")
	FAIRECompanionInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Inventory")
	FAIRECompanionWeaponEquipResult OnWeaponEquipResult;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class EAIREEquipmentCallbackMode : uint8
	{
		None,
		RestoreCurrent,
		Equipping,
		Recovering
	};

	void HandleWeaponEquipCompleted(
		UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
		bool bSucceeded);
	UFUNCTION()
	void HandleContainerChanged(FName ContainerId, int64 Revision);
	bool RequestRuntimeEquipment(
		FName ItemId,
		EAIREEquipmentCallbackMode CallbackMode,
		const FGuid& SessionId,
		const FGuid& MutationId);
	void ClearActiveEquipmentRequest();

	TWeakObjectPtr<UAIREGameplayInventorySubsystem> GameplayInventory;
	TWeakObjectPtr<UAIRECompanionEquipmentComponent> EquipmentComponent;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle WeaponEquipCompletedDelegateHandle;
	FGuid BoundInventorySessionId;
	FGuid ActiveEquipmentSessionId;
	FGuid ActiveEquipmentMutationId;
	EAIREEquipmentCallbackMode EquipmentCallbackMode =
		EAIREEquipmentCallbackMode::None;
	bool bIsInitialized = false;
};

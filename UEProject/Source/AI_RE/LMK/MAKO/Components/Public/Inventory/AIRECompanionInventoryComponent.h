#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIRECompanionInventoryComponent.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECompanionEquipmentComponent;
class UAIRECompanionItemDefinitionDataAsset;
class UAIRECompanionWeaponDefinitionDataAsset;
class UAbilitySystemComponent;

struct FAIRECompanionInventoryStack
{
	TObjectPtr<UAIRECompanionItemDefinitionDataAsset> ItemDefinition;

	int32 Count = 0;
};

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

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Inventory")
	FName GetEquippedWeaponItemId() const;

	const UAIRECompanionItemDefinitionDataAsset* FindItemDefinition(
		FName ItemId) const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Inventory")
	FAIRECompanionInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Inventory")
	FAIRECompanionWeaponEquipResult OnWeaponEquipResult;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool TryAddItemDefinition(
		UAIRECompanionItemDefinitionDataAsset* ItemDefinition,
		int32 Count);
	bool EquipWeaponItemInternal(FName ItemId, bool bIsRecovery);
	void HandleWeaponEquipCompleted(
		UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
		bool bSucceeded);

	TArray<FAIRECompanionInventoryStack> ItemStacks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAIRECompanionItemDefinitionDataAsset>> KnownItemDefinitions;

	int32 MaxInventorySlots = 0;

	FName EquippedWeaponItemId;

	FName PendingWeaponItemId;
	FName PreviousWeaponItemId;
	TWeakObjectPtr<UAIRECompanionEquipmentComponent> EquipmentComponent;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle WeaponEquipCompletedDelegateHandle;
	bool bIsInitialized = false;
};

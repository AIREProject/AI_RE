#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystemInterface.h"
#include "AIREHarvestRewardReceiver.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AI_REInteractableInterface.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AIRECompanionCharacter.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECombatEvadeComponent;
class UAIRECompanionAttributeSet;
class UAIRECompanionChatComponent;
class UAIRECompanionCommandGatewayComponent;
class UAIRECompanionEquipmentComponent;
class UAIRECompanionInventoryComponent;
class UAIRECompanionInventoryInteractionComponent;
class UAIRECompanionLocalBehaviorPolicyComponent;
class UAIRECompanionSupportComponent;
class UAIRECompanionStorageAutomationComponent;
class UAIRECompanionWorkOrderComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

UCLASS(Blueprintable)
class AI_RE_API AAIRECompanionCharacter
	: public ACharacter
	, public IAbilitySystemInterface
	, public IAIRECombatDamageTargetInterface
	, public IAIREHarvestRewardReceiver
	, public IAI_REInteractableInterface
{
	GENERATED_BODY()

public:
	AAIRECompanionCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FGameplayAttribute GetCombatHealthAttribute() const override;
	virtual EAIRECombatAffiliation GetCombatAffiliation() const override;
	virtual bool TryReceiveHarvestReward_Implementation(
		FGuid DeliveryId,
		FName ItemId,
		int32 Count) override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Abilities")
	const UAIRECompanionAttributeSet* GetCompanionAttributeSet() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Abilities")
	bool IsAbilitySystemDisabled() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Equipment")
	UAIRECompanionEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory")
	UAIRECompanionInventoryComponent* GetInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Interaction")
	UAIRECompanionInventoryInteractionComponent*
		GetInventoryInteractionComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Support")
	UAIRECompanionSupportComponent* GetSupportComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Policy")
	UAIRECompanionLocalBehaviorPolicyComponent*
		GetLocalBehaviorPolicyComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Chat")
	UAIRECompanionChatComponent* GetChatComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Command")
	UAIRECompanionCommandGatewayComponent* GetCommandGatewayComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Work")
	UAIRECompanionWorkOrderComponent* GetWorkOrderComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Storage")
	UAIRECompanionStorageAutomationComponent*
		GetStorageAutomationComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Combat")
	UAIRECombatEvadeComponent* GetCombatEvadeComponent() const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Appearance")
	void SetSoxAndShoesVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "AIRE|Appearance")
	bool AreSoxAndShoesVisible() const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Appearance")
	void SetHoodVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "AIRE|Appearance")
	bool IsHoodVisible() const;

	bool ResetAttributesToConfiguredDefaults();

	UFUNCTION(BlueprintPure, Category = "AIRE")
	FString GetCompanionId() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Configuration")
	const UAIRECompanionConfigDataAsset* GetCompanionConfig() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplySoxAndShoesVisibility();
	void ApplyHoodVisibility();
	bool InitializeAutonomousEvadeRuntime();
	void ShutdownAutonomousEvadeRuntime();
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleInvulnerableStateChanged(FGameplayTag Tag, int32 NewCount);
	void SynchronizeDeadState(float CurrentHealth);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Abilities", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionAttributeSet> CompanionAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Equipment", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Interaction", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionInventoryInteractionComponent>
		InventoryInteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Support", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionSupportComponent> SupportComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Policy", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionLocalBehaviorPolicyComponent>
		LocalBehaviorPolicyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Chat", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionChatComponent> ChatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Command", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionCommandGatewayComponent> CommandGatewayComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Work", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionWorkOrderComponent> WorkOrderComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Storage", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionStorageAutomationComponent>
		StorageAutomationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Combat", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECombatEvadeComponent> CombatEvadeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Configuration", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionConfigDataAsset> CompanionConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Appearance", meta = (AllowPrivateAccess = "true"))
	bool bSoxAndShoesVisible = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Appearance", meta = (AllowPrivateAccess = "true"))
	bool bHoodVisible = true;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle InvulnerableStateChangedDelegateHandle;
	FGameplayAbilitySpecHandle AutonomousEvadeAbilityHandle;
	FActiveGameplayEffectHandle StaminaRegenEffectHandle;
};

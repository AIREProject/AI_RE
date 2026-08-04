#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AIRECompanionCharacter.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECompanionAttributeSet;
class UAIRECompanionChatComponent;
class UAIRECompanionEquipmentComponent;
class UAIRECompanionInventoryComponent;
class UAIRECompanionLocalBehaviorPolicyComponent;
class UAIRECompanionSupportComponent;
class UAIRECompanionWorkOrderComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

UCLASS(Blueprintable)
class AI_RE_API AAIRECompanionCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAIRECompanionCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Abilities")
	const UAIRECompanionAttributeSet* GetCompanionAttributeSet() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Abilities")
	bool IsAbilitySystemDisabled() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Equipment")
	UAIRECompanionEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory")
	UAIRECompanionInventoryComponent* GetInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Support")
	UAIRECompanionSupportComponent* GetSupportComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Policy")
	UAIRECompanionLocalBehaviorPolicyComponent*
		GetLocalBehaviorPolicyComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Chat")
	UAIRECompanionChatComponent* GetChatComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Work")
	UAIRECompanionWorkOrderComponent* GetWorkOrderComponent() const;

	bool ResetAttributesToConfiguredDefaults();

	UFUNCTION(BlueprintPure, Category = "AIRE")
	FString GetCompanionId() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Configuration")
	const UAIRECompanionConfigDataAsset* GetCompanionConfig() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void SynchronizeDeadState(float CurrentHealth);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Abilities", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionAttributeSet> CompanionAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Equipment", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Inventory", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Support", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionSupportComponent> SupportComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Policy", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionLocalBehaviorPolicyComponent>
		LocalBehaviorPolicyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Chat", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionChatComponent> ChatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Work", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UAIRECompanionWorkOrderComponent> WorkOrderComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Configuration", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionConfigDataAsset> CompanionConfig;

	FDelegateHandle HealthChangedDelegateHandle;
};

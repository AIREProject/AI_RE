#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AIRECompanionCharacter.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECompanionAttributeSet;
class UAIRECompanionChatComponent;
class UAIRECompanionEquipmentComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

UCLASS(Blueprintable)
class AI_RE_API AAIRECompanionCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAIRECompanionCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Abilities")
	const UAIRECompanionAttributeSet* GetCompanionAttributeSet() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Abilities")
	bool IsAbilitySystemDisabled() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Equipment")
	UAIRECompanionEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Chat")
	UAIRECompanionChatComponent* GetChatComponent() const;

	bool ResetAttributesToConfiguredDefaults();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion")
	FString GetCompanionId() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Configuration")
	const UAIRECompanionConfigDataAsset* GetCompanionConfig() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void SynchronizeDeadState(float CurrentHealth);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionAttributeSet> CompanionAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Chat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionChatComponent> ChatComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Configuration", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionConfigDataAsset> CompanionConfig;

	FDelegateHandle HealthChangedDelegateHandle;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AIRECompanionSupportComponent.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECompanionInventoryComponent;
class UAbilitySystemComponent;

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionSupportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionSupportComponent();

	bool InitializeSupport(
		const UAIRECompanionConfigDataAsset* CompanionConfig,
		UAIRECompanionInventoryComponent* InInventoryComponent,
		UAbilitySystemComponent* InAbilitySystem);
	void ShutdownSupport();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Support")
	bool RequestSupport(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Support")
	void CancelSupportRequest();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Support")
	AActor* GetSupportTarget() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Support")
	bool IsSupportRequested() const;

	float GetSupportRange() const;
	void CompleteSupportRequest(AActor* TargetActor);
	FGameplayAbilitySpecHandle FindSupportAbilityHandle() const;

protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool GrantSupportAbilities();
	void ReleaseSupportAbilities();
	bool IsPlayerBelowAutoSupportThreshold(AActor* PlayerActor) const;
	void HandleDeadStateChanged(FGameplayTag Tag, int32 NewCount);
	void HandleAttackStateChanged(FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void HandleSupportTargetDestroyed(AActor* DestroyedActor);

	TWeakObjectPtr<UAIRECompanionConfigDataAsset> ActiveConfig;
	TWeakObjectPtr<UAIRECompanionInventoryComponent> InventoryComponent;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<AActor> SupportTarget;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
	FDelegateHandle DeadStateChangedDelegateHandle;
	FDelegateHandle AttackStateChangedDelegateHandle;
	bool bAutoSupportRequestActive = false;
};

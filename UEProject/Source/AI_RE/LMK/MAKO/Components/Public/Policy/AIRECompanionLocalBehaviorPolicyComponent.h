#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LocalAI/Policy/AIRECompanionLocalBehaviorPolicy.h"
#include "AIRECompanionLocalBehaviorPolicyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIRECompanionLocalBehaviorPolicyChangedSignature,
	FAIRECompanionLocalBehaviorPolicy,
	PreviousPolicy,
	FAIRECompanionLocalBehaviorPolicy,
	CurrentPolicy);

UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionLocalBehaviorPolicyComponent final
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionLocalBehaviorPolicyComponent();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Policy")
	FAIRECompanionLocalBehaviorPolicy GetLocalBehaviorPolicy() const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Policy")
	bool SetLocalBehaviorPolicy(
		FAIRECompanionLocalBehaviorPolicy NewPolicy);

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Policy")
	FAIRECompanionLocalBehaviorPolicyChangedSignature
		OnLocalBehaviorPolicyChanged;

protected:
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;

private:
	UPROPERTY(Transient)
	FAIRECompanionLocalBehaviorPolicy CurrentPolicy;

	bool bIsInitialized = false;
};

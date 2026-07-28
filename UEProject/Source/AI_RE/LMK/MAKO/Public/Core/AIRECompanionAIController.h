#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AIRECompanionAIController.generated.h"

class AAIRECompanionCharacter;
class UAIRECompanionThreatComponent;
class UStateTreeAIComponent;

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EAIRECompanionTestBehaviorRequest : uint8
{
	None = 0 UMETA(Hidden),
	Disabled = 0x01,
	Survival = 0x02,
	DirectCommand = 0x08,
	Work = 0x10
};
ENUM_CLASS_FLAGS(EAIRECompanionTestBehaviorRequest);

UCLASS(Blueprintable)
class AI_RE_API AAIRECompanionAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAIRECompanionAIController();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing")
	void SetTestBehaviorRequest(EAIRECompanionTestBehaviorRequest Request, bool bIsRequested);

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Testing")
	bool IsTestBehaviorRequestActive(EAIRECompanionTestBehaviorRequest Request) const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing")
	void ClearTestBehaviorRequests();

	UAIRECompanionThreatComponent* GetThreatComponent() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResetCompanionState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Threat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIRECompanionThreatComponent> ThreatComponent;

	UPROPERTY(Transient)
	EAIRECompanionTestBehaviorRequest ActiveTestBehaviorRequests = EAIRECompanionTestBehaviorRequest::None;

	TWeakObjectPtr<AAIRECompanionCharacter> CompanionCharacter;
};

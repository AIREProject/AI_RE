#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AI_REHarvestDamageTarget.h"
#include "AI_REHarvestableResourceActor.generated.h"

class UStaticMeshComponent;
class UAI_REHarvestableResourceComponent;

UCLASS()
class AI_RE_API AAI_REHarvestableResourceActor : public AActor, public IAI_REHarvestDamageTarget
{
	GENERATED_BODY()

public:
	AAI_REHarvestableResourceActor();

	virtual bool ApplyHarvestDamage_Implementation(float DamageAmount, AActor* InstigatorActor) override;

	UFUNCTION(BlueprintPure, Category = "AI_RE|Harvest")
	UAI_REHarvestableResourceComponent* GetHarvestableResourceComponent() const { return ResourceComponent; }

	// 스폰할 아이템 액터 클래스 (블루프린트에서 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI_RE|Harvest")
	TSubclassOf<class AAI_REItemActor> ItemActorClass;

	UFUNCTION(BlueprintNativeEvent, Category = "AI_RE|Harvest")
	void ApplyDepletedVisualState(bool bNewIsDepleted);
	
protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI_RE|Harvest")
	TObjectPtr<UStaticMeshComponent> ResourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI_RE|Harvest")
	TObjectPtr<UAI_REHarvestableResourceComponent> ResourceComponent;

private:
	UFUNCTION()
	void HandleDepletedStateChanged(bool bNewIsDepleted);

	UFUNCTION()
	void HandleHarvested(AActor* InstigatorActor, float AppliedDamage, float CurrentHealth, FName RewardName, int32 GrantedRewardAmount);

};

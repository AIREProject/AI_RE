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

	bool TryGetHarvestInteractionLocation(
		const FVector& FromLocation,
		FVector& OutInteractionLocation) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Harvest", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float HarvestInteractionRadius = 50.0f;

	// 스폰할 아이템 액터 클래스 (블루프린트에서 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI_RE|Harvest")
	TSubclassOf<class AAI_REItemActor> ItemActorClass;

	UFUNCTION(BlueprintNativeEvent, Category = "AI_RE|Harvest")
	void ApplyDepletedVisualState(bool bNewIsDepleted);

	// 피격 시 시각적 효과(흔들림 등)를 재생하기 위한 이벤트 (블루프린트에서 타임라인 등으로 구현)
	UFUNCTION(BlueprintImplementableEvent, Category = "AI_RE|Harvest")
	void PlayHitReactVisual(AActor* InstigatorActor, float DamageAmount);
	
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
	void HandleHarvested(AActor* InstigatorActor, float AppliedDamage, float CurrentHealth, class UAI_REItemDataAsset* RewardItemAsset, int32 GrantedRewardAmount, FGuid DeliveryId);

	bool SpawnHarvestReward(
		AActor* InstigatorActor,
		class UAI_REItemDataAsset* RewardItemAsset,
		int32 GrantedRewardAmount,
		const FGuid& DeliveryId);

	TSet<FGuid> SpawnedRewardDeliveries;
};

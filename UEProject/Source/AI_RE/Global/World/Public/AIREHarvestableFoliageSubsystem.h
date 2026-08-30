#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIREHarvestableFoliageSubsystem.generated.h"

class AAIREHarvestableFoliageProxyActor;
class AAI_REItemActor;
class UAIREHarvestableFoliageConfig;
class UAI_REItemDataAsset;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS()
class AI_RE_API UAIREHarvestableFoliageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	struct FActiveProxy
	{
		TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
		int32 InstanceIndex = INDEX_NONE;
		FTransform OriginalTransform;
		TWeakObjectPtr<AAIREHarvestableFoliageProxyActor> Proxy;
	};

	void ScanNearbyInstances();
	bool IsTracked(
		const UHierarchicalInstancedStaticMeshComponent* Component,
		int32 InstanceIndex) const;
	void RestoreProxy(int32 ActiveIndex);

	UPROPERTY(Transient)
	TObjectPtr<UAIREHarvestableFoliageConfig> Config;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> AllowedMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UAI_REItemDataAsset> RewardItem;

	UPROPERTY(Transient)
	TSubclassOf<AAI_REItemActor> DroppedItemClass;

	UPROPERTY(Transient)
	TSubclassOf<AAIREHarvestableFoliageProxyActor> ProxyActorClass;

	TArray<FActiveProxy> ActiveProxies;
	FTimerHandle ScanTimer;
};

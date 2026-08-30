#include "AIREHarvestableFoliageProxyActor.h"

#include "AI_REHarvestGameplayTags.h"
#include "AI_REHarvestableResourceComponent.h"
#include "Components/StaticMeshComponent.h"

void AAIREHarvestableFoliageProxyActor::Configure(
	UStaticMesh* SourceMesh,
	UAI_REItemDataAsset* RewardItem,
	TSubclassOf<AAI_REItemActor> DroppedItemClass)
{
	if (IsValid(ResourceMesh))
	{
		ResourceMesh->SetStaticMesh(SourceMesh);
		ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ResourceMesh->SetCollisionResponseToAllChannels(ECR_Block);
	}
	if (IsValid(ResourceComponent))
	{
		ResourceComponent->SetResourceDefaults(
			AI_REHarvestGameplayTags::Resource_Wood,
			RewardItem,
			1,
			25.0f);
	}
	ItemActorClass = DroppedItemClass;
}

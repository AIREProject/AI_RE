#include "AI_REHarvestableResourceActor.h"
#include "Components/StaticMeshComponent.h"
// Component 헤더 경로는 프로젝트 설정에 맞게 조정될 수 있습니다.
#include "AI_REHarvestableResourceComponent.h" 
#include "AI_REItemActor.h"
#include "AI_REItemDataAsset.h"
#include "Engine/World.h"

AAI_REHarvestableResourceActor::AAI_REHarvestableResourceActor()
{
	// bReplicates = true; // 멀티플레이 속성 제거
	// SetReplicateMovement(false);

	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	SetRootComponent(ResourceMesh);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ResourceMesh->SetCollisionResponseToAllChannels(ECR_Block);

	ResourceComponent = CreateDefaultSubobject<UAI_REHarvestableResourceComponent>(TEXT("ResourceComponent"));
}

bool AAI_REHarvestableResourceActor::ApplyHarvestDamage_Implementation(float DamageAmount, AActor* InstigatorActor)
{
	return ResourceComponent != nullptr && ResourceComponent->ApplyHarvestDamage(DamageAmount, InstigatorActor);
}

// void AAI_REHarvestableResourceActor::ApplyDepletedVisualState_Implementation(bool bNewIsDepleted)
// {
// }

void AAI_REHarvestableResourceActor::BeginPlay()
{
	Super::BeginPlay();

	if (ResourceComponent != nullptr)
	{
		ResourceComponent->OnDepletedStateChanged.AddDynamic(this, &AAI_REHarvestableResourceActor::HandleDepletedStateChanged);
		ResourceComponent->OnHarvested.AddDynamic(this, &AAI_REHarvestableResourceActor::HandleHarvested);
		ApplyDepletedVisualState(ResourceComponent->IsDepleted());
	}
}

void AAI_REHarvestableResourceActor::HandleDepletedStateChanged(bool bNewIsDepleted)
{
	ApplyDepletedVisualState(bNewIsDepleted);
}

void AAI_REHarvestableResourceActor::HandleHarvested(AActor* InstigatorActor, float AppliedDamage, float CurrentHealth, class UAI_REItemDataAsset* RewardItemAsset, int32 GrantedRewardAmount)
{
	if (GrantedRewardAmount > 0 && ItemActorClass && RewardItemAsset)
	{
		// 플레이어 약간 앞이나 주변에 스폰
		FVector SpawnLocation = GetActorLocation() + FVector(0.f, 0.f, 50.f) + FMath::VRand() * 50.f;
		
		FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
		if (AAI_REItemActor* SpawnedItem = GetWorld()->SpawnActorDeferred<AAI_REItemActor>(ItemActorClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			SpawnedItem->ItemAsset = RewardItemAsset;
			SpawnedItem->ItemCount = GrantedRewardAmount;
			SpawnedItem->FinishSpawning(SpawnTransform);
		}
	}
}

void AAI_REHarvestableResourceActor::ApplyDepletedVisualState_Implementation(bool bNewIsDepleted)
{
	if (ResourceMesh == nullptr)
	{
		return;
	}

	ResourceMesh->SetHiddenInGame(bNewIsDepleted);
	ResourceMesh->SetVisibility(!bNewIsDepleted, true);
	ResourceMesh->SetCollisionEnabled(bNewIsDepleted ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

#include "AIRESharedWarehouseActor.h"

#include "AIREGameplayInventorySubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRESharedWarehouse, Log, All);

AAIRESharedWarehouseActor::AAIRESharedWarehouseActor()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(
		TEXT("InteractionCollision"));
	InteractionCollision->SetBoxExtent(FVector(75.0f, 75.0f, 75.0f));
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block);
	SetRootComponent(InteractionCollision);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(InteractionCollision);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AAIRESharedWarehouseActor::Interact_Implementation(AActor* Interactor)
{
	if (!IsValid(Interactor))
	{
		return;
	}

	FAIREInventoryContainerSnapshot WarehouseSnapshot;
	if (!GetWarehouseSnapshot(WarehouseSnapshot))
	{
		UE_LOG(
			LogAIRESharedWarehouse,
			Warning,
			TEXT("Shared warehouse interaction failed to resolve its inventory snapshot. Actor=%s Interactor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Interactor));
		return;
	}

	UE_LOG(
		LogAIRESharedWarehouse,
		Log,
		TEXT("Shared warehouse opened. Actor=%s Interactor=%s Session=%s Revision=%lld Capacity=%d Stacks=%d"),
		*GetNameSafe(this),
		*GetNameSafe(Interactor),
		*WarehouseSnapshot.SessionId.ToString(),
		WarehouseSnapshot.Revision,
		WarehouseSnapshot.Capacity,
		WarehouseSnapshot.ItemStacks.Num());
	OnWarehouseOpened(Interactor, WarehouseSnapshot);
}

bool AAIRESharedWarehouseActor::GetWarehouseSnapshot(
	FAIREInventoryContainerSnapshot& OutSnapshot) const
{
	OutSnapshot = FAIREInventoryContainerSnapshot();
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World)
		? World->GetGameInstance()
		: nullptr;
	UAIREGameplayInventorySubsystem* InventorySubsystem =
		IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
		: nullptr;
	return IsValid(InventorySubsystem)
		&& InventorySubsystem->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetSharedWarehouseContainerId(),
			OutSnapshot);
}

#include "AIRESharedStorageActor.h"

#include "AIREGameplayInventorySubsystem.h"
#include "Inventory/UI/AIREInventoryUIWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRESharedStorage, Log, All);

AAIRESharedStorageActor::AAIRESharedStorageActor()
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

	CompanionInteractionPoint = CreateDefaultSubobject<USceneComponent>(
		TEXT("CompanionInteractionPoint"));
	check(CompanionInteractionPoint);
	CompanionInteractionPoint->SetupAttachment(InteractionCollision);
	CompanionInteractionPoint->SetRelativeLocation(
		FVector(150.0f, 0.0f, 0.0f));
	CompanionInteractionPoint->SetRelativeRotation(
		FRotator(0.0f, 180.0f, 0.0f));
}

void AAIRESharedStorageActor::Interact_Implementation(AActor* Interactor)
{
	if (!IsValid(Interactor))
	{
		return;
	}

	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!GetStorageSnapshot(StorageSnapshot))
	{
		UE_LOG(
			LogAIRESharedStorage,
			Warning,
			TEXT("Shared storage interaction failed to resolve its inventory snapshot. Actor=%s Interactor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Interactor));
		return;
	}

	UE_LOG(
		LogAIRESharedStorage,
		Log,
		TEXT("Shared storage opened. Actor=%s Interactor=%s Session=%s Revision=%lld Capacity=%d Stacks=%d"),
		*GetNameSafe(this),
		*GetNameSafe(Interactor),
		*StorageSnapshot.SessionId.ToString(),
		StorageSnapshot.Revision,
		StorageSnapshot.Capacity,
		StorageSnapshot.ItemStacks.Num());
	UWorld* World = GetWorld();
	if (UAIREInventoryUIWorldSubsystem* InventoryUI =
			IsValid(World)
				? World->GetSubsystem<UAIREInventoryUIWorldSubsystem>()
				: nullptr;
		IsValid(InventoryUI))
	{
		InventoryUI->OpenStorageInventory(this, Interactor);
	}
	OnStorageOpened(Interactor, StorageSnapshot);
}

bool AAIRESharedStorageActor::GetStorageSnapshot(
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
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			OutSnapshot);
}

FTransform AAIRESharedStorageActor::GetCompanionInteractionTransform() const
{
	return IsValid(CompanionInteractionPoint)
		? CompanionInteractionPoint->GetComponentTransform()
		: GetActorTransform();
}

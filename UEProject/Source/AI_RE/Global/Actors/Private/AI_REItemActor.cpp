#include "AI_REItemActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AI_RECharacter.h"
#include "AI_REPlayerInventoryComponent.h"
#include "../../OBI/Component/Public/AI_REItemDataAsset.h"

AAI_REItemActor::AAI_REItemActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	RootComponent = SphereComponent;
	SphereComponent->SetSphereRadius(100.f);
	SphereComponent->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAI_REItemActor::BeginPlay()
{
	Super::BeginPlay();
}

void AAI_REItemActor::Interact_Implementation(AActor* Interactor)
{
	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("ItemActor Interact Called!"));

	if (ItemAsset == nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("ItemAsset is NULL! Did you assign it in Blueprint?"));
		return;
	}

	if (AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(Interactor))
	{
		if (UAI_REPlayerInventoryComponent* InvComp = PlayerChar->GetInventoryComponent())
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("Trying to add item: %s"), *ItemAsset->ItemId.ToString()));
			if (InvComp->AddItem(ItemAsset->ItemId, ItemCount))
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Item Added to Inventory!"));
				Destroy();
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("AddItem Failed! Is inventory full or ItemId None?"));
			}
		}
	}
}

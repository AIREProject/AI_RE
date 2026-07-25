#include "AI_REItemActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "../../ThirdPerson/AI_RECharacter.h"
#include "../../Component/Public/AI_REPlayerInventoryComponent.h"

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
	if (AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(Interactor))
	{
		if (UAI_REPlayerInventoryComponent* InvComp = PlayerChar->GetInventoryComponent())
		{
			if (InvComp->AddItem(ItemId, ItemCount))
			{
				Destroy();
			}
		}
	}
}

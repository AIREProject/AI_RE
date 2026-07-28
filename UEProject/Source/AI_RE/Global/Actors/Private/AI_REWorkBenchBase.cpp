// Copyright MixUpProject. All Rights Reserved.

#include "AI_REWorkBenchBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "AI_RECharacter.h"

AAI_REWorkBenchBase::AAI_REWorkBenchBase()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	
	// Default to None, child classes will override this
	WorkbenchType = EWorkbenchType::None;
}

void AAI_REWorkBenchBase::BeginPlay()
{
	Super::BeginPlay();
}

void AAI_REWorkBenchBase::Interact_Implementation(AActor* Interactor)
{
	if (AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(Interactor))
	{
		PlayerCharacter->OpenCraftingUI(WorkbenchType);
	}
}

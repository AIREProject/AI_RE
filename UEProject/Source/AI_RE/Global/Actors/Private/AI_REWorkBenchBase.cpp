// Copyright MixUpProject. All Rights Reserved.

#include "AI_REWorkBenchBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "AI_RECharacter.h"
#include "../../Global/Tags/Public/AI_REWorkbenchGameplayTags.h"

AAI_REWorkBenchBase::AAI_REWorkBenchBase()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(SceneRoot);
	
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
		// 태그를 우선 확인하여 WorkbenchType 갱신 (블루프린트에서 태그만 설정했을 경우 대응)
		if (WorkbenchTags.HasTagExact(AI_REWorkbenchGameplayTags::Workbench_Alchemy))
		{
			WorkbenchType = EWorkbenchType::Alchemy;
		}
		else if (WorkbenchTags.HasTagExact(AI_REWorkbenchGameplayTags::Workbench_Blacksmith))
		{
			WorkbenchType = EWorkbenchType::Blacksmith;
		}
		else if (WorkbenchTags.HasTagExact(AI_REWorkbenchGameplayTags::Workbench_Smelter))
		{
			WorkbenchType = EWorkbenchType::Smelter;
		}
		else if (WorkbenchTags.HasTagExact(AI_REWorkbenchGameplayTags::Workbench_Cook))
		{
			WorkbenchType = EWorkbenchType::Cook;
		}
		else if (WorkbenchTags.HasTagExact(AI_REWorkbenchGameplayTags::Workbench_Basic))
		{
			WorkbenchType = EWorkbenchType::Basic;
		}

		PlayerCharacter->OpenCraftingUI(WorkbenchType);
	}
}

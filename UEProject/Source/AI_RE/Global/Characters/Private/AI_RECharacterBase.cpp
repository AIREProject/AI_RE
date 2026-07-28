// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECharacterBase.h"
#include "AI_REStatusComponent.h"
#include "AI_RESkillComponent.h" // Assuming this is where it's located currently

AAI_RECharacterBase::AAI_RECharacterBase()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 컴포넌트 초기화
	StatusComponent = CreateDefaultSubobject<UAI_REStatusComponent>(TEXT("StatusComponent"));
	SkillComponent = CreateDefaultSubobject<UAI_RESkillComponent>(TEXT("SkillComponent"));
}

void AAI_RECharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

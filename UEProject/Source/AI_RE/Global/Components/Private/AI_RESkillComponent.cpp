#include "AI_RESkillComponent.h"

#include "Engine/World.h"

UAI_RESkillComponent::UAI_RESkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAI_RESkillComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAI_RESkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAI_RESkillComponent::MarkSkillAsUsed(int32 SkillSlot)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();

    if (SkillSlot == 0) LightSkill.LastUseTime = CurrentTime;
    else if (SkillSlot == 1) MediumSkill.LastUseTime = CurrentTime;
    else if (SkillSlot == 2) HeavySkill.LastUseTime = CurrentTime;
}

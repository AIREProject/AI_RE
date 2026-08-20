// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECharacterBase.h"
#include "AI_REStatusComponent.h"
#include "AI_RESkillComponent.h" // Assuming this is where it's located currently
#include "AbilitySystemComponent.h"
#include "AI_REAttributeSet.h"

AAI_RECharacterBase::AAI_RECharacterBase()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 컴포넌트 초기화 (블루프린트 CDO 꼬임 방지를 위해 이름 변경)
	StatusComponent = CreateDefaultSubobject<UAI_REStatusComponent>(TEXT("BaseStatusComponent"));
	SkillComponent = CreateDefaultSubobject<UAI_RESkillComponent>(TEXT("BaseSkillComponent"));
	
	// GAS 컴포넌트 초기화
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UAI_REAttributeSet>(TEXT("AttributeSet"));
}

void AAI_RECharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

UAbilitySystemComponent* AAI_RECharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FGameplayAttribute AAI_RECharacterBase::GetCombatHealthAttribute() const
{
	return UAI_REAttributeSet::GetHPAttribute();
}

EAIRECombatAffiliation AAI_RECharacterBase::GetCombatAffiliation() const
{
	return EAIRECombatAffiliation::PlayerParty;
}

FGameplayAttribute AAI_RECharacterBase::GetHealingHealthAttribute() const
{
	return UAI_REAttributeSet::GetHPAttribute();
}

FGameplayAttribute AAI_RECharacterBase::GetHealingMaxHealthAttribute() const
{
	return UAI_REAttributeSet::GetMaxHPAttribute();
}

bool AAI_RECharacterBase::CanReceiveHealingFrom(
	const AActor* Healer) const
{
	const IAIRECombatDamageTargetInterface* HealerCombatTarget =
		Cast<IAIRECombatDamageTargetInterface>(Healer);
	return GetCombatAffiliation() == EAIRECombatAffiliation::PlayerParty
		&& IsValid(Healer)
		&& Healer != this
		&& HealerCombatTarget
		&& HealerCombatTarget->GetCombatAffiliation()
			== EAIRECombatAffiliation::PlayerParty
		&& IsCombatTargetAlive();
}

void AAI_RECharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 오너 액터와 아바타 액터 초기화 (서버/싱글플레이어 환경)
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

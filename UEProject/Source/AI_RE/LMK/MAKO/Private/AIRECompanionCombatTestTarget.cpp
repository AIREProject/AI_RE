#include "AIRECompanionCombatTestTarget.h"

#include "AbilitySystemComponent.h"
#include "AIRECompanionAttributeSet.h"
#include "AIRECompanionGameplayTags.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

AAIRECompanionCombatTestTarget::AAIRECompanionCombatTestTarget()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(
		TEXT("AbilitySystem"));
	check(AbilitySystemComponent);
	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UAIRECompanionAttributeSet>(
		TEXT("Attributes"));
	check(AttributeSet);

	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(
		TEXT("StimuliSource"));
	check(StimuliSource);
}

UAbilitySystemComponent*
AAIRECompanionCombatTestTarget::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

bool AAIRECompanionCombatTestTarget::IsHostileThreatFor_Implementation(
	const AActor* Observer) const
{
	return bIsHostile && IsValid(Observer) && Observer != this;
}

bool AAIRECompanionCombatTestTarget::IsAliveThreatTarget_Implementation() const
{
	return IsValid(AbilitySystemComponent)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateDisabledDead);
}

const UAIRECompanionAttributeSet*
AAIRECompanionCombatTestTarget::GetTestAttributeSet() const
{
	return AttributeSet;
}

void AAIRECompanionCombatTestTarget::BeginPlay()
{
	Super::BeginPlay();

	check(AbilitySystemComponent);
	check(AttributeSet);
	check(StimuliSource);
	const float ValidInitialHealth =
		FMath::IsFinite(InitialHealth) && InitialHealth > 0.0f
			? InitialHealth
			: 100.0f;
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetMaxHealthAttribute(),
		ValidInitialHealth);
	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetHealthAttribute(),
		ValidInitialHealth);
	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetMaxStaminaAttribute(),
		1.0f);
	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetStaminaAttribute(),
		0.0f);
	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(
			UAIRECompanionAttributeSet::GetHealthAttribute())
		.AddUObject(
			this,
			&AAIRECompanionCombatTestTarget::HandleHealthChanged);
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	SynchronizeDeadState(AttributeSet->GetHealth());
}

void AAIRECompanionCombatTestTarget::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(StimuliSource))
	{
		StimuliSource->UnregisterFromPerceptionSystem();
	}

	if (IsValid(AbilitySystemComponent))
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(
					UAIRECompanionAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedDelegateHandle);
			HealthChangedDelegateHandle.Reset();
		}

		AbilitySystemComponent->ClearActorInfo();
	}

	Super::EndPlay(EndPlayReason);
}

void AAIRECompanionCombatTestTarget::HandleHealthChanged(
	const FOnAttributeChangeData& ChangeData)
{
	SynchronizeDeadState(ChangeData.NewValue);
}

void AAIRECompanionCombatTestTarget::SynchronizeDeadState(
	const float CurrentHealth)
{
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent->SetLooseGameplayTagCount(
		AIRECompanionGameplayTags::StateDisabledDead,
		CurrentHealth <= 0.0f ? 1 : 0);
}

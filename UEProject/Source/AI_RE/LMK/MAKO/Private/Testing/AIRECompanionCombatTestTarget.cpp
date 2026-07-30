#include "Testing/AIRECompanionCombatTestTarget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
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

FGameplayAttribute
AAIRECompanionCombatTestTarget::GetHealingHealthAttribute() const
{
	return UAIRECompanionAttributeSet::GetHealthAttribute();
}

FGameplayAttribute
AAIRECompanionCombatTestTarget::GetHealingMaxHealthAttribute() const
{
	return UAIRECompanionAttributeSet::GetMaxHealthAttribute();
}

bool AAIRECompanionCombatTestTarget::CanReceiveHealingFrom(
	const AActor* Healer) const
{
	return !bIsHostile
		&& IsValid(Healer)
		&& Healer != this
		&& IsAliveThreatTarget_Implementation();
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
	const float ValidMaxHealth =
		FMath::IsFinite(MaxHealth) && MaxHealth > 0.0f
			? MaxHealth
			: 100.0f;
	const float ValidInitialHealth = FMath::Clamp(
		FMath::IsFinite(InitialHealth) ? InitialHealth : ValidMaxHealth,
		0.0f,
		ValidMaxHealth);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetMaxHealthAttribute(),
		ValidMaxHealth);
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

#include "AIREEnemyVitalityComponent.h"

#include "AbilitySystemComponent.h"
#include "AI_REAttributeSet.h"
#include "AIRECombatGameplayTags.h"

UAIREEnemyVitalityComponent::UAIREEnemyVitalityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIREEnemyVitalityComponent::InitializeVitality(
	UAbilitySystemComponent* InAbilitySystem)
{
	ShutdownVitality();
	if (!IsValid(InAbilitySystem)
		|| !InAbilitySystem->HasAttributeSetForAttribute(
			UAI_REAttributeSet::GetHPAttribute())
		|| !InAbilitySystem->HasAttributeSetForAttribute(
			UAI_REAttributeSet::GetMaxHPAttribute())
		|| !FMath::IsFinite(MaxHealth)
		|| !FMath::IsFinite(InitialHealth)
		|| MaxHealth <= 0.0f
		|| InitialHealth < 0.0f)
	{
		return false;
	}

	AbilitySystem = InAbilitySystem;
	InAbilitySystem->SetNumericAttributeBase(
		UAI_REAttributeSet::GetMaxHPAttribute(),
		MaxHealth);
	InAbilitySystem->SetNumericAttributeBase(
		UAI_REAttributeSet::GetHPAttribute(),
		FMath::Min(InitialHealth, MaxHealth));
	HealthChangedDelegateHandle = InAbilitySystem
		->GetGameplayAttributeValueChangeDelegate(
			UAI_REAttributeSet::GetHPAttribute())
		.AddUObject(this, &UAIREEnemyVitalityComponent::HandleHealthChanged);
	bDead = false;
	SynchronizeDeath(InAbilitySystem->GetNumericAttribute(
		UAI_REAttributeSet::GetHPAttribute()));
	return true;
}

void UAIREEnemyVitalityComponent::ShutdownVitality()
{
	if (AbilitySystem.IsValid() && HealthChangedDelegateHandle.IsValid())
	{
		AbilitySystem
			->GetGameplayAttributeValueChangeDelegate(
				UAI_REAttributeSet::GetHPAttribute())
			.Remove(HealthChangedDelegateHandle);
	}
	HealthChangedDelegateHandle.Reset();
	AbilitySystem.Reset();
	OnHealthChanged.Clear();
	OnDeath.Clear();
}

void UAIREEnemyVitalityComponent::ConfigureDefaults(
	const float InMaxHealth,
	const float InInitialHealth)
{
	if (FMath::IsFinite(InMaxHealth) && InMaxHealth > 0.0f)
	{
		MaxHealth = InMaxHealth;
	}
	if (FMath::IsFinite(InInitialHealth) && InInitialHealth >= 0.0f)
	{
		InitialHealth = FMath::Min(InInitialHealth, MaxHealth);
	}
}

FAIREEnemyVitalitySnapshot
UAIREEnemyVitalityComponent::GetVitalitySnapshot() const
{
	FAIREEnemyVitalitySnapshot Snapshot;
	if (AbilitySystem.IsValid())
	{
		Snapshot.Health = AbilitySystem->GetNumericAttribute(
			UAI_REAttributeSet::GetHPAttribute());
		Snapshot.MaxHealth = AbilitySystem->GetNumericAttribute(
			UAI_REAttributeSet::GetMaxHPAttribute());
	}
	Snapshot.bDead = bDead;
	return Snapshot;
}

bool UAIREEnemyVitalityComponent::IsDead() const
{
	return bDead;
}

void UAIREEnemyVitalityComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownVitality();
	Super::EndPlay(EndPlayReason);
}

void UAIREEnemyVitalityComponent::HandleHealthChanged(
	const FOnAttributeChangeData& ChangeData)
{
	OnHealthChanged.Broadcast(ChangeData.OldValue, ChangeData.NewValue);
	SynchronizeDeath(ChangeData.NewValue);
}

void UAIREEnemyVitalityComponent::SynchronizeDeath(
	const float CurrentHealth)
{
	if (!AbilitySystem.IsValid())
	{
		return;
	}

	const bool bShouldBeDead = CurrentHealth <= 0.0f;
	AbilitySystem->SetLooseGameplayTagCount(
		AIRECombatGameplayTags::StateDead,
		bShouldBeDead ? 1 : 0);
	if (!bShouldBeDead || bDead)
	{
		return;
	}

	bDead = true;
	AbilitySystem->CancelAllAbilities();
	OnDeath.Broadcast(GetOwner());
}

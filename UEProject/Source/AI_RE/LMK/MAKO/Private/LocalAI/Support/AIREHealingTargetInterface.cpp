#include "LocalAI/Support/AIREHealingTargetInterface.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

bool AIREHealingTarget::GetMissingHealth(
	const AActor* TargetActor,
	const AActor* Healer,
	float& OutMissingHealth)
{
	OutMissingHealth = 0.0f;
	if (!IsValid(TargetActor)
		|| TargetActor->IsActorBeingDestroyed()
		|| !IsValid(Healer)
		|| !TargetActor->GetClass()->ImplementsInterface(
			UAIREHealingTargetInterface::StaticClass()))
	{
		return false;
	}

	const IAIREHealingTargetInterface* HealingTarget =
		Cast<IAIREHealingTargetInterface>(TargetActor);
	if (!HealingTarget || !HealingTarget->CanReceiveHealingFrom(Healer))
	{
		return false;
	}

	const UAbilitySystemComponent* TargetAbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(
			TargetActor,
			true);
	const FGameplayAttribute HealthAttribute =
		HealingTarget->GetHealingHealthAttribute();
	const FGameplayAttribute MaxHealthAttribute =
		HealingTarget->GetHealingMaxHealthAttribute();
	if (!IsValid(TargetAbilitySystem)
		|| !HealthAttribute.IsValid()
		|| !MaxHealthAttribute.IsValid()
		|| !TargetAbilitySystem->HasAttributeSetForAttribute(
			HealthAttribute)
		|| !TargetAbilitySystem->HasAttributeSetForAttribute(
			MaxHealthAttribute))
	{
		return false;
	}

	const float Health =
		TargetAbilitySystem->GetNumericAttribute(HealthAttribute);
	const float MaxHealth =
		TargetAbilitySystem->GetNumericAttribute(MaxHealthAttribute);
	if (!FMath::IsFinite(Health)
		|| !FMath::IsFinite(MaxHealth)
		|| Health <= 0.0f
		|| MaxHealth <= 0.0f
		|| Health >= MaxHealth)
	{
		return false;
	}

	OutMissingHealth = MaxHealth - Health;
	return OutMissingHealth > 0.0f;
}

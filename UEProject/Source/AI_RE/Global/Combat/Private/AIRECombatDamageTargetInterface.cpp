#include "AIRECombatDamageTargetInterface.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

FGameplayAttribute IAIRECombatDamageTargetInterface::GetCombatFlinchAttribute() const
{
	return FGameplayAttribute();
}

FGameplayAttribute IAIRECombatDamageTargetInterface::GetCombatStunAttribute() const
{
	return FGameplayAttribute();
}

bool IAIRECombatDamageTargetInterface::CanReceiveCombatDamageFrom(
	const AActor* Source) const
{
	const AActor* TargetActor = Cast<AActor>(this);
	if (!IsValid(Source)
		|| !IsValid(TargetActor)
		|| Source == TargetActor
		|| !Source->GetClass()->ImplementsInterface(
			UAIRECombatDamageTargetInterface::StaticClass()))
	{
		return false;
	}

	const IAIRECombatDamageTargetInterface* SourceCombatant =
		Cast<IAIRECombatDamageTargetInterface>(Source);
	return SourceCombatant
		&& SourceCombatant->GetCombatAffiliation() != GetCombatAffiliation();
}

bool IAIRECombatDamageTargetInterface::IsCombatTargetAlive() const
{
	return AIRECombatDamageTarget::IsAlive(Cast<AActor>(this));
}

void IAIRECombatDamageTargetInterface::NotifyCombatDamageApplied(
	const FAIRECombatDamageRequest& Request)
{
	(void)Request;
}

bool AIRECombatDamageTarget::ResolveAbilitySystemAndHealth(
	const AActor* TargetActor,
	UAbilitySystemComponent*& OutAbilitySystem,
	FGameplayAttribute& OutHealthAttribute)
{
	OutAbilitySystem = nullptr;
	OutHealthAttribute = FGameplayAttribute();
	if (!IsValid(TargetActor)
		|| TargetActor->IsActorBeingDestroyed()
		|| !TargetActor->GetClass()->ImplementsInterface(
			UAIRECombatDamageTargetInterface::StaticClass()))
	{
		return false;
	}

	const IAIRECombatDamageTargetInterface* CombatTarget =
		Cast<IAIRECombatDamageTargetInterface>(TargetActor);
	if (!CombatTarget)
	{
		return false;
	}

	OutAbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(
			TargetActor,
			true);
	OutHealthAttribute = CombatTarget->GetCombatHealthAttribute();
	return IsValid(OutAbilitySystem)
		&& OutHealthAttribute.IsValid()
		&& OutAbilitySystem->HasAttributeSetForAttribute(OutHealthAttribute);
}

bool AIRECombatDamageTarget::IsAlive(const AActor* TargetActor)
{
	UAbilitySystemComponent* AbilitySystem = nullptr;
	FGameplayAttribute HealthAttribute;
	if (!ResolveAbilitySystemAndHealth(
		TargetActor,
		AbilitySystem,
		HealthAttribute))
	{
		return false;
	}

	const float Health = AbilitySystem->GetNumericAttribute(HealthAttribute);
	return FMath::IsFinite(Health) && Health > 0.0f;
}

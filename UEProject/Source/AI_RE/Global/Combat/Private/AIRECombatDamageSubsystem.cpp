#include "AIRECombatDamageSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIRECombatDamageGameplayEffect.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AIRECombatGameplayTags.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 MaxAppliedExecutionHistory = 4096;
}

EAIRECombatDamageResult UAIRECombatDamageSubsystem::ApplyDamageRequest(
	const FAIRECombatDamageRequest& Request)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return EAIRECombatDamageResult::InvalidWorld;
	}

	AActor* Source = Request.Source.Get();
	AActor* Target = Request.Target.Get();
	if (!IsValid(Source) || Source->IsActorBeingDestroyed()
		|| Source->GetWorld() != World)
	{
		return EAIRECombatDamageResult::InvalidSource;
	}
	if (!IsValid(Target) || Target->IsActorBeingDestroyed()
		|| Target->GetWorld() != World)
	{
		return EAIRECombatDamageResult::InvalidTarget;
	}
	if (Source == Target)
	{
		return EAIRECombatDamageResult::SelfTarget;
	}
	if (!Source->GetClass()->ImplementsInterface(
		UAIRECombatDamageTargetInterface::StaticClass()))
	{
		return EAIRECombatDamageResult::InvalidSource;
	}
	const IAIRECombatDamageTargetInterface* SourceCombatant =
		Cast<IAIRECombatDamageTargetInterface>(Source);
	if (!SourceCombatant || !SourceCombatant->IsCombatTargetAlive())
	{
		return EAIRECombatDamageResult::SourceDead;
	}
	if (!Request.ExecutionId.IsValid())
	{
		return EAIRECombatDamageResult::InvalidExecutionId;
	}
	if (!FMath::IsFinite(Request.Damage)
		|| !FMath::IsFinite(Request.StaggerValue)
		|| Request.Damage < 0.0f
		|| Request.StaggerValue < 0.0f
		|| (Request.Damage <= 0.0f && Request.StaggerValue <= 0.0f))
	{
		return EAIRECombatDamageResult::InvalidMagnitude;
	}
	if (!Target->GetClass()->ImplementsInterface(
		UAIRECombatDamageTargetInterface::StaticClass()))
	{
		return EAIRECombatDamageResult::UnsupportedTarget;
	}

	IAIRECombatDamageTargetInterface* CombatTarget =
		Cast<IAIRECombatDamageTargetInterface>(Target);
	if (!CombatTarget
		|| !CombatTarget->CanReceiveCombatDamageFrom(Source))
	{
		return EAIRECombatDamageResult::UnsupportedTarget;
	}

	UAbilitySystemComponent* SourceAbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, true);
	if (!IsValid(SourceAbilitySystem))
	{
		return EAIRECombatDamageResult::MissingSourceAbilitySystem;
	}

	UAbilitySystemComponent* TargetAbilitySystem = nullptr;
	FGameplayAttribute HealthAttribute;
	if (!AIRECombatDamageTarget::ResolveAbilitySystemAndHealth(
		Target,
		TargetAbilitySystem,
		HealthAttribute))
	{
		return EAIRECombatDamageResult::MissingTargetAbilitySystem;
	}
	if (!CombatTarget->IsCombatTargetAlive())
	{
		return EAIRECombatDamageResult::TargetDead;
	}
	const FGameplayAttribute FlinchAttribute =
		CombatTarget->GetCombatFlinchAttribute();
	const FGameplayAttribute StunAttribute =
		CombatTarget->GetCombatStunAttribute();
	const bool bSupportsRequestedStagger = Request.StaggerValue <= 0.0f
		|| (FlinchAttribute.IsValid()
			&& TargetAbilitySystem->HasAttributeSetForAttribute(FlinchAttribute))
		|| (StunAttribute.IsValid()
			&& TargetAbilitySystem->HasAttributeSetForAttribute(StunAttribute));
	if (Request.Damage <= 0.0f && !bSupportsRequestedStagger)
	{
		return EAIRECombatDamageResult::UnsupportedTarget;
	}

	PruneExecutionHistory();
	if (HasAppliedExecution(Target, Request.ExecutionId))
	{
		return EAIRECombatDamageResult::DuplicateExecution;
	}
	if (TargetAbilitySystem->HasMatchingGameplayTag(
		AIRECombatGameplayTags::StateInvulnerable))
	{
		// Invulnerability is a terminal resolution for this target-scoped
		// execution. Recording it prevents a delayed duplicate from applying
		// after the short immunity window has closed.
		RecordAppliedExecution(Target, Request.ExecutionId);
		return EAIRECombatDamageResult::TargetInvulnerable;
	}

	FGameplayEffectContextHandle EffectContext =
		SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(Source);
	if (Request.bHasHitResult)
	{
		EffectContext.AddHitResult(Request.HitResult, true);
	}
	FGameplayEffectSpecHandle DamageSpec =
		SourceAbilitySystem->MakeOutgoingSpec(
			UAIRECombatDamageGameplayEffect::StaticClass(),
			1.0f,
			EffectContext);
	if (!DamageSpec.IsValid())
	{
		return EAIRECombatDamageResult::EffectSpecFailed;
	}

	DamageSpec.Data->SetSetByCallerMagnitude(
		AIRECombatGameplayTags::DataDamage,
		Request.Damage);
	DamageSpec.Data->SetSetByCallerMagnitude(
		AIRECombatGameplayTags::DataStagger,
		Request.StaggerValue);
	RecordAppliedExecution(Target, Request.ExecutionId);
	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
			*DamageSpec.Data.Get(),
			TargetAbilitySystem);
	if (!AppliedHandle.WasSuccessfullyApplied())
	{
		RemoveAppliedExecution(Target, Request.ExecutionId);
		return EAIRECombatDamageResult::EffectSpecFailed;
	}

	CombatTarget->NotifyCombatDamageApplied(Request);
	return EAIRECombatDamageResult::Applied;
}

void UAIRECombatDamageSubsystem::Deinitialize()
{
	AppliedExecutionIdsByTarget.Reset();
	AppliedExecutionHistory.Reset();
	Super::Deinitialize();
}

void UAIRECombatDamageSubsystem::PruneExecutionHistory()
{
	for (auto Iterator = AppliedExecutionIdsByTarget.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Key().IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}
	AppliedExecutionHistory.RemoveAll(
		[](const FAppliedExecutionRecord& Record)
		{
			return !Record.Target.IsValid();
		});
}

bool UAIRECombatDamageSubsystem::HasAppliedExecution(
	AActor* Target,
	const FGuid& ExecutionId) const
{
	const TSet<FGuid>* TargetExecutions =
		AppliedExecutionIdsByTarget.Find(Target);
	return TargetExecutions && TargetExecutions->Contains(ExecutionId);
}

void UAIRECombatDamageSubsystem::RecordAppliedExecution(
	AActor* Target,
	const FGuid& ExecutionId)
{
	AppliedExecutionIdsByTarget.FindOrAdd(Target).Add(ExecutionId);
	FAppliedExecutionRecord& Record = AppliedExecutionHistory.AddDefaulted_GetRef();
	Record.Target = Target;
	Record.ExecutionId = ExecutionId;
	while (AppliedExecutionHistory.Num() > MaxAppliedExecutionHistory)
	{
		const FAppliedExecutionRecord Oldest = AppliedExecutionHistory[0];
		AppliedExecutionHistory.RemoveAt(0, 1, EAllowShrinking::No);
		if (Oldest.Target.IsValid())
		{
			RemoveAppliedExecution(Oldest.Target.Get(), Oldest.ExecutionId);
		}
	}
}

void UAIRECombatDamageSubsystem::RemoveAppliedExecution(
	AActor* Target,
	const FGuid& ExecutionId)
{
	AppliedExecutionHistory.RemoveAll(
		[Target, &ExecutionId](const FAppliedExecutionRecord& Record)
		{
			return Record.Target.Get() == Target
				&& Record.ExecutionId == ExecutionId;
		});
	TSet<FGuid>* TargetExecutions = AppliedExecutionIdsByTarget.Find(Target);
	if (!TargetExecutions)
	{
		return;
	}
	TargetExecutions->Remove(ExecutionId);
	if (TargetExecutions->IsEmpty())
	{
		AppliedExecutionIdsByTarget.Remove(Target);
	}
}

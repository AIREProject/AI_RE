#include "AbilitySystem/Combat/Abilities/AIRECompanionCombatSkillAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionCombatSkillCooldownGameplayEffect.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AIRECombatDamageSubsystem.h"
#include "Animation/AnimMontage.h"
#include "Core/AIRECompanionAIController.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Threat/AIRECompanionThreatComponent.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionCombatSkill, Log, All);

UAIRECompanionCombatSkillAbility::UAIRECompanionCombatSkillAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AIRECompanionGameplayTags::AbilityCombatSkill);
	SetAssetTags(AssetTags);

	FAbilityTriggerData& SkillTrigger = AbilityTriggers.AddDefaulted_GetRef();
	SkillTrigger.TriggerTag = AIRECompanionGameplayTags::EventCombatSkillRequest;
	SkillTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;

	ActivationOwnedTags.AddTag(AIRECompanionGameplayTags::StateActionAttacking);
	ActivationOwnedTags.AddTag(
		AIRECompanionGameplayTags::StateActionAttackingSkill);
	ActivationBlockedTags.AddTag(
		AIRECompanionGameplayTags::StateActionAttackingSkill);
	CooldownGameplayEffectClass =
		UAIRECompanionCombatSkillCooldownGameplayEffect::StaticClass();
}

bool UAIRECompanionCombatSkillAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystem =
		ActorInfo->AbilitySystemComponent.Get();
	if (AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttacking)
		&& (!AbilitySystem->HasMatchingGameplayTag(
				AIRECompanionGameplayTags::StateActionAttackingBasic)
			|| !AbilitySystem->HasMatchingGameplayTag(
				AIRECompanionGameplayTags::
					StateActionAttackingSkillCancelable)))
	{
		return false;
	}

	const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition =
		GetWeaponDefinition(Handle, ActorInfo);
	return IsValid(WeaponDefinition)
		&& WeaponDefinition->CombatSkill.bEnabled
		&& Super::CanActivateAbility(
			Handle,
			ActorInfo,
			SourceTags,
			TargetTags,
			OptionalRelevantTags);
}

void UAIRECompanionCombatSkillAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bIsEnding = false;
	bHitConsumed = false;
	bTransitionStarted = false;
	ActiveExecutionId = FGuid::NewGuid();
	ActiveWeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);

	AActor* TargetActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;
	const bool bActivationDataValid =
		InitializeEventTarget(TriggerEventData)
		&& IsValid(ActiveWeaponDefinition)
		&& ActiveWeaponDefinition->CombatSkill.bEnabled
		&& IsTargetValidForSkill(TargetActor)
		&& IsTargetInRange(TargetActor);
	if (!bActivationDataValid
		|| !CommitAbilityCooldown(
			Handle,
			ActorInfo,
			ActivationInfo,
			false))
	{
		UE_LOG(
			LogAIRECompanionCombatSkill,
			Verbose,
			TEXT("Companion combat skill activation rejected. Source=%s Target=%s Weapon=%s"),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
			*GetNameSafe(TargetActor),
			*GetNameSafe(ActiveWeaponDefinition));
		FinishAbility(true);
		return;
	}

	FaceTarget(TargetActor);
	StartHitEventWait();
	if (bIsEnding)
	{
		return;
	}

	bTransitionStarted = true;
	SendTransitionEvent(
		AIRECompanionGameplayTags::EventCombatSkillStarted,
		false);

	UAnimMontage* SkillMontage =
		ActiveWeaponDefinition->CombatSkill.SkillMontage.Get();
	if (!IsValid(SkillMontage))
	{
		StartFallback();
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::
		CreatePlayMontageAndWaitProxy(
			this,
			TEXT("CompanionCombatSkillMontage"),
			SkillMontage);
	if (!IsValid(MontageTask))
	{
		StartFallback();
		return;
	}

	MontageTask->OnCompleted.AddDynamic(
		this,
		&UAIRECompanionCombatSkillAbility::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(
		this,
		&UAIRECompanionCombatSkillAbility::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(
		this,
		&UAIRECompanionCombatSkillAbility::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UAIRECompanionCombatSkillAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	bIsEnding = true;
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (UWorld* World = ActorInfo->AvatarActor->GetWorld())
		{
			World->GetTimerManager().ClearTimer(FallbackHitTimerHandle);
			World->GetTimerManager().ClearTimer(FallbackRecoveryTimerHandle);
		}
	}

	if (bTransitionStarted)
	{
		bTransitionStarted = false;
		SendTransitionEvent(
			AIRECompanionGameplayTags::EventCombatSkillEnded,
			!bWasCancelled);
	}

	UE_LOG(
		LogAIRECompanionCombatSkill,
		Log,
		TEXT("Companion combat skill ended. Source=%s Target=%s Cancelled=%s HitConsumed=%s"),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
		*GetNameSafe(GetEventTarget()),
		bWasCancelled ? TEXT("true") : TEXT("false"),
		bHitConsumed ? TEXT("true") : TEXT("false"));

	MontageTask = nullptr;
	HitEventTask = nullptr;
	ActiveWeaponDefinition = nullptr;
	ActiveExecutionId.Invalidate();
	bHitConsumed = false;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

UAIRECompanionWeaponDefinitionDataAsset*
UAIRECompanionCombatSkillAbility::GetWeaponDefinition(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return nullptr;
	}

	const FGameplayAbilitySpec* AbilitySpec =
		ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	return AbilitySpec
		? Cast<UAIRECompanionWeaponDefinitionDataAsset>(
			AbilitySpec->SourceObject.Get())
		: nullptr;
}

bool UAIRECompanionCombatSkillAbility::IsTargetValidForSkill(
	const AActor* TargetActor) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)
		|| !IsValid(TargetActor)
		|| !TargetActor->GetClass()->ImplementsInterface(
			UAIREThreatTargetInterface::StaticClass()))
	{
		return false;
	}

	return IAIREThreatTargetInterface::Execute_IsAliveThreatTarget(
			const_cast<AActor*>(TargetActor))
		&& IAIREThreatTargetInterface::Execute_IsHostileThreatFor(
			const_cast<AActor*>(TargetActor),
			AvatarActor);
}

bool UAIRECompanionCombatSkillAbility::IsTargetInRange(
	const AActor* TargetActor) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)
		|| !IsValid(TargetActor)
		|| !IsValid(ActiveWeaponDefinition))
	{
		return false;
	}

	const float HorizontalDistance = FVector::Dist2D(
		AvatarActor->GetActorLocation(),
		TargetActor->GetActorLocation());
	const float EffectiveDistance = FMath::Max(
		0.0f,
		HorizontalDistance
			- AvatarActor->GetSimpleCollisionRadius()
			- TargetActor->GetSimpleCollisionRadius());
	return EffectiveDistance
		<= ActiveWeaponDefinition->CombatSkill.AttackRange;
}

void UAIRECompanionCombatSkillAbility::FaceTarget(
	const AActor* TargetActor) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)
		|| !IsValid(TargetActor))
	{
		return;
	}

	FVector TargetDirection =
		TargetActor->GetActorLocation() - AvatarActor->GetActorLocation();
	TargetDirection.Z = 0.0f;
	if (TargetDirection.IsNearlyZero())
	{
		return;
	}

	AvatarActor->SetActorRotation(
		FRotator(
			0.0f,
			TargetDirection.Rotation().Yaw,
			0.0f));
}

bool UAIRECompanionCombatSkillAbility::IsActiveExecutionValid() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (bIsEnding
		|| !IsActive()
		|| !ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !IsValid(ActiveWeaponDefinition)
		|| GetWeaponDefinition(
			GetCurrentAbilitySpecHandle(),
			ActorInfo) != ActiveWeaponDefinition
		|| !ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttackingSkill))
	{
		return false;
	}

	const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	const AAIRECompanionAIController* CompanionController =
		IsValid(AvatarPawn)
			? Cast<AAIRECompanionAIController>(AvatarPawn->GetController())
			: nullptr;
	if (!IsValid(CompanionController))
	{
		return true;
	}

	const UAIRECompanionThreatComponent* ThreatComponent =
		CompanionController->GetThreatComponent();
	return IsValid(ThreatComponent)
		&& ThreatComponent->IsCombatRequested()
		&& ThreatComponent->GetSelectedThreatTarget() == GetEventTarget();
}

bool UAIRECompanionCombatSkillAbility::ResolveSkillHit()
{
	AActor* TargetActor = GetEventTarget();
	if (!IsActiveExecutionValid()
		|| !IsTargetValidForSkill(TargetActor)
		|| !IsTargetInRange(TargetActor)
		|| ActiveWeaponDefinition->CombatSkill.TargetingMode
			!= EAIRECombatTargetingMode::SingleTarget)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UAIRECombatDamageSubsystem* DamageSubsystem = IsValid(World)
		? World->GetSubsystem<UAIRECombatDamageSubsystem>()
		: nullptr;
	if (!IsValid(DamageSubsystem))
	{
		return false;
	}

	FAIRECombatDamageRequest DamageRequest;
	DamageRequest.Source = GetAvatarActorFromActorInfo();
	DamageRequest.Target = TargetActor;
	DamageRequest.Damage = ActiveWeaponDefinition->CombatSkill.Damage;
	DamageRequest.StaggerValue =
		ActiveWeaponDefinition->CombatSkill.StaggerValue;
	DamageRequest.ExecutionId = ActiveExecutionId;
	return DamageSubsystem->ApplyDamageRequest(DamageRequest)
		== EAIRECombatDamageResult::Applied;
}

void UAIRECompanionCombatSkillAbility::StartHitEventWait()
{
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		AIRECompanionGameplayTags::EventCombatSkillHit,
		nullptr,
		false,
		true);
	if (!IsValid(HitEventTask))
	{
		FinishAbility(true);
		return;
	}

	HitEventTask->EventReceived.AddDynamic(
		this,
		&UAIRECompanionCombatSkillAbility::HandleHitEvent);
	HitEventTask->ReadyForActivation();
}

void UAIRECompanionCombatSkillAbility::StartFallback()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!IsActiveExecutionValid()
		|| !ActorInfo
		|| !ActorInfo->AvatarActor.IsValid())
	{
		FinishAbility(true);
		return;
	}

	UWorld* World = ActorInfo->AvatarActor->GetWorld();
	if (!IsValid(World))
	{
		FinishAbility(true);
		return;
	}

	const float HitDelay =
		ActiveWeaponDefinition->CombatSkill.FallbackHitDelay;
	if (HitDelay <= 0.0f)
	{
		SendFallbackHitEvent();
		return;
	}

	World->GetTimerManager().SetTimer(
		FallbackHitTimerHandle,
		this,
		&UAIRECompanionCombatSkillAbility::SendFallbackHitEvent,
		HitDelay,
		false);
}

void UAIRECompanionCombatSkillAbility::SendFallbackHitEvent()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!IsActiveExecutionValid()
		|| !ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !ActorInfo->AvatarActor.IsValid())
	{
		FinishAbility(true);
		return;
	}

	FGameplayEventData HitPayload;
	HitPayload.EventTag =
		AIRECompanionGameplayTags::EventCombatSkillHit;
	HitPayload.Instigator = ActorInfo->AvatarActor.Get();
	HitPayload.Target = GetEventTarget();
	ActorInfo->AbilitySystemComponent->HandleGameplayEvent(
		AIRECompanionGameplayTags::EventCombatSkillHit,
		&HitPayload);

	ActorInfo = GetCurrentActorInfo();
	if (!IsActiveExecutionValid()
		|| !ActorInfo
		|| !ActorInfo->AvatarActor.IsValid()
		|| !IsValid(ActiveWeaponDefinition))
	{
		FinishAbility(true);
		return;
	}

	const float RecoveryDuration =
		ActiveWeaponDefinition->CombatSkill.FallbackRecoveryDuration;
	if (RecoveryDuration <= 0.0f)
	{
		FinishFallback();
		return;
	}

	if (UWorld* World = ActorInfo->AvatarActor->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FallbackRecoveryTimerHandle,
			this,
			&UAIRECompanionCombatSkillAbility::FinishFallback,
			RecoveryDuration,
			false);
	}
	else
	{
		FinishAbility(true);
	}
}

void UAIRECompanionCombatSkillAbility::FinishFallback()
{
	FinishAbility(false);
}

void UAIRECompanionCombatSkillAbility::SendTransitionEvent(
	const FGameplayTag EventTag,
	const bool bCompleted)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !ActorInfo->AvatarActor.IsValid())
	{
		return;
	}

	FGameplayEventData TransitionPayload;
	TransitionPayload.EventTag = EventTag;
	TransitionPayload.Instigator = ActorInfo->AvatarActor.Get();
	TransitionPayload.Target = GetEventTarget();
	TransitionPayload.EventMagnitude = bCompleted ? 1.0f : 0.0f;
	ActorInfo->AbilitySystemComponent->HandleGameplayEvent(
		EventTag,
		&TransitionPayload);
}

void UAIRECompanionCombatSkillAbility::HandleHitEvent(
	const FGameplayEventData Payload)
{
	if (bIsEnding || bHitConsumed)
	{
		return;
	}

	AActor* TargetActor = GetEventTarget();
	if (IsValid(Payload.Target.Get())
		&& Payload.Target.Get() != TargetActor)
	{
		return;
	}

	bHitConsumed = true;
	ResolveSkillHit();
}

void UAIRECompanionCombatSkillAbility::HandleMontageCompleted()
{
	FinishAbility(false);
}

void UAIRECompanionCombatSkillAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UAIRECompanionCombatSkillAbility::FinishAbility(
	const bool bWasCancelled)
{
	if (bIsEnding || !IsActive())
	{
		return;
	}

	bIsEnding = true;
	EndAbility(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActorInfo(),
		GetCurrentActivationInfo(),
		true,
		bWasCancelled);
}

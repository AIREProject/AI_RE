#include "AIRECompanionMeleeAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AIRECompanionAttackCooldownGameplayEffect.h"
#include "AIRECompanionAttackCostGameplayEffect.h"
#include "AIRECompanionAttributeSet.h"
#include "AIRECompanionDamageGameplayEffect.h"
#include "AIRECompanionGameplayTags.h"
#include "AIRECompanionWeaponDefinitionDataAsset.h"
#include "AIREThreatTargetInterface.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionMeleeAttack, Log, All);

UAIRECompanionMeleeAttackAbility::UAIRECompanionMeleeAttackAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AIRECompanionGameplayTags::AbilityCombatBasicAttack);
	SetAssetTags(AssetTags);

	FAbilityTriggerData& AttackTrigger = AbilityTriggers.AddDefaulted_GetRef();
	AttackTrigger.TriggerTag = AIRECompanionGameplayTags::EventAttackRequest;
	AttackTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;

	ActivationOwnedTags.AddTag(AIRECompanionGameplayTags::StateActionAttacking);
	CostGameplayEffectClass = UAIRECompanionAttackCostGameplayEffect::StaticClass();
	CooldownGameplayEffectClass = UAIRECompanionAttackCooldownGameplayEffect::StaticClass();
}

bool UAIRECompanionMeleeAttackAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer*) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition =
		GetWeaponDefinition(Handle, ActorInfo);
	const UAIRECompanionAttributeSet* Attributes =
		ActorInfo->AbilitySystemComponent->GetSet<UAIRECompanionAttributeSet>();
	if (!IsValid(WeaponDefinition) || !IsValid(Attributes))
	{
		return false;
	}

	return Attributes->GetStamina() >= WeaponDefinition->StaminaCost;
}

void UAIRECompanionMeleeAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bIsEnding = false;
	bHitConsumed = false;
	ActiveWeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	AttackRange = TriggerEventData ? TriggerEventData->EventMagnitude : 0.0f;

	const bool bHasValidRange = FMath::IsFinite(AttackRange) && AttackRange >= 0.0f;
	if (!InitializeEventTarget(TriggerEventData)
		|| !IsValid(ActiveWeaponDefinition)
		|| !ActiveWeaponDefinition->IsMeleeWeapon()
		|| !bHasValidRange
		|| !IsTargetValidForAttack(GetEventTarget())
		|| !IsTargetInRange(GetEventTarget()))
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Warning,
			TEXT("Companion melee attack rejected invalid activation data. Source=%s Target=%s Weapon=%s Range=%.2f"),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
			*GetNameSafe(GetEventTarget()),
			*GetNameSafe(ActiveWeaponDefinition),
			AttackRange);
		FinishAbility(true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Verbose,
			TEXT("Companion melee attack could not commit cost or cooldown. Source=%s Weapon=%s"),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
			*GetNameSafe(ActiveWeaponDefinition));
		FinishAbility(true);
		return;
	}

	StartHitEventWait();
	if (bIsEnding)
	{
		return;
	}

	if (ActiveWeaponDefinition->AttackMontage.IsNull())
	{
		StartFallbackAttack();
		return;
	}

	UAnimMontage* AttackMontage = ActiveWeaponDefinition->AttackMontage.LoadSynchronous();
	if (!IsValid(AttackMontage))
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Warning,
			TEXT("Configured attack montage could not be loaded. Weapon=%s"),
			*GetNameSafe(ActiveWeaponDefinition));
		FinishAbility(true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("CompanionBasicAttackMontage"),
		AttackMontage,
		1.0f,
		NAME_None,
		true,
		0.0f);
	if (!IsValid(MontageTask))
	{
		FinishAbility(true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UAIRECompanionMeleeAttackAbility::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UAIRECompanionMeleeAttackAbility::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UAIRECompanionMeleeAttackAbility::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UAIRECompanionMeleeAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	bIsEnding = true;
	UE_LOG(
		LogAIRECompanionMeleeAttack,
		Log,
		TEXT("Companion melee attack ended. Source=%s Target=%s Cancelled=%s HitConsumed=%s"),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
		*GetNameSafe(GetEventTarget()),
		bWasCancelled ? TEXT("true") : TEXT("false"),
		bHitConsumed ? TEXT("true") : TEXT("false"));
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (UWorld* World = ActorInfo->AvatarActor->GetWorld())
		{
			World->GetTimerManager().ClearTimer(FallbackHitTimerHandle);
			World->GetTimerManager().ClearTimer(FallbackRecoveryTimerHandle);
		}
	}

	MontageTask = nullptr;
	HitEventTask = nullptr;
	ActiveWeaponDefinition = nullptr;
	AttackRange = 0.0f;
	bHitConsumed = false;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

UAIRECompanionWeaponDefinitionDataAsset*
UAIRECompanionMeleeAttackAbility::GetWeaponDefinition(
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
		? Cast<UAIRECompanionWeaponDefinitionDataAsset>(AbilitySpec->SourceObject.Get())
		: nullptr;
}

bool UAIRECompanionMeleeAttackAbility::IsTargetValidForAttack(
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

bool UAIRECompanionMeleeAttackAbility::IsTargetInRange(
	const AActor* TargetActor) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || !IsValid(TargetActor))
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
	return EffectiveDistance <= AttackRange;
}

void UAIRECompanionMeleeAttackAbility::StartHitEventWait()
{
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		AIRECompanionGameplayTags::EventAttackHit,
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
		&UAIRECompanionMeleeAttackAbility::HandleHitEvent);
	HitEventTask->ReadyForActivation();
}

void UAIRECompanionMeleeAttackAbility::StartFallbackAttack()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !IsValid(ActiveWeaponDefinition))
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

	if (ActiveWeaponDefinition->FallbackHitDelay <= 0.0f)
	{
		SendFallbackHitEvent();
		return;
	}

	World->GetTimerManager().SetTimer(
		FallbackHitTimerHandle,
		this,
		&UAIRECompanionMeleeAttackAbility::SendFallbackHitEvent,
		ActiveWeaponDefinition->FallbackHitDelay,
		false);
}

void UAIRECompanionMeleeAttackAbility::SendFallbackHitEvent()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !ActorInfo->AvatarActor.IsValid())
	{
		FinishAbility(true);
		return;
	}

	FGameplayEventData HitPayload;
	HitPayload.EventTag = AIRECompanionGameplayTags::EventAttackHit;
	HitPayload.Instigator = ActorInfo->AvatarActor.Get();
	HitPayload.Target = GetEventTarget();
	ActorInfo->AbilitySystemComponent->HandleGameplayEvent(
		AIRECompanionGameplayTags::EventAttackHit,
		&HitPayload);
	ScheduleFallbackRecovery();
}

void UAIRECompanionMeleeAttackAbility::ScheduleFallbackRecovery()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !IsValid(ActiveWeaponDefinition))
	{
		FinishAbility(true);
		return;
	}

	if (ActiveWeaponDefinition->FallbackRecoveryDuration <= 0.0f)
	{
		FinishFallbackAttack();
		return;
	}

	UWorld* World = ActorInfo->AvatarActor->GetWorld();
	if (!IsValid(World))
	{
		FinishAbility(true);
		return;
	}

	World->GetTimerManager().SetTimer(
		FallbackRecoveryTimerHandle,
		this,
		&UAIRECompanionMeleeAttackAbility::FinishFallbackAttack,
		ActiveWeaponDefinition->FallbackRecoveryDuration,
		false);
}

void UAIRECompanionMeleeAttackAbility::FinishFallbackAttack()
{
	FinishAbility(false);
}

void UAIRECompanionMeleeAttackAbility::HandleHitEvent(
	const FGameplayEventData Payload)
{
	if (bIsEnding || bHitConsumed)
	{
		return;
	}

	AActor* TargetActor = GetEventTarget();
	if (IsValid(Payload.Target.Get()) && Payload.Target.Get() != TargetActor)
	{
		return;
	}

	bHitConsumed = true;
	if (!IsTargetValidForAttack(TargetActor) || !IsTargetInRange(TargetActor))
	{
		return;
	}

	UAbilitySystemComponent* SourceAbilitySystem = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetAbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor, true);
	if (!IsValid(SourceAbilitySystem)
		|| !IsValid(TargetAbilitySystem)
		|| !IsValid(ActiveWeaponDefinition))
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());
	FGameplayEffectSpecHandle DamageSpec = SourceAbilitySystem->MakeOutgoingSpec(
		UAIRECompanionDamageGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!DamageSpec.IsValid())
	{
		return;
	}

	DamageSpec.Data->SetSetByCallerMagnitude(
		AIRECompanionGameplayTags::DataDamage,
		-ActiveWeaponDefinition->Damage);
	const FActiveGameplayEffectHandle AppliedDamage =
		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
			*DamageSpec.Data.Get(),
			TargetAbilitySystem);
	if (AppliedDamage.WasSuccessfullyApplied())
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Log,
			TEXT("Companion melee hit applied. Source=%s Target=%s Damage=%.2f"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*GetNameSafe(TargetActor),
			ActiveWeaponDefinition->Damage);
	}
}

void UAIRECompanionMeleeAttackAbility::HandleMontageCompleted()
{
	if (!bHitConsumed)
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Warning,
			TEXT("Companion attack montage completed without receiving a hit event. Weapon=%s"),
			*GetNameSafe(ActiveWeaponDefinition));
	}
	FinishAbility(false);
}

void UAIRECompanionMeleeAttackAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UAIRECompanionMeleeAttackAbility::FinishAbility(const bool bWasCancelled)
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

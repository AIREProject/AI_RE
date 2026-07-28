#include "AbilitySystem/Combat/Abilities/AIRECompanionMeleeAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionAttackCooldownGameplayEffect.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionDamageGameplayEffect.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Animation/AnimMontage.h"
#include "Core/AIRECompanionAIController.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Threat/AIRECompanionThreatComponent.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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
	ActivationOwnedTags.AddTag(
		AIRECompanionGameplayTags::StateActionAttackingBasic);
	ActivationBlockedTags.AddTag(
		AIRECompanionGameplayTags::StateActionAttackingSkill);
	CooldownGameplayEffectClass = UAIRECompanionAttackCooldownGameplayEffect::StaticClass();
}

void UAIRECompanionMeleeAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bIsEnding = false;
	bUsingFallback = false;
	bSuspendedForCombatSkill = false;
	bSkillCancelWindowTagApplied = false;
	CurrentStepIndex = 0;
	ResumeStepIndex = INDEX_NONE;
	ResetCurrentStepState();
	ActiveWeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	AttackRange = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->AttackRange
		: 0.0f;

	const bool bHasValidRange = FMath::IsFinite(AttackRange) && AttackRange >= 0.0f;
	if (!InitializeEventTarget(TriggerEventData)
		|| !IsValid(ActiveWeaponDefinition)
		|| !ActiveWeaponDefinition->IsMeleeWeapon()
		|| !IsAttackStepIndexValid(CurrentStepIndex)
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

	if (!CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false))
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Verbose,
			TEXT("Companion melee attack could not commit cooldown. Source=%s Weapon=%s"),
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

	if (GetAttackStepCount() > 1)
	{
		StartComboWindowEventWait();
		if (bIsEnding)
		{
			return;
		}
	}

	StartCombatSkillTransitionEventWait();
	if (bIsEnding)
	{
		return;
	}

	FaceTarget(GetEventTarget());

	StartAttackMontage();
}

void UAIRECompanionMeleeAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	bIsEnding = true;
	SetSkillCancelWindowTag(false);
	UE_LOG(
		LogAIRECompanionMeleeAttack,
		Log,
		TEXT("Companion melee attack ended. Source=%s Target=%s Cancelled=%s Step=%d StepHitConsumed=%s"),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
		*GetNameSafe(GetEventTarget()),
		bWasCancelled ? TEXT("true") : TEXT("false"),
		CurrentStepIndex,
		bCurrentStepHitConsumed ? TEXT("true") : TEXT("false"));
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
	ComboWindowEventTask = nullptr;
	CombatSkillTransitionEventTask = nullptr;
	ActiveWeaponDefinition = nullptr;
	AttackRange = 0.0f;
	CurrentStepIndex = INDEX_NONE;
	ResumeStepIndex = INDEX_NONE;
	bCurrentStepHitConsumed = false;
	bComboWindowOpen = false;
	bNextStepQueued = false;
	bUsingFallback = false;
	bSuspendedForCombatSkill = false;

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

int32 UAIRECompanionMeleeAttackAbility::GetAttackStepCount() const
{
	if (!IsValid(ActiveWeaponDefinition))
	{
		return 0;
	}

	return ActiveWeaponDefinition->ComboSteps.IsEmpty()
		? 1
		: ActiveWeaponDefinition->ComboSteps.Num();
}

bool UAIRECompanionMeleeAttackAbility::IsAttackStepIndexValid(const int32 StepIndex) const
{
	return StepIndex >= 0 && StepIndex < GetAttackStepCount();
}

float UAIRECompanionMeleeAttackAbility::GetAttackStepDamage(const int32 StepIndex) const
{
	if (!IsAttackStepIndexValid(StepIndex))
	{
		return 0.0f;
	}

	return ActiveWeaponDefinition->ComboSteps.IsEmpty()
		? ActiveWeaponDefinition->Damage
		: ActiveWeaponDefinition->ComboSteps[StepIndex].Damage;
}

FName UAIRECompanionMeleeAttackAbility::GetAttackStepMontageSection(const int32 StepIndex) const
{
	if (!IsAttackStepIndexValid(StepIndex)
		|| ActiveWeaponDefinition->ComboSteps.IsEmpty())
	{
		return NAME_None;
	}

	return ActiveWeaponDefinition->ComboSteps[StepIndex].MontageSection;
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

void UAIRECompanionMeleeAttackAbility::FaceTarget(
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

bool UAIRECompanionMeleeAttackAbility::IsActiveExecutionValid() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (bIsEnding
		|| !IsActive()
		|| !ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !IsValid(ActiveWeaponDefinition)
		|| GetWeaponDefinition(GetCurrentAbilitySpecHandle(), ActorInfo) != ActiveWeaponDefinition)
	{
		return false;
	}

	if (!ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(
		AIRECompanionGameplayTags::StateActionAttacking))
	{
		return false;
	}

	const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	const AAIRECompanionAIController* CompanionController = IsValid(AvatarPawn)
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

bool UAIRECompanionMeleeAttackAbility::AreComboMontageSectionsValid(
	const UAnimMontage* AttackMontage) const
{
	if (!IsValid(AttackMontage) || !IsValid(ActiveWeaponDefinition))
	{
		return false;
	}

	for (const FAIREWeaponComboStepDefinition& ComboStep : ActiveWeaponDefinition->ComboSteps)
	{
		if (AttackMontage->GetSectionIndex(ComboStep.MontageSection) == INDEX_NONE)
		{
			return false;
		}
	}

	return true;
}

bool UAIRECompanionMeleeAttackAbility::TryStartNextStep()
{
	const int32 NextStepIndex = CurrentStepIndex + 1;
	AActor* TargetActor = GetEventTarget();
	if (!IsActiveExecutionValid()
		|| !IsAttackStepIndexValid(NextStepIndex)
		|| !IsTargetValidForAttack(TargetActor)
		|| !IsTargetInRange(TargetActor))
	{
		return false;
	}

	FaceTarget(TargetActor);
	CurrentStepIndex = NextStepIndex;
	ResetCurrentStepState();
	return true;
}

bool UAIRECompanionMeleeAttackAbility::StartAttackMontage()
{
	if (!IsActiveExecutionValid() || bSuspendedForCombatSkill)
	{
		return false;
	}

	UAnimMontage* AttackMontage =
		ActiveWeaponDefinition->AttackMontage.Get();
	if (!IsValid(AttackMontage)
		|| !AreComboMontageSectionsValid(AttackMontage))
	{
		if (!ActiveWeaponDefinition->AttackMontage.IsNull())
		{
			UE_LOG(
				LogAIRECompanionMeleeAttack,
				Warning,
				TEXT("Configured attack montage or combo sections are unavailable. Using the Basic Attack fallback. Weapon=%s"),
				*GetNameSafe(ActiveWeaponDefinition));
		}

		bUsingFallback = true;
		StartFallbackStep();
		return !bIsEnding;
	}

	MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("CompanionBasicAttackMontage"),
			AttackMontage,
			1.0f,
			GetAttackStepMontageSection(CurrentStepIndex),
			true,
			0.0f);
	if (!IsValid(MontageTask))
	{
		FinishAbility(true);
		return false;
	}

	MontageTask->OnCompleted.AddDynamic(
		this,
		&UAIRECompanionMeleeAttackAbility::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(
		this,
		&UAIRECompanionMeleeAttackAbility::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(
		this,
		&UAIRECompanionMeleeAttackAbility::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();

	if (!ActiveWeaponDefinition->ComboSteps.IsEmpty())
	{
		for (const FAIREWeaponComboStepDefinition& ComboStep
			: ActiveWeaponDefinition->ComboSteps)
		{
			MontageSetNextSectionName(
				ComboStep.MontageSection,
				NAME_None);
		}

		if (CurrentStepIndex + 1 < GetAttackStepCount())
		{
			MontageSetNextSectionName(
				GetAttackStepMontageSection(CurrentStepIndex),
				GetAttackStepMontageSection(CurrentStepIndex + 1));
		}
	}

	return true;
}

bool UAIRECompanionMeleeAttackAbility::ResumeAfterCombatSkill()
{
	AActor* TargetActor = GetEventTarget();
	if (!IsAttackStepIndexValid(ResumeStepIndex)
		|| !IsActiveExecutionValid()
		|| !IsTargetValidForAttack(TargetActor))
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Verbose,
			TEXT("Companion combo resume rejected by execution or target validation. ResumeStep=%d Target=%s"),
			ResumeStepIndex,
			*GetNameSafe(TargetActor));
		return false;
	}

	if (!IsTargetInRange(TargetActor))
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Log,
			TEXT("Companion combo resume rejected by range. ResumeStep=%d Target=%s AttackRange=%.2f"),
			ResumeStepIndex,
			*GetNameSafe(TargetActor),
			AttackRange);
		return false;
	}

	FaceTarget(TargetActor);
	CurrentStepIndex = ResumeStepIndex;
	ResumeStepIndex = INDEX_NONE;
	ResetCurrentStepState();
	bUsingFallback = false;
	return StartAttackMontage();
}

bool UAIRECompanionMeleeAttackAbility::TryGetEventStepIndex(
	const FGameplayEventData& Payload,
	int32& OutStepIndex) const
{
	if (!FMath::IsFinite(Payload.EventMagnitude))
	{
		return false;
	}

	const int32 PayloadStepIndex = FMath::RoundToInt(Payload.EventMagnitude);
	if (!FMath::IsNearlyEqual(
			Payload.EventMagnitude,
			static_cast<float>(PayloadStepIndex))
		|| !IsAttackStepIndexValid(PayloadStepIndex))
	{
		return false;
	}

	OutStepIndex = PayloadStepIndex;
	return true;
}

bool UAIRECompanionMeleeAttackAbility::ResolveCurrentStepHit()
{
	AActor* TargetActor = GetEventTarget();
	if (!IsActiveExecutionValid()
		|| !IsTargetValidForAttack(TargetActor)
		|| !IsTargetInRange(TargetActor))
	{
		return false;
	}

	UAbilitySystemComponent* SourceAbilitySystem = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetAbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor, true);
	if (!IsValid(SourceAbilitySystem) || !IsValid(TargetAbilitySystem))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());
	FGameplayEffectSpecHandle DamageSpec = SourceAbilitySystem->MakeOutgoingSpec(
		UAIRECompanionDamageGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!DamageSpec.IsValid())
	{
		return false;
	}

	const float StepDamage = GetAttackStepDamage(CurrentStepIndex);
	DamageSpec.Data->SetSetByCallerMagnitude(
		AIRECompanionGameplayTags::DataDamage,
		-StepDamage);
	const FActiveGameplayEffectHandle AppliedDamage =
		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
			*DamageSpec.Data.Get(),
			TargetAbilitySystem);
	if (!AppliedDamage.WasSuccessfullyApplied())
	{
		return false;
	}

	UE_LOG(
		LogAIRECompanionMeleeAttack,
		Log,
		TEXT("Companion melee hit applied. Source=%s Target=%s Step=%d Damage=%.2f"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(TargetActor),
		CurrentStepIndex,
		StepDamage);
	return true;
}

void UAIRECompanionMeleeAttackAbility::ResetCurrentStepState()
{
	bCurrentStepHitConsumed = false;
	bComboWindowOpen = false;
	bNextStepQueued = false;
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

void UAIRECompanionMeleeAttackAbility::StartComboWindowEventWait()
{
	ComboWindowEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		AIRECompanionGameplayTags::EventAttackComboWindow,
		nullptr,
		false,
		false);
	if (!IsValid(ComboWindowEventTask))
	{
		FinishAbility(true);
		return;
	}

	ComboWindowEventTask->EventReceived.AddDynamic(
		this,
		&UAIRECompanionMeleeAttackAbility::HandleComboWindowEvent);
	ComboWindowEventTask->ReadyForActivation();
}

void UAIRECompanionMeleeAttackAbility::StartCombatSkillTransitionEventWait()
{
	CombatSkillTransitionEventTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			AIRECompanionGameplayTags::EventCombatSkillTransition,
			nullptr,
			false,
			false);
	if (!IsValid(CombatSkillTransitionEventTask))
	{
		FinishAbility(true);
		return;
	}

	CombatSkillTransitionEventTask->EventReceived.AddDynamic(
		this,
		&UAIRECompanionMeleeAttackAbility::HandleCombatSkillTransitionEvent);
	CombatSkillTransitionEventTask->ReadyForActivation();
}

void UAIRECompanionMeleeAttackAbility::SetSkillCancelWindowTag(
	const bool bEnabled)
{
	if (bSkillCancelWindowTagApplied == bEnabled)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem =
		GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(AbilitySystem))
	{
		bSkillCancelWindowTagApplied = false;
		return;
	}

	if (bEnabled)
	{
		AbilitySystem->AddLooseGameplayTag(
			AIRECompanionGameplayTags::
				StateActionAttackingSkillCancelable);
	}
	else
	{
		AbilitySystem->RemoveLooseGameplayTag(
			AIRECompanionGameplayTags::
				StateActionAttackingSkillCancelable);
	}
	bSkillCancelWindowTagApplied = bEnabled;
}

void UAIRECompanionMeleeAttackAbility::StartFallbackStep()
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
	if (!IsActiveExecutionValid()
		|| !ActorInfo
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
	HitPayload.EventMagnitude = static_cast<float>(CurrentStepIndex);
	ActorInfo->AbilitySystemComponent->HandleGameplayEvent(
		AIRECompanionGameplayTags::EventAttackHit,
		&HitPayload);
	ScheduleFallbackRecovery();
}

void UAIRECompanionMeleeAttackAbility::ScheduleFallbackRecovery()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!IsActiveExecutionValid()
		|| !ActorInfo
		|| !ActorInfo->AvatarActor.IsValid())
	{
		FinishAbility(true);
		return;
	}

	if (ActiveWeaponDefinition->FallbackRecoveryDuration <= 0.0f)
	{
		FinishFallbackStep();
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
		&UAIRECompanionMeleeAttackAbility::FinishFallbackStep,
		ActiveWeaponDefinition->FallbackRecoveryDuration,
		false);
}

void UAIRECompanionMeleeAttackAbility::FinishFallbackStep()
{
	if (CurrentStepIndex + 1 >= GetAttackStepCount())
	{
		FinishAbility(false);
		return;
	}

	if (!TryStartNextStep())
	{
		FinishAbility(false);
		return;
	}

	StartFallbackStep();
}

void UAIRECompanionMeleeAttackAbility::HandleHitEvent(
	const FGameplayEventData Payload)
{
	int32 PayloadStepIndex = INDEX_NONE;
	if (bIsEnding
		|| bSuspendedForCombatSkill
		|| bCurrentStepHitConsumed
		|| !TryGetEventStepIndex(Payload, PayloadStepIndex)
		|| PayloadStepIndex != CurrentStepIndex)
	{
		return;
	}

	AActor* TargetActor = GetEventTarget();
	if (IsValid(Payload.Target.Get()) && Payload.Target.Get() != TargetActor)
	{
		return;
	}

	bCurrentStepHitConsumed = true;
	ResolveCurrentStepHit();
}

void UAIRECompanionMeleeAttackAbility::HandleComboWindowEvent(
	const FGameplayEventData Payload)
{
	int32 PayloadStepIndex = INDEX_NONE;
	if (bIsEnding
		|| bUsingFallback
		|| !IsActiveExecutionValid()
		|| !TryGetEventStepIndex(Payload, PayloadStepIndex)
		|| PayloadStepIndex != CurrentStepIndex)
	{
		return;
	}

	if (Payload.EventTag.MatchesTagExact(
		AIRECompanionGameplayTags::EventAttackComboWindowBegin))
	{
		if (!bComboWindowOpen)
		{
			bComboWindowOpen = true;
			bNextStepQueued = CurrentStepIndex + 1 < GetAttackStepCount();
			SetSkillCancelWindowTag(bNextStepQueued);
			UE_LOG(
				LogAIRECompanionMeleeAttack,
				Verbose,
				TEXT("Companion combo window opened. Step=%d Queued=%s"),
				CurrentStepIndex,
				bNextStepQueued ? TEXT("true") : TEXT("false"));
		}
		return;
	}

	if (!Payload.EventTag.MatchesTagExact(
			AIRECompanionGameplayTags::EventAttackComboWindowEnd)
		|| !bComboWindowOpen)
	{
		return;
	}

	bComboWindowOpen = false;
	SetSkillCancelWindowTag(false);
	const bool bShouldAdvance = bNextStepQueued;
	bNextStepQueued = false;
	if (!bShouldAdvance)
	{
		return;
	}

	if (!TryStartNextStep())
	{
		FinishAbility(false);
		return;
	}

	UE_LOG(
		LogAIRECompanionMeleeAttack,
		Log,
		TEXT("Companion combo advanced. Step=%d Section=%s"),
		CurrentStepIndex,
		*GetAttackStepMontageSection(CurrentStepIndex).ToString());

	if (CurrentStepIndex + 1 < GetAttackStepCount())
	{
		MontageSetNextSectionName(
			GetAttackStepMontageSection(CurrentStepIndex),
			GetAttackStepMontageSection(CurrentStepIndex + 1));
	}
}

void UAIRECompanionMeleeAttackAbility::HandleCombatSkillTransitionEvent(
	const FGameplayEventData Payload)
{
	if (bIsEnding || bUsingFallback)
	{
		return;
	}

	AActor* TargetActor = GetEventTarget();
	if (IsValid(Payload.Target.Get())
		&& Payload.Target.Get() != TargetActor)
	{
		return;
	}

	if (Payload.EventTag.MatchesTagExact(
		AIRECompanionGameplayTags::EventCombatSkillStarted))
	{
		if (bSuspendedForCombatSkill
			|| !bSkillCancelWindowTagApplied
			|| !IsActiveExecutionValid())
		{
			return;
		}

		ResumeStepIndex = CurrentStepIndex + 1;
		bSuspendedForCombatSkill = true;
		bComboWindowOpen = false;
		bNextStepQueued = false;
		SetSkillCancelWindowTag(false);
		MontageStop(0.05f);
		MontageTask = nullptr;

		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Log,
			TEXT("Companion basic combo suspended for combat skill. CurrentStep=%d ResumeStep=%d"),
			CurrentStepIndex,
			ResumeStepIndex);
		return;
	}

	if (!Payload.EventTag.MatchesTagExact(
			AIRECompanionGameplayTags::EventCombatSkillEnded)
		|| !bSuspendedForCombatSkill)
	{
		return;
	}

	bSuspendedForCombatSkill = false;
	const bool bSkillCompleted =
		FMath::IsNearlyEqual(Payload.EventMagnitude, 1.0f);
	if (!bSkillCompleted || !ResumeAfterCombatSkill())
	{
		FinishAbility(!bSkillCompleted);
		return;
	}

	UE_LOG(
		LogAIRECompanionMeleeAttack,
		Log,
		TEXT("Companion basic combo resumed after combat skill. Step=%d Section=%s"),
		CurrentStepIndex,
		*GetAttackStepMontageSection(CurrentStepIndex).ToString());
}

void UAIRECompanionMeleeAttackAbility::HandleMontageCompleted()
{
	if (bSuspendedForCombatSkill)
	{
		return;
	}

	if (!bCurrentStepHitConsumed)
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Warning,
			TEXT("Companion attack montage completed without receiving a hit event for Step %d. Weapon=%s"),
			CurrentStepIndex,
			*GetNameSafe(ActiveWeaponDefinition));
	}
	FinishAbility(false);
}

void UAIRECompanionMeleeAttackAbility::HandleMontageInterrupted()
{
	if (bSuspendedForCombatSkill)
	{
		return;
	}

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

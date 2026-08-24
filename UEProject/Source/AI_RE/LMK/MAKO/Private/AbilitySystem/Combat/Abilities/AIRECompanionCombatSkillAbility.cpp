#include "AbilitySystem/Combat/Abilities/AIRECompanionCombatSkillAbility.h"

#include "AbilitySystem/Combat/AIRECompanionCombatVFX.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionCombatSkillCooldownGameplayEffect.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AIRECombatDamageSubsystem.h"
#include "AIRECombatMeleeTraceResolver.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionCombatSkill, Log, All);

namespace
{
	constexpr int32 WeaponTraceSubstepCount = 6;

	FQuat MakeWeaponCapsuleRotation(const FVector& Start, const FVector& End)
	{
		const FVector Axis = (End - Start).GetSafeNormal();
		return Axis.IsNearlyZero()
			? FQuat::Identity
			: FQuat::FindBetweenNormals(FVector::UpVector, Axis);
	}

	FVector MakeWeaponCapsuleCenter(
		const FVector& Start,
		const FVector& End)
	{
		return (Start + End) * 0.5f;
	}
}

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
	bPointSampleConsumed = false;
	bUsingFallback = false;
	bTraceWindowEverOpened = false;
	bTransitionStarted = false;
	CloseSkillTrace();
	ActiveExecutionId = FGuid::NewGuid();
	ActiveWeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	ActiveDamage = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->CombatSkill.Damage
		: 0.0f;
	ActiveStaggerValue = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->CombatSkill.StaggerValue
		: 0.0f;
	ActiveAttackRange = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->CombatSkill.AttackRange
		: 0.0f;
	ActiveTargetingMode = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->CombatSkill.TargetingMode
		: EAIRECombatTargetingMode::Area;
	ActiveTraceRadius = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->TraceCapsuleRadius
		: 0.0f;
	ActiveTraceCapsuleHalfHeight = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->TraceCapsuleHalfHeight
		: 0.0f;
	ActiveTraceChannel = ECC_MAX;
	if (IsValid(ActiveWeaponDefinition))
	{
		ActiveTraceChannel = ActiveWeaponDefinition->TraceChannel;
	}
	ActiveTraceStartSocket = NAME_None;
	ActiveTraceEndSocket = NAME_None;
	if (IsValid(ActiveWeaponDefinition))
	{
		const FAIREWeaponTraceSocketPair TraceSockets =
			ActiveWeaponDefinition->ResolveTraceSockets(
				ActiveWeaponDefinition->CombatSkill.TraceSide,
				ActiveWeaponDefinition->CombatSkill.TraceSocketOverride);
		ActiveTraceStartSocket = TraceSockets.TraceStartSocket;
		ActiveTraceEndSocket = TraceSockets.TraceEndSocket;
	}

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
	GetEventTarget()->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIRECompanionCombatSkillAbility::HandleTargetDestroyed);
	UE_LOG(
		LogAIRECompanionCombatSkill,
		Log,
		TEXT("[MAKO ATTACK] Type=CombatSkill Phase=StrikeReady Source=%s Target=%s ExecutionId=%s Damage=%.2f Stagger=%.2f CapsuleRadius=%.1f CapsuleHalfHeight=%.1f Sockets=%s->%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(TargetActor),
		*ActiveExecutionId.ToString(),
		ActiveDamage,
		ActiveStaggerValue,
		ActiveTraceRadius,
		ActiveTraceCapsuleHalfHeight,
		*ActiveTraceStartSocket.ToString(),
		*ActiveTraceEndSocket.ToString());

	FaceTarget(TargetActor);
	StartHitEventWait();
	if (bIsEnding)
	{
		return;
	}
	StartTraceEventWait();
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
	if (AActor* TargetActor = GetEventTarget())
	{
		TargetActor->OnDestroyed.RemoveDynamic(
			this,
			&UAIRECompanionCombatSkillAbility::HandleTargetDestroyed);
	}
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
		TEXT("[MAKO ATTACK] Type=CombatSkill Phase=Ended Source=%s Target=%s Cancelled=%s TerminalSpatialResult=%s"),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
		*GetNameSafe(GetEventTarget()),
		bWasCancelled ? TEXT("true") : TEXT("false"),
		bHitConsumed ? TEXT("Resolved") : TEXT("Miss"));

	MontageTask = nullptr;
	HitEventTask = nullptr;
	TraceEventTask = nullptr;
	CloseSkillTrace();
	ActiveWeaponDefinition = nullptr;
	ActiveDamage = 0.0f;
	ActiveStaggerValue = 0.0f;
	ActiveAttackRange = 0.0f;
	ActiveTraceRadius = 0.0f;
	ActiveTraceCapsuleHalfHeight = 0.0f;
	ActiveTraceChannel = ECC_MAX;
	ActiveTraceStartSocket = NAME_None;
	ActiveTraceEndSocket = NAME_None;
	ActiveTargetingMode = EAIRECombatTargetingMode::SingleTarget;
	ActiveExecutionId.Invalidate();
	bHitConsumed = false;
	bPointSampleConsumed = false;
	bUsingFallback = false;
	bTraceWindowEverOpened = false;

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

	// The activation target is the skill strike snapshot. Later threat
	// reselection and range changes do not replace an in-flight target.
	return IsTargetValidForSkill(GetEventTarget());
}

bool UAIRECompanionCombatSkillAbility::ResolveSkillHit()
{
	const ACharacter* AvatarCharacter =
		Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* MeshComponent = IsValid(AvatarCharacter)
		? AvatarCharacter->GetMesh()
		: nullptr;
	FHitResult TargetHit;
	const EAIRECombatMeleeTraceResult TraceResult =
		SampleSkillTrace(MeshComponent, TargetHit);
	ResolveSkillTraceSample(TraceResult, TargetHit);
	return TraceResult == EAIRECombatMeleeTraceResult::TargetHit;
}

EAIRECombatMeleeTraceResult
UAIRECompanionCombatSkillAbility::SampleSkillTrace(
	USkeletalMeshComponent* MeshComponent,
	FHitResult& OutTargetHit)
{
	AActor* SourceActor = GetAvatarActorFromActorInfo();
	AActor* TargetActor = GetEventTarget();
	const bool bCommonTraceInputsValid =
		IsActiveExecutionValid()
		&& IsValid(SourceActor)
		&& IsValid(TargetActor)
		&& ActiveTargetingMode == EAIRECombatTargetingMode::SingleTarget
		&& FMath::IsFinite(ActiveTraceRadius)
		&& ActiveTraceRadius > 0.0f
		&& FMath::IsFinite(ActiveTraceCapsuleHalfHeight)
		&& ActiveTraceCapsuleHalfHeight >= ActiveTraceRadius
		&& ActiveTraceChannel.GetValue() < ECC_MAX;
	if (!bCommonTraceInputsValid)
	{
		return EAIRECombatMeleeTraceResult::Invalid;
	}

	if (bUsingFallback)
	{
		FVector Forward = SourceActor->GetActorForwardVector().GetSafeNormal2D();
		if (Forward.IsNearlyZero()
			|| !FMath::IsFinite(ActiveAttackRange)
			|| ActiveAttackRange < 0.0f)
		{
			return EAIRECombatMeleeTraceResult::Invalid;
		}

		const FVector TraceStart = SourceActor->GetActorLocation()
			+ Forward * SourceActor->GetSimpleCollisionRadius();
		const float CenterTravelDistance = FMath::Max(
			0.0f,
			ActiveAttackRange - ActiveTraceRadius);
		const FVector TraceEnd =
			TraceStart + Forward * CenterTravelDistance;
		const FVector CapsuleCenter = TraceStart
			+ Forward * FMath::Max(
				0.0f,
				ActiveTraceCapsuleHalfHeight - ActiveTraceRadius);
		FAIRECombatMeleeTraceRequest TraceRequest;
		TraceRequest.World = GetWorld();
		TraceRequest.Source = SourceActor;
		TraceRequest.Target = TargetActor;
		TraceRequest.Shape = EAIRECombatMeleeTraceShape::Capsule;
		TraceRequest.Radius = ActiveTraceRadius;
		TraceRequest.CapsuleHalfHeight = ActiveTraceCapsuleHalfHeight;
		TraceRequest.TraceChannel = ActiveTraceChannel.GetValue();
		TraceRequest.Segments.Emplace(
			CapsuleCenter,
			CapsuleCenter,
			MakeWeaponCapsuleRotation(TraceStart, TraceEnd));
		const FAIRECombatMeleeTraceResolution Resolution =
			FAIRECombatMeleeTraceResolver::Resolve(TraceRequest);
		OutTargetHit = Resolution.HitResult;
		return Resolution.Result;
	}

	const bool bSocketTraceInputsValid =
		IsValid(MeshComponent)
		&& MeshComponent->GetOwner() == SourceActor
		&& !ActiveTraceStartSocket.IsNone()
		&& !ActiveTraceEndSocket.IsNone()
		&& MeshComponent->DoesSocketExist(ActiveTraceStartSocket)
		&& MeshComponent->DoesSocketExist(ActiveTraceEndSocket);
	if (!bSocketTraceInputsValid)
	{
		UE_LOG(
			LogAIRECompanionCombatSkill,
			Warning,
			TEXT("Companion combat skill trace rejected invalid source or sockets. Source=%s Mesh=%s Start=%s End=%s"),
			*GetNameSafe(SourceActor),
			*GetNameSafe(MeshComponent),
			*ActiveTraceStartSocket.ToString(),
			*ActiveTraceEndSocket.ToString());
		return EAIRECombatMeleeTraceResult::Invalid;
	}

	const FVector CurrentTraceStart =
		MeshComponent->GetSocketLocation(ActiveTraceStartSocket);
	const FVector CurrentTraceEnd =
		MeshComponent->GetSocketLocation(ActiveTraceEndSocket);
	const bool bHasPreviousSample =
		bTraceWindowOpen && ActiveTraceMesh.Get() == MeshComponent;
	const FVector TracePreviousStart = bHasPreviousSample
		? PreviousTraceStart
		: CurrentTraceStart;
	const FVector TracePreviousEnd = bHasPreviousSample
		? PreviousTraceEnd
		: CurrentTraceEnd;
	const FVector PreviousCapsuleCenter = MakeWeaponCapsuleCenter(
		TracePreviousStart,
		TracePreviousEnd);
	const FVector CurrentCapsuleCenter = MakeWeaponCapsuleCenter(
		CurrentTraceStart,
		CurrentTraceEnd);
	const FQuat PreviousCapsuleRotation = MakeWeaponCapsuleRotation(
		TracePreviousStart,
		TracePreviousEnd);
	const FQuat CurrentCapsuleRotation = MakeWeaponCapsuleRotation(
		CurrentTraceStart,
		CurrentTraceEnd);

	FAIRECombatMeleeTraceRequest TraceRequest;
	TraceRequest.World = GetWorld();
	TraceRequest.Source = SourceActor;
	TraceRequest.Target = TargetActor;
	TraceRequest.Shape = EAIRECombatMeleeTraceShape::Capsule;
	TraceRequest.Radius = ActiveTraceRadius;
	TraceRequest.CapsuleHalfHeight = ActiveTraceCapsuleHalfHeight;
	TraceRequest.TraceChannel = ActiveTraceChannel.GetValue();
	TraceRequest.Segments.Reserve(WeaponTraceSubstepCount);
	FVector SubstepStart = PreviousCapsuleCenter;
	for (int32 SubstepIndex = 1;
		SubstepIndex <= WeaponTraceSubstepCount;
		++SubstepIndex)
	{
		const float Alpha = static_cast<float>(SubstepIndex)
			/ static_cast<float>(WeaponTraceSubstepCount);
		const FVector SubstepEnd = FMath::Lerp(
			PreviousCapsuleCenter,
			CurrentCapsuleCenter,
			Alpha);
		const FQuat SubstepRotation = FQuat::Slerp(
			PreviousCapsuleRotation,
			CurrentCapsuleRotation,
			Alpha).GetNormalized();
		TraceRequest.Segments.Emplace(
			SubstepStart,
			SubstepEnd,
			SubstepRotation);
		SubstepStart = SubstepEnd;
	}

	const FAIRECombatMeleeTraceResolution Resolution =
		FAIRECombatMeleeTraceResolver::Resolve(TraceRequest);
	if (bHasPreviousSample)
	{
		PreviousTraceStart = CurrentTraceStart;
		PreviousTraceEnd = CurrentTraceEnd;
	}
	OutTargetHit = Resolution.HitResult;
	return Resolution.Result;
}

bool UAIRECompanionCombatSkillAbility::CommitSkillHit(
	const FHitResult& TargetHit)
{
	AActor* TargetActor = GetEventTarget();
	if (!IsActiveExecutionValid()
		|| !IsTargetValidForSkill(TargetActor)
		|| !FMath::IsFinite(ActiveDamage)
		|| ActiveDamage < 0.0f
		|| !FMath::IsFinite(ActiveStaggerValue)
		|| ActiveStaggerValue < 0.0f
		|| ActiveTargetingMode != EAIRECombatTargetingMode::SingleTarget)
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
	DamageRequest.Damage = ActiveDamage;
	DamageRequest.StaggerValue = ActiveStaggerValue;
	DamageRequest.ExecutionId = ActiveExecutionId;
	DamageRequest.bHasHitResult = true;
	DamageRequest.HitResult = TargetHit;
	const EAIRECombatDamageResult DamageResult =
		DamageSubsystem->ApplyDamageRequest(DamageRequest);
	if (DamageResult == EAIRECombatDamageResult::Applied)
	{
		AIRECompanionCombatVFX::SpawnBossHitSlash(
			ActiveWeaponDefinition,
			GetAvatarActorFromActorInfo(),
			TargetActor,
			TargetHit);
	}
	UE_LOG(
		LogAIRECompanionCombatSkill,
		Log,
		TEXT("[MAKO ATTACK] Type=CombatSkill Phase=DamageCommit Source=%s Target=%s ExecutionId=%s Damage=%.2f Stagger=%.2f Result=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(TargetActor),
		*ActiveExecutionId.ToString(),
		ActiveDamage,
		ActiveStaggerValue,
		*StaticEnum<EAIRECombatDamageResult>()->GetNameStringByValue(
			static_cast<int64>(DamageResult)));
	return DamageResult == EAIRECombatDamageResult::Applied;
}

void UAIRECompanionCombatSkillAbility::ResolveSkillTraceSample(
	const EAIRECombatMeleeTraceResult TraceResult,
	const FHitResult& TargetHit)
{
	if (bHitConsumed)
	{
		return;
	}
	if (TraceResult == EAIRECombatMeleeTraceResult::NoHit
		|| TraceResult == EAIRECombatMeleeTraceResult::Blocked)
	{
		const TCHAR* SampleResultName =
			TraceResult == EAIRECombatMeleeTraceResult::Blocked
				? TEXT("Blocked")
				: TEXT("NoHit");
		UE_LOG(
			LogAIRECompanionCombatSkill,
			Verbose,
			TEXT("[MAKO ATTACK] Type=CombatSkill Phase=SpatialSample Source=%s Target=%s ExecutionId=%s Result=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*GetNameSafe(GetEventTarget()),
			*ActiveExecutionId.ToString(),
			SampleResultName);
		return;
	}

	const TCHAR* TraceResultName =
		TraceResult == EAIRECombatMeleeTraceResult::TargetHit
			? TEXT("TargetHit")
			: TEXT("Invalid");
	UE_LOG(
		LogAIRECompanionCombatSkill,
		Log,
		TEXT("[MAKO ATTACK] Type=CombatSkill Phase=SpatialTerminal Source=%s Target=%s ExecutionId=%s Result=%s HitActor=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(GetEventTarget()),
		*ActiveExecutionId.ToString(),
		TraceResultName,
		*GetNameSafe(TargetHit.GetActor()));

	bHitConsumed = true;
	CloseSkillTrace();
	if (TraceResult == EAIRECombatMeleeTraceResult::TargetHit)
	{
		CommitSkillHit(TargetHit);
	}
}

bool UAIRECompanionCombatSkillAbility::TryBeginSkillTrace(
	USkeletalMeshComponent* MeshComponent)
{
	if (bHitConsumed
		|| bTraceWindowOpen
		|| bTraceWindowEverOpened
		|| !IsValid(MeshComponent)
		|| MeshComponent->GetOwner() != GetAvatarActorFromActorInfo())
	{
		return false;
	}

	ActiveTraceMesh = MeshComponent;
	bTraceWindowOpen = true;
	bTraceWindowEverOpened = true;
	if (MeshComponent->DoesSocketExist(ActiveTraceStartSocket)
		&& MeshComponent->DoesSocketExist(ActiveTraceEndSocket))
	{
		PreviousTraceStart =
			MeshComponent->GetSocketLocation(ActiveTraceStartSocket);
		PreviousTraceEnd =
			MeshComponent->GetSocketLocation(ActiveTraceEndSocket);
	}

	FHitResult TargetHit;
	const EAIRECombatMeleeTraceResult TraceResult =
		SampleSkillTrace(MeshComponent, TargetHit);
	ResolveSkillTraceSample(TraceResult, TargetHit);
	return TraceResult != EAIRECombatMeleeTraceResult::Invalid;
}

void UAIRECompanionCombatSkillAbility::CloseSkillTrace()
{
	bTraceWindowOpen = false;
	ActiveTraceMesh.Reset();
	PreviousTraceStart = FVector::ZeroVector;
	PreviousTraceEnd = FVector::ZeroVector;
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

void UAIRECompanionCombatSkillAbility::StartTraceEventWait()
{
	TraceEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		AIRECompanionGameplayTags::EventCombatSkillTrace,
		nullptr,
		false,
		false);
	if (!IsValid(TraceEventTask))
	{
		FinishAbility(true);
		return;
	}

	TraceEventTask->EventReceived.AddDynamic(
		this,
		&UAIRECompanionCombatSkillAbility::HandleTraceEvent);
	TraceEventTask->ReadyForActivation();
}

void UAIRECompanionCombatSkillAbility::StartFallback()
{
	bUsingFallback = true;
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
	if (bIsEnding || bHitConsumed || bPointSampleConsumed)
	{
		return;
	}

	AActor* TargetActor = GetEventTarget();
	if (IsValid(Payload.Target.Get())
		&& Payload.Target.Get() != TargetActor)
	{
		return;
	}

	bPointSampleConsumed = true;
	ResolveSkillHit();
}

void UAIRECompanionCombatSkillAbility::HandleTraceEvent(
	const FGameplayEventData Payload)
{
	if (bIsEnding || bHitConsumed)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent =
		Cast<USkeletalMeshComponent>(
			const_cast<UObject*>(Payload.OptionalObject.Get()));
	if (!IsValid(MeshComponent)
		|| MeshComponent->GetOwner() != GetAvatarActorFromActorInfo())
	{
		return;
	}

	if (Payload.EventTag.MatchesTagExact(
		AIRECompanionGameplayTags::EventCombatSkillTraceBegin))
	{
		TryBeginSkillTrace(MeshComponent);
		return;
	}

	if (!bTraceWindowOpen || ActiveTraceMesh.Get() != MeshComponent)
	{
		return;
	}

	if (Payload.EventTag.MatchesTagExact(
		AIRECompanionGameplayTags::EventCombatSkillTraceSample))
	{
		FHitResult TargetHit;
		const EAIRECombatMeleeTraceResult TraceResult =
			SampleSkillTrace(MeshComponent, TargetHit);
		ResolveSkillTraceSample(TraceResult, TargetHit);
		return;
	}

	if (Payload.EventTag.MatchesTagExact(
		AIRECompanionGameplayTags::EventCombatSkillTraceEnd))
	{
		FHitResult TargetHit;
		const EAIRECombatMeleeTraceResult TraceResult =
			SampleSkillTrace(MeshComponent, TargetHit);
		ResolveSkillTraceSample(TraceResult, TargetHit);
		if (TraceResult == EAIRECombatMeleeTraceResult::NoHit)
		{
			UE_LOG(
				LogAIRECompanionCombatSkill,
				Log,
				TEXT("[MAKO ATTACK] Type=CombatSkill Phase=SpatialTerminal Source=%s Target=%s ExecutionId=%s Result=Miss"),
				*GetNameSafe(GetAvatarActorFromActorInfo()),
				*GetNameSafe(GetEventTarget()),
				*ActiveExecutionId.ToString());
		}
		CloseSkillTrace();
	}
}

void UAIRECompanionCombatSkillAbility::HandleTargetDestroyed(
	AActor* DestroyedActor)
{
	if (DestroyedActor)
	{
		FinishAbility(true);
	}
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

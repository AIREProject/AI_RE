#include "AbilitySystem/Combat/Abilities/AIRECompanionMeleeAttackAbility.h"

#include "AbilitySystem/Combat/AIRECompanionCombatVFX.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionAttackCooldownGameplayEffect.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AIRECombatDamageSubsystem.h"
#include "AIRECombatMeleeTraceResolver.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/AIRECompanionAIController.h"
#include "Core/AIRECompanionCharacter.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Threat/AIRECompanionThreatComponent.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h" 
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "AI_REHarvestDamageTarget.h"
#include "AI_REHarvestableResourceActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionMeleeAttack, Log, All);

namespace
{
	constexpr int32 MeleeWeaponTraceSubstepCount = 6;

	FQuat MakeMeleeWeaponCapsuleRotation(
		const FVector& Start,
		const FVector& End)
	{
		const FVector Axis = (End - Start).GetSafeNormal();
		return Axis.IsNearlyZero()
			? FQuat::Identity
			: FQuat::FindBetweenNormals(FVector::UpVector, Axis);
	}

	FVector MakeMeleeWeaponCapsuleCenter(
		const FVector& Start,
		const FVector& End)
	{
		return (Start + End) * 0.5f;
	}
}

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
	ActivationBlockedTags.AddTag(
		AIRECompanionGameplayTags::StateActionEquipping);
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
	bHarvestWeaponVisibilityApplied = false;
	CurrentStepIndex = 0;
	ResumeStepIndex = INDEX_NONE;
	ActiveWeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	if (const AAIRECompanionCharacter* CompanionCharacter =
			Cast<AAIRECompanionCharacter>(
				ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (const UAIRECompanionEquipmentComponent* EquipmentComponent =
				CompanionCharacter->GetEquipmentComponent())
		{
			LastSelectedComboVariantIndex =
				EquipmentComponent->GetLastBasicComboVariantIndex(
					ActiveWeaponDefinition);
		}
	}
	const bool bHasEventTarget = InitializeEventTarget(TriggerEventData);
	ActiveExecutionMode = bHasEventTarget
		? ResolveExecutionMode(GetEventTarget())
		: EExecutionMode::None;
	InitializeComboVariantSelection();
	AttackRange = 0.0f;
	if (IsValid(ActiveWeaponDefinition))
	{
		AttackRange = ActiveExecutionMode == EExecutionMode::Harvest
			? ActiveWeaponDefinition->HarvestAttackRange
				+ AIRECompanionWeaponDefinition::HarvestRangeAcceptanceTolerance
			: ActiveWeaponDefinition->AttackRange;
	}
	ResetCurrentStepState();
	const bool bHasValidRange = FMath::IsFinite(AttackRange) && AttackRange >= 0.0f;
	if (!bHasEventTarget
		|| ActiveExecutionMode == EExecutionMode::None
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
	GetEventTarget()->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIRECompanionMeleeAttackAbility::HandleTargetDestroyed);
	ApplyHarvestWeaponVisibility();

	StartHitEventWait();
	if (bIsEnding)
	{
		return;
	}
	if (ActiveExecutionMode == EExecutionMode::Combat)
	{
		StartTraceEventWait();
		if (bIsEnding)
		{
			return;
		}
	}

	if (GetAttackStepCount() > 1)
	{
		StartComboWindowEventWait();
		if (bIsEnding)
		{
			return;
		}
	}

	if (ActiveExecutionMode == EExecutionMode::Combat)
	{
		StartCombatSkillTransitionEventWait();
		if (bIsEnding)
		{
			return;
		}
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
	RestoreHarvestWeaponVisibility();
	if (AActor* TargetActor = GetEventTarget())
	{
		TargetActor->OnDestroyed.RemoveDynamic(
			this,
			&UAIRECompanionMeleeAttackAbility::HandleTargetDestroyed);
	}
	UE_LOG(
		LogAIRECompanionMeleeAttack,
		Log,
		TEXT("[MAKO ATTACK] Type=Basic Phase=Ended Source=%s Target=%s Cancelled=%s StepIndex=%d TerminalSpatialResult=%s"),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
		*GetNameSafe(GetEventTarget()),
		bWasCancelled ? TEXT("true") : TEXT("false"),
		CurrentStepIndex,
		bCurrentStepHitConsumed ? TEXT("Resolved") : TEXT("Miss"));
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
	TraceEventTask = nullptr;
	ComboWindowEventTask = nullptr;
	CombatSkillTransitionEventTask = nullptr;
	ActiveWeaponDefinition = nullptr;
	AttackRange = 0.0f;
	CurrentStepDamage = 0.0f;
	CurrentStepStaggerValue = 0.0f;
	CurrentTraceRadius = 0.0f;
	CurrentTraceCapsuleHalfHeight = 0.0f;
	CurrentTraceChannel = ECC_MAX;
	CurrentTraceStartSocket = NAME_None;
	CurrentTraceEndSocket = NAME_None;
	CurrentStepTargetingMode = EAIRECombatTargetingMode::SingleTarget;
	CurrentStepExecutionId.Invalidate();
	ActiveExecutionMode = EExecutionMode::None;
	CurrentStepIndex = INDEX_NONE;
	ResumeStepIndex = INDEX_NONE;
	ActiveComboVariantIndex = INDEX_NONE;
	PendingComboVariantIndex = INDEX_NONE;
	bCurrentStepHitConsumed = false;
	bCurrentStepPointSampleConsumed = false;
	bTraceWindowEverOpened = false;
	CloseCurrentStepTrace();
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

	if (HasComboMontageVariants())
	{
		return ActiveWeaponDefinition->ComboMontageVariants.IsValidIndex(
			ActiveComboVariantIndex)
			? ActiveWeaponDefinition->ComboMontageVariants[
				ActiveComboVariantIndex].MontageSections.Num()
			: 0;
	}
	if (ActiveExecutionMode == EExecutionMode::Harvest
		&& !ActiveWeaponDefinition->ComboMontageVariants.IsEmpty())
	{
		return ActiveWeaponDefinition->ComboMontageVariants[0]
			.MontageSections.Num();
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

	if (!ActiveWeaponDefinition->ComboMontageVariants.IsEmpty())
	{
		return GetComboVariantMontageSection(
			HasComboMontageVariants() ? ActiveComboVariantIndex : 0,
			StepIndex);
	}

	return ActiveWeaponDefinition->ComboSteps[StepIndex].MontageSection;
}

FName UAIRECompanionMeleeAttackAbility::GetComboVariantMontageSection(
	const int32 VariantIndex,
	const int32 StepIndex) const
{
	if (!IsValid(ActiveWeaponDefinition)
		|| ActiveWeaponDefinition->ComboMontageVariants.IsEmpty()
		|| !ActiveWeaponDefinition->ComboMontageVariants.IsValidIndex(
			VariantIndex))
	{
		return NAME_None;
	}

	const FAIREWeaponComboMontageVariantDefinition& Variant =
		ActiveWeaponDefinition->ComboMontageVariants[VariantIndex];
	return Variant.MontageSections.IsValidIndex(StepIndex)
		? Variant.MontageSections[StepIndex]
		: NAME_None;
}

bool UAIRECompanionMeleeAttackAbility::HasComboMontageVariants() const
{
	return ActiveExecutionMode == EExecutionMode::Combat
		&& IsValid(ActiveWeaponDefinition)
		&& !ActiveWeaponDefinition->ComboSteps.IsEmpty()
		&& !ActiveWeaponDefinition->ComboMontageVariants.IsEmpty();
}

void UAIRECompanionMeleeAttackAbility::InitializeComboVariantSelection()
{
	ActiveComboVariantIndex = INDEX_NONE;
	PendingComboVariantIndex = INDEX_NONE;
	if (!HasComboMontageVariants())
	{
		return;
	}

	if (!bComboVariantRandomInitialized)
	{
		ComboVariantRandomStream.Initialize(FMath::Rand());
		bComboVariantRandomInitialized = true;
	}

	ActiveComboVariantIndex =
		AIRECompanionWeaponDefinition::SelectNonRepeatingComboVariantIndex(
			ActiveWeaponDefinition->ComboMontageVariants.Num(),
			LastSelectedComboVariantIndex,
			ComboVariantRandomStream);
	LastSelectedComboVariantIndex = ActiveComboVariantIndex;
	CacheLastSelectedComboVariant();
	PrepareNextComboVariant();
}

void UAIRECompanionMeleeAttackAbility::PrepareNextComboVariant()
{
	PendingComboVariantIndex = HasComboMontageVariants()
		? AIRECompanionWeaponDefinition::
			SelectNonRepeatingComboVariantIndex(
				ActiveWeaponDefinition->ComboMontageVariants.Num(),
				ActiveComboVariantIndex,
				ComboVariantRandomStream)
		: INDEX_NONE;
}

void UAIRECompanionMeleeAttackAbility::CacheLastSelectedComboVariant() const
{
	AAIRECompanionCharacter* CompanionCharacter =
		Cast<AAIRECompanionCharacter>(GetAvatarActorFromActorInfo());
	UAIRECompanionEquipmentComponent* EquipmentComponent =
		IsValid(CompanionCharacter)
			? CompanionCharacter->GetEquipmentComponent()
			: nullptr;
	if (IsValid(EquipmentComponent))
	{
		EquipmentComponent->SetLastBasicComboVariantIndex(
			ActiveWeaponDefinition,
			LastSelectedComboVariantIndex);
	}
}

void UAIRECompanionMeleeAttackAbility::ConfigureCurrentComboMontageLinks()
{
	if (!HasComboMontageVariants())
	{
		return;
	}

	for (int32 VariantIndex = 0;
		VariantIndex < ActiveWeaponDefinition->ComboMontageVariants.Num();
		++VariantIndex)
	{
		const FAIREWeaponComboMontageVariantDefinition& Variant =
			ActiveWeaponDefinition->ComboMontageVariants[VariantIndex];
		for (const FName SectionName : Variant.MontageSections)
		{
			MontageSetNextSectionName(SectionName, NAME_None);
		}
	}

	if (CurrentStepIndex + 1 < GetAttackStepCount())
	{
		MontageSetNextSectionName(
			GetAttackStepMontageSection(CurrentStepIndex),
			GetAttackStepMontageSection(CurrentStepIndex + 1));
	}

	const FName LastSection = GetComboVariantMontageSection(
		ActiveComboVariantIndex,
		GetAttackStepCount() - 1);
	const FName PendingFirstSection = GetComboVariantMontageSection(
		PendingComboVariantIndex,
		0);
	if (!LastSection.IsNone() && !PendingFirstSection.IsNone())
	{
		MontageSetNextSectionName(LastSection, PendingFirstSection);
	}
}

float UAIRECompanionMeleeAttackAbility::GetAttackStepStaggerValue(
	const int32 StepIndex) const
{
	if (!IsAttackStepIndexValid(StepIndex))
	{
		return 0.0f;
	}

	return ActiveWeaponDefinition->ComboSteps.IsEmpty()
		? ActiveWeaponDefinition->StaggerValue
		: ActiveWeaponDefinition->ComboSteps[StepIndex].StaggerValue;
}

EAIRECombatTargetingMode
UAIRECompanionMeleeAttackAbility::GetAttackStepTargetingMode(
	const int32 StepIndex) const
{
	if (!IsAttackStepIndexValid(StepIndex))
	{
		return EAIRECombatTargetingMode::Area;
	}
	return ActiveWeaponDefinition->ComboSteps.IsEmpty()
		? ActiveWeaponDefinition->TargetingMode
		: ActiveWeaponDefinition->ComboSteps[StepIndex].TargetingMode;
}

UAIRECompanionMeleeAttackAbility::EExecutionMode
UAIRECompanionMeleeAttackAbility::ResolveExecutionMode(
	const AActor* TargetActor) const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AAIRECompanionAIController* CompanionController =
		IsValid(AvatarPawn)
			? Cast<AAIRECompanionAIController>(AvatarPawn->GetController())
			: nullptr;
	const UAIRECompanionThreatComponent* ThreatComponent =
		IsValid(CompanionController)
			? CompanionController->GetThreatComponent()
			: nullptr;
	if (IsValid(ThreatComponent)
		&& ThreatComponent->IsCombatRequested()
		&& IsValid(ThreatComponent->GetSelectedThreatTarget()))
	{
		return ThreatComponent->GetSelectedThreatTarget() == TargetActor
			? EExecutionMode::Combat
			: EExecutionMode::None;
	}

	if (IsValid(TargetActor)
		&& TargetActor->GetClass()->ImplementsInterface(
			UAI_REHarvestDamageTarget::StaticClass()))
	{
		const AAIRECompanionCharacter* CompanionCharacter =
			Cast<AAIRECompanionCharacter>(AvatarPawn);
		const UAIRECompanionWorkOrderComponent* WorkOrderComponent =
			IsValid(CompanionCharacter)
				? CompanionCharacter->GetWorkOrderComponent()
				: nullptr;
		if (IsValid(WorkOrderComponent))
		{
			const FAIRECompanionWorkOrderSnapshot Snapshot =
				WorkOrderComponent->GetWorkOrderSnapshot();
			if (Snapshot.WorkType
					== EAIRECompanionWorkOrderType::Harvesting
				&& Snapshot.State
					== EAIRECompanionWorkOrderState::Working
				&& Snapshot.TargetActor.Get() == TargetActor)
			{
				return EExecutionMode::Harvest;
			}
		}
	}

	return IsTargetValidForAttack(TargetActor)
		? EExecutionMode::Combat
		: EExecutionMode::None;
}

bool UAIRECompanionMeleeAttackAbility::IsTargetValidForAttack(
	const AActor* TargetActor) const
{
	if (ActiveExecutionMode == EExecutionMode::Harvest)
	{
		const AAI_REHarvestableResourceActor* ResourceActor =
			Cast<AAI_REHarvestableResourceActor>(TargetActor);
		const UAI_REHarvestableResourceComponent* ResourceComponent =
			IsValid(ResourceActor)
				? ResourceActor->GetHarvestableResourceComponent()
				: nullptr;
		return IsValid(ResourceComponent)
			&& !ResourceComponent->IsDepleted();
	}

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

	if (const AAI_REHarvestableResourceActor* ResourceActor =
			Cast<AAI_REHarvestableResourceActor>(TargetActor))
	{
		FVector InteractionLocation;
		if (!ResourceActor->TryGetHarvestInteractionLocation(
				AvatarActor->GetActorLocation(),
				InteractionLocation))
		{
			return false;
		}

		const float HorizontalDistance = FVector::Dist2D(
			AvatarActor->GetActorLocation(),
			InteractionLocation);
		const float EffectiveDistance = FMath::Max(
			0.0f,
			HorizontalDistance - AvatarActor->GetSimpleCollisionRadius());
		return EffectiveDistance <= AttackRange;
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
	if (ActiveExecutionMode == EExecutionMode::Harvest)
	{
		const AAIRECompanionCharacter* CompanionCharacter =
			Cast<AAIRECompanionCharacter>(AvatarPawn);
		const UAIRECompanionWorkOrderComponent* WorkOrderComponent =
			IsValid(CompanionCharacter)
				? CompanionCharacter->GetWorkOrderComponent()
				: nullptr;
		if (!IsValid(WorkOrderComponent))
		{
			return false;
		}

		const FAIRECompanionWorkOrderSnapshot Snapshot =
			WorkOrderComponent->GetWorkOrderSnapshot();
		return Snapshot.WorkType
				== EAIRECompanionWorkOrderType::Harvesting
			&& Snapshot.State
				== EAIRECompanionWorkOrderState::Working
			&& Snapshot.TargetActor.Get() == GetEventTarget()
			&& IsTargetValidForAttack(GetEventTarget());
	}

	// The activation target is the strike snapshot. Threat reselection or a
	// range change must not replace or cancel an in-flight attack sample.
	return IsTargetValidForAttack(GetEventTarget());
}

bool UAIRECompanionMeleeAttackAbility::AreComboMontageSectionsValid(
	const UAnimMontage* AttackMontage) const
{
	if (!IsValid(AttackMontage) || !IsValid(ActiveWeaponDefinition))
	{
		return false;
	}

	if (!ActiveWeaponDefinition->ComboMontageVariants.IsEmpty())
	{
		for (const FAIREWeaponComboMontageVariantDefinition& Variant
			: ActiveWeaponDefinition->ComboMontageVariants)
		{
			if (Variant.MontageSections.IsEmpty()
				|| Variant.MontageSections.Num()
					> ActiveWeaponDefinition->ComboSteps.Num())
			{
				return false;
			}

			for (const FName SectionName : Variant.MontageSections)
			{
				if (SectionName.IsNone()
					|| AttackMontage->GetSectionIndex(SectionName)
						== INDEX_NONE)
				{
					return false;
				}
			}
		}
		return true;
	}

	for (const FAIREWeaponComboStepDefinition& ComboStep
		: ActiveWeaponDefinition->ComboSteps)
	{
		if (AttackMontage->GetSectionIndex(ComboStep.MontageSection)
			== INDEX_NONE)
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

void UAIRECompanionMeleeAttackAbility::PrepareComboLoopStep(
	const int32 PayloadStepIndex,
	const int32 PayloadVariantIndex)
{
	const int32 LastStepIndex = GetAttackStepCount() - 1;
	if (PayloadStepIndex != 0
		|| CurrentStepIndex != LastStepIndex
		|| (HasComboMontageVariants()
			&& PayloadVariantIndex != PendingComboVariantIndex))
	{
		return;
	}

	AActor* TargetActor = GetEventTarget();
	bool bCanContinueLoop = IsActiveExecutionValid()
		&& IsTargetInRange(TargetActor);
	if (bCanContinueLoop && ActiveExecutionMode == EExecutionMode::Combat)
	{
		const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		const AAIRECompanionAIController* CompanionController =
			IsValid(AvatarPawn)
				? Cast<AAIRECompanionAIController>(AvatarPawn->GetController())
				: nullptr;
		const UAIRECompanionThreatComponent* ThreatComponent =
			IsValid(CompanionController)
				? CompanionController->GetThreatComponent()
				: nullptr;
		bCanContinueLoop = IsValid(ThreatComponent)
			&& ThreatComponent->IsCombatRequested()
			&& ThreatComponent->GetSelectedThreatTarget() == TargetActor;
	}

	if (!bCanContinueLoop)
	{
		FinishAbility(false);
		return;
	}

	if (HasComboMontageVariants())
	{
		if (!ActiveWeaponDefinition->ComboMontageVariants.IsValidIndex(
			PendingComboVariantIndex))
		{
			FinishAbility(true);
			return;
		}

		ActiveComboVariantIndex = PendingComboVariantIndex;
		LastSelectedComboVariantIndex = ActiveComboVariantIndex;
		CacheLastSelectedComboVariant();
		PrepareNextComboVariant();
	}

	CurrentStepIndex = 0;
	ResetCurrentStepState();
	FaceTarget(TargetActor);
	ConfigureCurrentComboMontageLinks();
	UE_LOG(
		LogAIRECompanionMeleeAttack,
		Verbose,
		TEXT("Companion combo looped without returning to locomotion. Mode=%s Target=%s Variant=%d PendingVariant=%d"),
		ActiveExecutionMode == EExecutionMode::Harvest
			? TEXT("Harvest")
			: TEXT("Combat"),
		*GetNameSafe(TargetActor),
		ActiveComboVariantIndex,
		PendingComboVariantIndex);
}

void UAIRECompanionMeleeAttackAbility::ApplyHarvestWeaponVisibility()
{
	if (ActiveExecutionMode != EExecutionMode::Harvest)
	{
		return;
	}

	AAIRECompanionCharacter* CompanionCharacter =
		Cast<AAIRECompanionCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(CompanionCharacter))
	{
		return;
	}

	bPreviousBackWeaponsVisible =
		CompanionCharacter->AreBackWeaponsVisible();
	bPreviousHandWeaponsVisible =
		CompanionCharacter->AreHandWeaponsVisible();
	const bool bIsKatana = ActiveWeaponDefinition->WeaponTag.MatchesTagExact(
		AIRECompanionGameplayTags::WeaponCompanionMeleeKatana);
	CompanionCharacter->SetBackWeaponsVisible(false);
	CompanionCharacter->SetHandWeaponsVisible(!bIsKatana);
	bHarvestWeaponVisibilityApplied = true;
}

void UAIRECompanionMeleeAttackAbility::RestoreHarvestWeaponVisibility()
{
	if (!bHarvestWeaponVisibilityApplied)
	{
		return;
	}

	bHarvestWeaponVisibilityApplied = false;
	AAIRECompanionCharacter* CompanionCharacter =
		Cast<AAIRECompanionCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(CompanionCharacter))
	{
		return;
	}

	const AAIRECompanionAIController* CompanionController =
		Cast<AAIRECompanionAIController>(CompanionCharacter->GetController());
	const UAIRECompanionThreatComponent* ThreatComponent =
		IsValid(CompanionController)
			? CompanionController->GetThreatComponent()
			: nullptr;
	if (IsValid(ThreatComponent)
		&& ThreatComponent->IsCombatRequested()
		&& IsValid(ThreatComponent->GetSelectedThreatTarget()))
	{
		CompanionCharacter->SetCombatEquipmentActive(true);
		return;
	}

	CompanionCharacter->SetBackWeaponsVisible(
		bPreviousBackWeaponsVisible);
	CompanionCharacter->SetHandWeaponsVisible(
		bPreviousHandWeaponsVisible);
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

	if (HasComboMontageVariants())
	{
		ConfigureCurrentComboMontageLinks();
	}
	else if (!ActiveWeaponDefinition->ComboSteps.IsEmpty())
	{
		for (const FAIREWeaponComboStepDefinition& ComboStep
			: ActiveWeaponDefinition->ComboSteps)
		{
			MontageSetNextSectionName(
				ComboStep.MontageSection,
				NAME_None);
		}

		MontageSetNextSectionName(
			GetAttackStepMontageSection(GetAttackStepCount() - 1),
			GetAttackStepMontageSection(0));

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
	int32& OutStepIndex,
	int32& OutVariantIndex) const
{
	OutStepIndex = INDEX_NONE;
	OutVariantIndex = INDEX_NONE;
	if (!FMath::IsFinite(Payload.EventMagnitude))
	{
		return false;
	}

	const int32 PayloadStepIndex = FMath::RoundToInt(Payload.EventMagnitude);
	if (!FMath::IsNearlyEqual(
		Payload.EventMagnitude,
		static_cast<float>(PayloadStepIndex)))
	{
		return false;
	}

	if (bUsingFallback)
	{
		if (!IsAttackStepIndexValid(PayloadStepIndex))
		{
			return false;
		}
		OutStepIndex = PayloadStepIndex;
		OutVariantIndex = ActiveComboVariantIndex;
		return true;
	}

	if (HasComboMontageVariants())
	{
		return AIRECompanionWeaponDefinition::ResolveComboVariantStepIndex(
			ActiveWeaponDefinition->ComboMontageVariants,
			PayloadStepIndex,
			OutVariantIndex,
			OutStepIndex);
	}

	if (!IsAttackStepIndexValid(PayloadStepIndex))
	{
		return false;
	}

	OutStepIndex = PayloadStepIndex;
	return true;
}

bool UAIRECompanionMeleeAttackAbility::IsEventVariantActive(
	const int32 PayloadVariantIndex) const
{
	return !HasComboMontageVariants()
		|| PayloadVariantIndex == ActiveComboVariantIndex;
}

bool UAIRECompanionMeleeAttackAbility::ResolveCurrentStepHit()
{
	AActor* TargetActor = GetEventTarget();
	if (!IsActiveExecutionValid()
		|| !IsTargetValidForAttack(TargetActor))
	{
		return false;
	}

	if (!FMath::IsFinite(CurrentStepDamage) || CurrentStepDamage < 0.0f)
	{
		return false;
	}

	if (ActiveExecutionMode == EExecutionMode::Harvest)
	{
		if (CurrentStepDamage <= 0.0f
			|| !IsTargetInRange(TargetActor))
		{
			return false;
		}
		const bool bAppliedHarvestDamage =
			IAI_REHarvestDamageTarget::Execute_ApplyHarvestDamage(
				TargetActor,
				CurrentStepDamage,
				GetAvatarActorFromActorInfo());
		if (bAppliedHarvestDamage)
		{
			UE_LOG(
				LogAIRECompanionMeleeAttack,
				Log,
				TEXT("Companion harvest hit applied. Source=%s Target=%s Step=%d Damage=%.2f"),
				*GetNameSafe(GetAvatarActorFromActorInfo()),
				*GetNameSafe(TargetActor),
				CurrentStepIndex,
				CurrentStepDamage);
		}
		return bAppliedHarvestDamage;
	}

	const ACharacter* AvatarCharacter =
		Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* MeshComponent = IsValid(AvatarCharacter)
		? AvatarCharacter->GetMesh()
		: nullptr;
	FHitResult TargetHit;
	const EAIRECombatMeleeTraceResult TraceResult =
		SampleCurrentStepCombatTrace(MeshComponent, TargetHit);
	ResolveCurrentStepTraceSample(TraceResult, TargetHit);
	return TraceResult == EAIRECombatMeleeTraceResult::TargetHit;
}

EAIRECombatMeleeTraceResult
UAIRECompanionMeleeAttackAbility::SampleCurrentStepCombatTrace(
	USkeletalMeshComponent* MeshComponent,
	FHitResult& OutTargetHit)
{
	AActor* SourceActor = GetAvatarActorFromActorInfo();
	AActor* TargetActor = GetEventTarget();
	const bool bCommonTraceInputsValid =
		IsActiveExecutionValid()
		&& ActiveExecutionMode == EExecutionMode::Combat
		&& IsValid(SourceActor)
		&& IsValid(TargetActor)
		&& CurrentStepTargetingMode
			== EAIRECombatTargetingMode::SingleTarget
		&& FMath::IsFinite(CurrentTraceRadius)
		&& CurrentTraceRadius > 0.0f
		&& FMath::IsFinite(CurrentTraceCapsuleHalfHeight)
		&& CurrentTraceCapsuleHalfHeight >= CurrentTraceRadius
		&& CurrentTraceChannel.GetValue() < ECC_MAX;
	if (!bCommonTraceInputsValid)
	{
		return EAIRECombatMeleeTraceResult::Invalid;
	}

	if (bUsingFallback)
	{
		FVector Forward = SourceActor->GetActorForwardVector().GetSafeNormal2D();
		if (Forward.IsNearlyZero()
			|| !FMath::IsFinite(AttackRange)
			|| AttackRange < 0.0f)
		{
			return EAIRECombatMeleeTraceResult::Invalid;
		}

		const FVector TraceStart = SourceActor->GetActorLocation()
			+ Forward * SourceActor->GetSimpleCollisionRadius();
		const float CenterTravelDistance = FMath::Max(
			0.0f,
			AttackRange - CurrentTraceRadius);
		const FVector TraceEnd =
			TraceStart + Forward * CenterTravelDistance;
		const FVector CapsuleCenter = TraceStart
			+ Forward * FMath::Max(
				0.0f,
				CurrentTraceCapsuleHalfHeight - CurrentTraceRadius);
		FAIRECombatMeleeTraceRequest TraceRequest;
		TraceRequest.World = GetWorld();
		TraceRequest.Source = SourceActor;
		TraceRequest.Target = TargetActor;
		TraceRequest.Shape = EAIRECombatMeleeTraceShape::Capsule;
		TraceRequest.Radius = CurrentTraceRadius;
		TraceRequest.CapsuleHalfHeight = CurrentTraceCapsuleHalfHeight;
		TraceRequest.TraceChannel = CurrentTraceChannel.GetValue();
		TraceRequest.Segments.Emplace(
			CapsuleCenter,
			CapsuleCenter,
			MakeMeleeWeaponCapsuleRotation(TraceStart, TraceEnd));
		const FAIRECombatMeleeTraceResolution Resolution =
			FAIRECombatMeleeTraceResolver::Resolve(TraceRequest);
		OutTargetHit = Resolution.HitResult;
		return Resolution.Result;
	}

	const bool bSocketTraceInputsValid =
		IsValid(MeshComponent)
		&& MeshComponent->GetOwner() == SourceActor
		&& !CurrentTraceStartSocket.IsNone()
		&& !CurrentTraceEndSocket.IsNone()
		&& MeshComponent->DoesSocketExist(CurrentTraceStartSocket)
		&& MeshComponent->DoesSocketExist(CurrentTraceEndSocket);
	if (!bSocketTraceInputsValid)
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Warning,
			TEXT("Companion melee trace rejected invalid source or sockets. Source=%s Mesh=%s Start=%s End=%s Step=%d"),
			*GetNameSafe(SourceActor),
			*GetNameSafe(MeshComponent),
			*CurrentTraceStartSocket.ToString(),
			*CurrentTraceEndSocket.ToString(),
			CurrentStepIndex);
		return EAIRECombatMeleeTraceResult::Invalid;
	}

	const FVector CurrentTraceStart =
		MeshComponent->GetSocketLocation(CurrentTraceStartSocket);
	const FVector CurrentTraceEnd =
		MeshComponent->GetSocketLocation(CurrentTraceEndSocket);
	const bool bHasPreviousSample =
		bTraceWindowOpen && ActiveTraceMesh.Get() == MeshComponent;
	const FVector TracePreviousStart = bHasPreviousSample
		? PreviousTraceStart
		: CurrentTraceStart;
	const FVector TracePreviousEnd = bHasPreviousSample
		? PreviousTraceEnd
		: CurrentTraceEnd;
	const FVector PreviousCapsuleCenter = MakeMeleeWeaponCapsuleCenter(
		TracePreviousStart,
		TracePreviousEnd);
	const FVector CurrentCapsuleCenter = MakeMeleeWeaponCapsuleCenter(
		CurrentTraceStart,
		CurrentTraceEnd);
	const FQuat PreviousCapsuleRotation = MakeMeleeWeaponCapsuleRotation(
		TracePreviousStart,
		TracePreviousEnd);
	const FQuat CurrentCapsuleRotation = MakeMeleeWeaponCapsuleRotation(
		CurrentTraceStart,
		CurrentTraceEnd);

	FAIRECombatMeleeTraceRequest TraceRequest;
	TraceRequest.World = GetWorld();
	TraceRequest.Source = SourceActor;
	TraceRequest.Target = TargetActor;
	TraceRequest.Shape = EAIRECombatMeleeTraceShape::Capsule;
	TraceRequest.Radius = CurrentTraceRadius;
	TraceRequest.CapsuleHalfHeight = CurrentTraceCapsuleHalfHeight;
	TraceRequest.TraceChannel = CurrentTraceChannel.GetValue();
	TraceRequest.Segments.Reserve(MeleeWeaponTraceSubstepCount);
	FVector SubstepStart = PreviousCapsuleCenter;
	for (int32 SubstepIndex = 1;
		SubstepIndex <= MeleeWeaponTraceSubstepCount;
		++SubstepIndex)
	{
		const float Alpha = static_cast<float>(SubstepIndex)
			/ static_cast<float>(MeleeWeaponTraceSubstepCount);
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

bool UAIRECompanionMeleeAttackAbility::CommitCurrentStepCombatHit(
	const FHitResult& TargetHit)
{
	AActor* TargetActor = GetEventTarget();
	if (!IsActiveExecutionValid()
		|| !IsTargetValidForAttack(TargetActor)
		|| !FMath::IsFinite(CurrentStepDamage)
		|| CurrentStepDamage < 0.0f
		|| !FMath::IsFinite(CurrentStepStaggerValue)
		|| CurrentStepStaggerValue < 0.0f
		|| CurrentStepTargetingMode
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
	DamageRequest.Damage = CurrentStepDamage;
	DamageRequest.StaggerValue = CurrentStepStaggerValue;
	DamageRequest.ExecutionId = CurrentStepExecutionId;
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
		LogAIRECompanionMeleeAttack,
		Log,
		TEXT("[MAKO ATTACK] Type=Basic Phase=DamageCommit Source=%s Target=%s StepIndex=%d ExecutionId=%s Damage=%.2f Stagger=%.2f Result=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(TargetActor),
		CurrentStepIndex,
		*CurrentStepExecutionId.ToString(),
		CurrentStepDamage,
		CurrentStepStaggerValue,
		*StaticEnum<EAIRECombatDamageResult>()->GetNameStringByValue(
			static_cast<int64>(DamageResult)));
	return DamageResult == EAIRECombatDamageResult::Applied;
}

void UAIRECompanionMeleeAttackAbility::ResolveCurrentStepTraceSample(
	const EAIRECombatMeleeTraceResult TraceResult,
	const FHitResult& TargetHit)
{
	if (bCurrentStepHitConsumed)
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
			LogAIRECompanionMeleeAttack,
			Verbose,
			TEXT("[MAKO ATTACK] Type=Basic Phase=SpatialSample Source=%s Target=%s StepIndex=%d ExecutionId=%s Result=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*GetNameSafe(GetEventTarget()),
			CurrentStepIndex,
			*CurrentStepExecutionId.ToString(),
			SampleResultName);
		return;
	}

	const TCHAR* TraceResultName =
		TraceResult == EAIRECombatMeleeTraceResult::TargetHit
			? TEXT("TargetHit")
			: TEXT("Invalid");
	UE_LOG(
		LogAIRECompanionMeleeAttack,
		Log,
		TEXT("[MAKO ATTACK] Type=Basic Phase=SpatialTerminal Source=%s Target=%s StepIndex=%d ExecutionId=%s Result=%s HitActor=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(GetEventTarget()),
		CurrentStepIndex,
		*CurrentStepExecutionId.ToString(),
		TraceResultName,
		*GetNameSafe(TargetHit.GetActor()));

	bCurrentStepHitConsumed = true;
	CloseCurrentStepTrace();
	if (TraceResult == EAIRECombatMeleeTraceResult::TargetHit)
	{
		CommitCurrentStepCombatHit(TargetHit);
	}
}

bool UAIRECompanionMeleeAttackAbility::TryBeginCurrentStepTrace(
	USkeletalMeshComponent* MeshComponent)
{
	if (bCurrentStepHitConsumed
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
	if (MeshComponent->DoesSocketExist(CurrentTraceStartSocket)
		&& MeshComponent->DoesSocketExist(CurrentTraceEndSocket))
	{
		PreviousTraceStart =
			MeshComponent->GetSocketLocation(CurrentTraceStartSocket);
		PreviousTraceEnd =
			MeshComponent->GetSocketLocation(CurrentTraceEndSocket);
	}

	FHitResult TargetHit;
	const EAIRECombatMeleeTraceResult TraceResult =
		SampleCurrentStepCombatTrace(MeshComponent, TargetHit);
	ResolveCurrentStepTraceSample(TraceResult, TargetHit);
	return TraceResult != EAIRECombatMeleeTraceResult::Invalid;
}

void UAIRECompanionMeleeAttackAbility::CloseCurrentStepTrace()
{
	bTraceWindowOpen = false;
	ActiveTraceMesh.Reset();
	PreviousTraceStart = FVector::ZeroVector;
	PreviousTraceEnd = FVector::ZeroVector;
}

void UAIRECompanionMeleeAttackAbility::ResetCurrentStepState()
{
	CloseCurrentStepTrace();
	CurrentStepExecutionId = FGuid::NewGuid();
	bCurrentStepHitConsumed = false;
	bCurrentStepPointSampleConsumed = false;
	bTraceWindowEverOpened = false;
	bComboWindowOpen = false;
	bNextStepQueued = false;
	CurrentStepDamage = GetAttackStepDamage(CurrentStepIndex);
	CurrentStepStaggerValue =
		GetAttackStepStaggerValue(CurrentStepIndex);
	CurrentStepTargetingMode =
		GetAttackStepTargetingMode(CurrentStepIndex);
	CurrentTraceRadius = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->TraceCapsuleRadius
		: 0.0f;
	CurrentTraceCapsuleHalfHeight = IsValid(ActiveWeaponDefinition)
		? ActiveWeaponDefinition->TraceCapsuleHalfHeight
		: 0.0f;
	CurrentTraceChannel = ECC_MAX;
	if (IsValid(ActiveWeaponDefinition))
	{
		CurrentTraceChannel = ActiveWeaponDefinition->TraceChannel;
	}
	CurrentTraceStartSocket = NAME_None;
	CurrentTraceEndSocket = NAME_None;
	if (IsAttackStepIndexValid(CurrentStepIndex))
	{
		const FAIREWeaponComboStepDefinition* ComboStep =
			ActiveWeaponDefinition->ComboSteps.IsEmpty()
				? nullptr
				: &ActiveWeaponDefinition->ComboSteps[CurrentStepIndex];
		const EAIRECompanionWeaponTraceSide TraceSide = ComboStep
			? ComboStep->TraceSide
			: EAIRECompanionWeaponTraceSide::Right;
		const FAIREWeaponTraceSocketPair EmptyOverride;
		const FAIREWeaponTraceSocketPair TraceSockets =
			ActiveWeaponDefinition->ResolveTraceSockets(
				TraceSide,
				ComboStep
					? ComboStep->TraceSocketOverride
					: EmptyOverride);
		CurrentTraceStartSocket = TraceSockets.TraceStartSocket;
		CurrentTraceEndSocket = TraceSockets.TraceEndSocket;
	}
	if (ActiveExecutionMode == EExecutionMode::Combat)
	{
		UE_LOG(
			LogAIRECompanionMeleeAttack,
			Log,
			TEXT("[MAKO ATTACK] Type=Basic Phase=StrikeSnapshot Source=%s Target=%s StepIndex=%d ExecutionId=%s Damage=%.2f Stagger=%.2f CapsuleRadius=%.1f CapsuleHalfHeight=%.1f Sockets=%s->%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*GetNameSafe(GetEventTarget()),
			CurrentStepIndex,
			*CurrentStepExecutionId.ToString(),
			CurrentStepDamage,
			CurrentStepStaggerValue,
			CurrentTraceRadius,
			CurrentTraceCapsuleHalfHeight,
			*CurrentTraceStartSocket.ToString(),
			*CurrentTraceEndSocket.ToString());
	}
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

void UAIRECompanionMeleeAttackAbility::StartTraceEventWait()
{
	TraceEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		AIRECompanionGameplayTags::EventAttackTrace,
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
		&UAIRECompanionMeleeAttackAbility::HandleTraceEvent);
	TraceEventTask->ReadyForActivation();
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
	int32 PayloadVariantIndex = INDEX_NONE;
	if (bIsEnding
		|| bSuspendedForCombatSkill
		|| !TryGetEventStepIndex(
			Payload,
			PayloadStepIndex,
			PayloadVariantIndex))
	{
		return;
	}

	PrepareComboLoopStep(PayloadStepIndex, PayloadVariantIndex);
	if (bIsEnding
		|| bCurrentStepHitConsumed
		|| bCurrentStepPointSampleConsumed
		|| !IsEventVariantActive(PayloadVariantIndex)
		|| PayloadStepIndex != CurrentStepIndex)
	{
		return;
	}

	AActor* TargetActor = GetEventTarget();
	if (IsValid(Payload.Target.Get()) && Payload.Target.Get() != TargetActor)
	{
		return;
	}

	bCurrentStepPointSampleConsumed = true;
	if (ActiveExecutionMode == EExecutionMode::Harvest)
	{
		bCurrentStepHitConsumed = true;
		ResolveCurrentStepHit();
		if (!IsTargetValidForAttack(TargetActor))
		{
			FinishAbility(false);
		}
		return;
	}

	ResolveCurrentStepHit();
}

void UAIRECompanionMeleeAttackAbility::HandleTraceEvent(
	const FGameplayEventData Payload)
{
	int32 PayloadStepIndex = INDEX_NONE;
	int32 PayloadVariantIndex = INDEX_NONE;
	if (bIsEnding
		|| bSuspendedForCombatSkill
		|| ActiveExecutionMode != EExecutionMode::Combat
		|| !TryGetEventStepIndex(
			Payload,
			PayloadStepIndex,
			PayloadVariantIndex))
	{
		return;
	}

	PrepareComboLoopStep(PayloadStepIndex, PayloadVariantIndex);
	if (bIsEnding
		|| bCurrentStepHitConsumed
		|| !IsEventVariantActive(PayloadVariantIndex)
		|| PayloadStepIndex != CurrentStepIndex)
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
		AIRECompanionGameplayTags::EventAttackTraceBegin))
	{
		TryBeginCurrentStepTrace(MeshComponent);
		return;
	}

	if (!bTraceWindowOpen || ActiveTraceMesh.Get() != MeshComponent)
	{
		return;
	}

	if (Payload.EventTag.MatchesTagExact(
		AIRECompanionGameplayTags::EventAttackTraceSample))
	{
		FHitResult TargetHit;
		const EAIRECombatMeleeTraceResult TraceResult =
			SampleCurrentStepCombatTrace(MeshComponent, TargetHit);
		ResolveCurrentStepTraceSample(TraceResult, TargetHit);
		return;
	}

	if (Payload.EventTag.MatchesTagExact(
		AIRECompanionGameplayTags::EventAttackTraceEnd))
	{
		FHitResult TargetHit;
		const EAIRECombatMeleeTraceResult TraceResult =
			SampleCurrentStepCombatTrace(MeshComponent, TargetHit);
		ResolveCurrentStepTraceSample(TraceResult, TargetHit);
		if (TraceResult == EAIRECombatMeleeTraceResult::NoHit)
		{
			UE_LOG(
				LogAIRECompanionMeleeAttack,
				Log,
				TEXT("[MAKO ATTACK] Type=Basic Phase=SpatialTerminal Source=%s Target=%s StepIndex=%d ExecutionId=%s Result=Miss"),
				*GetNameSafe(GetAvatarActorFromActorInfo()),
				*GetNameSafe(GetEventTarget()),
				CurrentStepIndex,
				*CurrentStepExecutionId.ToString());
		}
		CloseCurrentStepTrace();
	}
}

void UAIRECompanionMeleeAttackAbility::HandleTargetDestroyed(
	AActor* DestroyedActor)
{
	if (DestroyedActor)
	{
		FinishAbility(true);
	}
}

void UAIRECompanionMeleeAttackAbility::HandleComboWindowEvent(
	const FGameplayEventData Payload)
{
	int32 PayloadStepIndex = INDEX_NONE;
	int32 PayloadVariantIndex = INDEX_NONE;
	if (bIsEnding
		|| bUsingFallback
		|| !TryGetEventStepIndex(
			Payload,
			PayloadStepIndex,
			PayloadVariantIndex))
	{
		return;
	}

	PrepareComboLoopStep(PayloadStepIndex, PayloadVariantIndex);
	if (bIsEnding
		|| !IsActiveExecutionValid()
		|| !IsEventVariantActive(PayloadVariantIndex)
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
		CloseCurrentStepTrace();
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

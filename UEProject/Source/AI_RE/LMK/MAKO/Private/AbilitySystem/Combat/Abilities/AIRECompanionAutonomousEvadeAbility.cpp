#include "AbilitySystem/Combat/Abilities/AIRECompanionAutonomousEvadeAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionAutonomousEvadeGameplayEffects.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AIRECombatEvadeComponent.h"
#include "AIREEnemyAttackComponent.h"
#include "Core/AIRECompanionCharacter.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionAutonomousEvade, Log, All);

UAIRECompanionAutonomousEvadeAbility::
UAIRECompanionAutonomousEvadeAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(
		AIRECompanionGameplayTags::AbilityCombatAutonomousEvade);
	SetAssetTags(AssetTags);

	FAbilityTriggerData& EvadeTrigger = AbilityTriggers.AddDefaulted_GetRef();
	EvadeTrigger.TriggerTag =
		AIRECompanionGameplayTags::EventAutonomousEvadeRequest;
	EvadeTrigger.TriggerSource =
		EGameplayAbilityTriggerSource::GameplayEvent;

	ActivationOwnedTags.AddTag(
		AIRECompanionGameplayTags::StateActionEvading);
	ActivationBlockedTags.AddTag(
		AIRECompanionGameplayTags::StateActionEvading);
	ActivationBlockedTags.AddTag(
		AIRECompanionGameplayTags::CooldownAutonomousEvade);
}

bool UAIRECompanionAutonomousEvadeAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo
		|| !ActorInfo->AbilitySystemComponent.IsValid()
		|| !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	const AAIRECompanionCharacter* Companion =
		Cast<AAIRECompanionCharacter>(ActorInfo->AvatarActor.Get());
	const UAIRECompanionConfigDataAsset* Config = IsValid(Companion)
		? Companion->GetCompanionConfig()
		: nullptr;
	const UAIRECompanionEquipmentComponent* Equipment = IsValid(Companion)
		? Companion->GetEquipmentComponent()
		: nullptr;
	const UAIRECompanionWeaponDefinitionDataAsset* Weapon =
		IsValid(Equipment)
			? Equipment->GetCurrentWeaponDefinition()
			: nullptr;
	const UAIRECombatEvadeComponent* Evade = IsValid(Companion)
		? Companion->GetCombatEvadeComponent()
		: nullptr;
	const UAbilitySystemComponent* AbilitySystem =
		ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(Config)
		|| !Config->AutonomousEvade.bEnabled
		|| !IsValid(Weapon)
		|| !Weapon->IsMeleeWeapon()
		|| !IsValid(Evade)
		|| Evade->IsEvading()
		|| AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::CooldownAutonomousEvade)
		|| AbilitySystem->GetNumericAttribute(
			UAIRECompanionAttributeSet::GetStaminaAttribute())
			+ KINDA_SMALL_NUMBER
			< Config->AutonomousEvade.StaminaCost)
	{
		return false;
	}

	return Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags);
}

void UAIRECompanionAutonomousEvadeAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bIsEnding = false;
	bEvadeStarted = false;
	ActiveTriggerExecutionId.Invalidate();
	ActiveThreatActor.Reset();
	InvulnerabilityEffectHandle.Invalidate();
	ActiveInvulnerabilityDuration = 0.0f;

	AAIRECompanionCharacter* Companion = ActorInfo
		? Cast<AAIRECompanionCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const UAIRECompanionConfigDataAsset* Config = IsValid(Companion)
		? Companion->GetCompanionConfig()
		: nullptr;
	UAIRECompanionEquipmentComponent* Equipment = IsValid(Companion)
		? Companion->GetEquipmentComponent()
		: nullptr;
	const UAIRECompanionWeaponDefinitionDataAsset* Weapon =
		IsValid(Equipment)
			? Equipment->GetCurrentWeaponDefinition()
			: nullptr;
	UAbilitySystemComponent* AbilitySystem = ActorInfo
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	AActor* ThreatActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;
	FAIREEnemyAttackSnapshot AttackSnapshot;
	const bool bRuntimeValid =
		InitializeEventTarget(TriggerEventData)
		&& IsValid(Config)
		&& Config->AutonomousEvade.bEnabled
		&& IsValid(Weapon)
		&& Weapon->IsMeleeWeapon()
		&& IsValid(AbilitySystem)
		&& !AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::CooldownAutonomousEvade)
		&& AbilitySystem->GetNumericAttribute(
			UAIRECompanionAttributeSet::GetStaminaAttribute())
			+ KINDA_SMALL_NUMBER
			>= Config->AutonomousEvade.StaminaCost
		&& IsAttackOpportunityCurrent(
			ThreatActor,
			FGuid(),
			&AttackSnapshot);
	if (!bRuntimeValid)
	{
		FinishAbility(true);
		return;
	}

	ActiveEvadeComponent = Companion->GetCombatEvadeComponent();
	ActiveThreatActor = ThreatActor;
	ActiveTriggerExecutionId = AttackSnapshot.ExecutionId;
	FAIRECombatEvadePlan EvadePlan;
	if (!IsValid(ActiveEvadeComponent)
		|| !ActiveEvadeComponent->BuildLateralDashPlan(
			ThreatActor,
			ActiveTriggerExecutionId,
			EvadePlan)
		|| EvadePlan.AvailableDistance
			+ KINDA_SMALL_NUMBER
			< Config->AutonomousEvade.MinimumClearance
		|| !IsAttackOpportunityCurrent(
			ThreatActor,
			ActiveTriggerExecutionId))
	{
		FinishAbility(true);
		return;
	}

	ActiveEvadeComponent->OnEvadeFinished.AddDynamic(
		this,
		&UAIRECompanionAutonomousEvadeAbility::HandleEvadeFinished);
	ThreatActor->OnDestroyed.AddDynamic(
		this,
		&UAIRECompanionAutonomousEvadeAbility::HandleThreatDestroyed);
	DisabledStateChangedDelegateHandle = AbilitySystem
		->RegisterGameplayTagEvent(
			AIRECompanionGameplayTags::StateDisabledDead,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(
			this,
			&UAIRECompanionAutonomousEvadeAbility::
				HandleDisabledStateChanged);
	if (!ActiveEvadeComponent->TryStartLateralDashPlan(EvadePlan, this))
	{
		FinishAbility(true);
		return;
	}

	bEvadeStarted = true;
	if (!ApplySuccessfulEvadeEffects(Config->AutonomousEvade))
	{
		UE_LOG(
			LogAIRECompanionAutonomousEvade,
			Error,
			TEXT("Autonomous evade could not apply its GAS cost/cooldown effects. Companion=%s Threat=%s ExecutionId=%s"),
			*GetNameSafe(Companion),
			*GetNameSafe(ThreatActor),
			*ActiveTriggerExecutionId.ToString());
		FinishAbility(true);
		return;
	}

	ScheduleInvulnerability(Config->AutonomousEvade);
	if (bIsEnding)
	{
		return;
	}
	UE_LOG(
		LogAIRECompanionAutonomousEvade,
		Log,
		TEXT("[MAKO EVADE] Started Companion=%s Threat=%s ExecutionId=%s Side=%s Clearance=%.1f StaminaCost=%.1f InvulnerabilityDelay=%.3f InvulnerabilityDuration=%.3f"),
		*GetNameSafe(Companion),
		*GetNameSafe(ThreatActor),
		*ActiveTriggerExecutionId.ToString(),
		EvadePlan.Side == EAIRECombatEvadeSide::Right
			? TEXT("Right")
			: TEXT("Left"),
		EvadePlan.AvailableDistance,
		Config->AutonomousEvade.StaminaCost,
		Config->AutonomousEvade.InvulnerabilityStartDelay,
		Config->AutonomousEvade.InvulnerabilityDuration);
}

void UAIRECompanionAutonomousEvadeAbility::EndAbility(
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
			World->GetTimerManager().ClearTimer(
				InvulnerabilityStartTimerHandle);
		}
	}

	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		&& InvulnerabilityEffectHandle.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(
			InvulnerabilityEffectHandle);
	}
	InvulnerabilityEffectHandle.Invalidate();
	if (ActorInfo
		&& ActorInfo->AbilitySystemComponent.IsValid()
		&& DisabledStateChangedDelegateHandle.IsValid())
	{
		ActorInfo->AbilitySystemComponent->UnregisterGameplayTagEvent(
			DisabledStateChangedDelegateHandle,
			AIRECompanionGameplayTags::StateDisabledDead,
			EGameplayTagEventType::NewOrRemoved);
	}
	DisabledStateChangedDelegateHandle.Reset();

	if (IsValid(ActiveEvadeComponent))
	{
		ActiveEvadeComponent->OnEvadeFinished.RemoveDynamic(
			this,
			&UAIRECompanionAutonomousEvadeAbility::HandleEvadeFinished);
	}
	if (ActiveThreatActor.IsValid())
	{
		ActiveThreatActor->OnDestroyed.RemoveDynamic(
			this,
			&UAIRECompanionAutonomousEvadeAbility::HandleThreatDestroyed);
	}

	if (bWasCancelled
		&& bEvadeStarted
		&& IsValid(ActiveEvadeComponent)
		&& ActiveEvadeComponent->IsEvading())
	{
		ActiveEvadeComponent->CancelEvade();
	}

	ActiveEvadeComponent = nullptr;
	ActiveThreatActor.Reset();
	ActiveTriggerExecutionId.Invalidate();
	ActiveInvulnerabilityDuration = 0.0f;
	bEvadeStarted = false;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

bool UAIRECompanionAutonomousEvadeAbility::IsAttackOpportunityCurrent(
	const AActor* ThreatActor,
	const FGuid& ExecutionId,
	FAIREEnemyAttackSnapshot* OutSnapshot) const
{
	const AActor* Companion = GetAvatarActorFromActorInfo();
	const UAIREEnemyAttackComponent* AttackComponent = IsValid(ThreatActor)
		? ThreatActor->FindComponentByClass<UAIREEnemyAttackComponent>()
		: nullptr;
	if (!IsValid(Companion)
		|| !AIRECombatDamageTarget::IsAlive(Companion)
		|| !AIRECombatDamageTarget::IsAlive(ThreatActor)
		|| !IsValid(AttackComponent))
	{
		return false;
	}

	const FAIREEnemyAttackSnapshot Snapshot =
		AttackComponent->GetAttackSnapshot();
	const bool bMatchesExecution = !ExecutionId.IsValid()
		|| Snapshot.ExecutionId == ExecutionId;
	if (!Snapshot.bActive
		|| !Snapshot.bOpportunityOpen
		|| Snapshot.bHitCommitted
		|| Snapshot.bDamageCancelled
		|| Snapshot.TargetingMode
			!= EAIRECombatTargetingMode::SingleTarget
		|| Snapshot.Target.Get() != Companion
		|| !Snapshot.ExecutionId.IsValid()
		|| !bMatchesExecution)
	{
		return false;
	}

	if (OutSnapshot)
	{
		*OutSnapshot = Snapshot;
	}
	return true;
}

bool UAIRECompanionAutonomousEvadeAbility::ApplySuccessfulEvadeEffects(
	const FAIRECompanionAutonomousEvadeSettings& Settings)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(AbilitySystem))
	{
		return false;
	}

	const FGameplayEffectContextHandle EffectContext =
		AbilitySystem->MakeEffectContext();
	FGameplayEffectSpecHandle CooldownSpec = AbilitySystem->MakeOutgoingSpec(
		UAIRECompanionAutonomousEvadeCooldownGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	FGameplayEffectSpecHandle RegenBlockSpec = AbilitySystem->MakeOutgoingSpec(
		UAIRECompanionAutonomousEvadeRegenBlockGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	FGameplayEffectSpecHandle CostSpec = AbilitySystem->MakeOutgoingSpec(
		UAIRECompanionAutonomousEvadeCostGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!CooldownSpec.IsValid()
		|| !RegenBlockSpec.IsValid()
		|| !CostSpec.IsValid())
	{
		return false;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(
		AIRECompanionGameplayTags::DataAutonomousEvadeCooldownDuration,
		Settings.CooldownDuration);
	RegenBlockSpec.Data->SetSetByCallerMagnitude(
		AIRECompanionGameplayTags::DataAutonomousEvadeRegenBlockDuration,
		Settings.StaminaRegenDelay);
	CostSpec.Data->SetSetByCallerMagnitude(
		AIRECompanionGameplayTags::DataAutonomousEvadeStaminaCost,
		-Settings.StaminaCost);

	const FActiveGameplayEffectHandle CooldownHandle =
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get());
	if (!CooldownHandle.WasSuccessfullyApplied())
	{
		return false;
	}
	const FActiveGameplayEffectHandle RegenBlockHandle =
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*RegenBlockSpec.Data.Get());
	if (!RegenBlockHandle.WasSuccessfullyApplied())
	{
		AbilitySystem->RemoveActiveGameplayEffect(CooldownHandle);
		return false;
	}
	const FActiveGameplayEffectHandle CostHandle =
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
	if (!CostHandle.WasSuccessfullyApplied())
	{
		AbilitySystem->RemoveActiveGameplayEffect(CooldownHandle);
		AbilitySystem->RemoveActiveGameplayEffect(RegenBlockHandle);
		return false;
	}

	return true;
}

void UAIRECompanionAutonomousEvadeAbility::ScheduleInvulnerability(
	const FAIRECompanionAutonomousEvadeSettings& Settings)
{
	ActiveInvulnerabilityDuration = Settings.InvulnerabilityDuration;
	if (Settings.InvulnerabilityStartDelay <= 0.0f)
	{
		ApplyInvulnerability();
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		FinishAbility(true);
		return;
	}
	World->GetTimerManager().SetTimer(
		InvulnerabilityStartTimerHandle,
		this,
		&UAIRECompanionAutonomousEvadeAbility::ApplyInvulnerability,
		Settings.InvulnerabilityStartDelay,
		false);
}

void UAIRECompanionAutonomousEvadeAbility::ApplyInvulnerability()
{
	if (bIsEnding
		|| !IsActive()
		|| !IsValid(ActiveEvadeComponent)
		|| !ActiveEvadeComponent->IsEvadingFrom(
			ActiveThreatActor.Get(),
			ActiveTriggerExecutionId))
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(AbilitySystem))
	{
		FinishAbility(true);
		return;
	}

	FGameplayEffectSpecHandle InvulnerabilitySpec =
		AbilitySystem->MakeOutgoingSpec(
			UAIRECompanionAutonomousEvadeInvulnerabilityGameplayEffect::
				StaticClass(),
			1.0f,
			AbilitySystem->MakeEffectContext());
	if (!InvulnerabilitySpec.IsValid())
	{
		FinishAbility(true);
		return;
	}
	InvulnerabilitySpec.Data->SetSetByCallerMagnitude(
		AIRECompanionGameplayTags::
			DataAutonomousEvadeInvulnerabilityDuration,
		ActiveInvulnerabilityDuration);
	InvulnerabilityEffectHandle =
		AbilitySystem->ApplyGameplayEffectSpecToSelf(
			*InvulnerabilitySpec.Data.Get());
	if (!InvulnerabilityEffectHandle.WasSuccessfullyApplied())
	{
		InvulnerabilityEffectHandle.Invalidate();
		FinishAbility(true);
		return;
	}
	UE_LOG(
		LogAIRECompanionAutonomousEvade,
		Log,
		TEXT("[MAKO EVADE] Invulnerability effect applied. Companion=%s Threat=%s ExecutionId=%s Duration=%.3f"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(ActiveThreatActor.Get()),
		*ActiveTriggerExecutionId.ToString(),
		ActiveInvulnerabilityDuration);
}

void UAIRECompanionAutonomousEvadeAbility::HandleEvadeFinished()
{
	FinishAbility(false);
}

void UAIRECompanionAutonomousEvadeAbility::HandleThreatDestroyed(
	AActor*)
{
	FinishAbility(true);
}

void UAIRECompanionAutonomousEvadeAbility::HandleDisabledStateChanged(
	const FGameplayTag,
	const int32 NewCount)
{
	if (NewCount > 0)
	{
		FinishAbility(true);
	}
}

void UAIRECompanionAutonomousEvadeAbility::FinishAbility(
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

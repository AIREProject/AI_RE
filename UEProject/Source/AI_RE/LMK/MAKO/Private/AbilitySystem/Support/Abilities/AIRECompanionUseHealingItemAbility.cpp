#include "AbilitySystem/Support/Abilities/AIRECompanionUseHealingItemAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AbilitySystem/Support/Effects/AIRECompanionHealingGameplayEffect.h"
#include "AbilitySystem/Support/Effects/AIRECompanionSupportCooldownGameplayEffect.h"
#include "Core/AIRECompanionCharacter.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"
#include "LocalAI/Support/AIREHealingTargetInterface.h"
#include "Support/AIRECompanionSupportComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionHealingItem, Log, All);

UAIRECompanionUseHealingItemAbility::
UAIRECompanionUseHealingItemAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(
		AIRECompanionGameplayTags::AbilitySupportHealingItem);
	SetAssetTags(AssetTags);

	FAbilityTriggerData& HealTrigger =
		AbilityTriggers.AddDefaulted_GetRef();
	HealTrigger.TriggerTag =
		AIRECompanionGameplayTags::EventSupportHealRequest;
	HealTrigger.TriggerSource =
		EGameplayAbilityTriggerSource::GameplayEvent;

	ActivationOwnedTags.AddTag(
		AIRECompanionGameplayTags::StateActionSupporting);
	ActivationBlockedTags.AddTag(
		AIRECompanionGameplayTags::StateActionAttacking);
	ActivationBlockedTags.AddTag(
		AIRECompanionGameplayTags::StateActionSupporting);
	CooldownGameplayEffectClass =
		UAIRECompanionSupportCooldownGameplayEffect::StaticClass();
}

void UAIRECompanionUseHealingItemAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bIsEnding = false;
	ActiveConfig = const_cast<UAIRECompanionConfigDataAsset*>(
		GetCompanionConfig(Handle, ActorInfo));
	AActor* SourceActor = ActorInfo
		? ActorInfo->AvatarActor.Get()
		: nullptr;
	AActor* TargetActor = TriggerEventData
		? const_cast<AActor*>(TriggerEventData->Target.Get())
		: nullptr;
	const UAIRECompanionItemDefinitionDataAsset* HealingItem =
		GetHealingItem();
	float MissingHealth = 0.0f;
	const bool bCanStartTreatment =
		InitializeEventTarget(TriggerEventData)
		&& IsValid(SourceActor)
		&& IsValid(HealingItem)
		&& AIREHealingTarget::GetMissingHealth(
			TargetActor,
			SourceActor,
			MissingHealth)
		&& IsTargetInSupportRange(
			SourceActor,
			TargetActor,
			HealingItem->SupportRange);
	if (!bCanStartTreatment)
	{
		FinishAbility(true);
		return;
	}

	if (HealingItem->TreatmentDuration <= 0.0f)
	{
		ApplyHealingItem();
		return;
	}

	UWorld* World = SourceActor->GetWorld();
	if (!IsValid(World))
	{
		FinishAbility(true);
		return;
	}

	World->GetTimerManager().SetTimer(
		TreatmentTimerHandle,
		this,
		&UAIRECompanionUseHealingItemAbility::ApplyHealingItem,
		HealingItem->TreatmentDuration,
		false);
}

void UAIRECompanionUseHealingItemAbility::EndAbility(
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
				TreatmentTimerHandle);
		}
	}

	ActiveConfig.Reset();
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

const UAIRECompanionConfigDataAsset*
UAIRECompanionUseHealingItemAbility::GetCompanionConfig(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return nullptr;
	}

	const FGameplayAbilitySpec* AbilitySpec =
		ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(
			Handle);
	return AbilitySpec
		? Cast<UAIRECompanionConfigDataAsset>(
			AbilitySpec->SourceObject.Get())
		: nullptr;
}

const UAIRECompanionItemDefinitionDataAsset*
UAIRECompanionUseHealingItemAbility::GetHealingItem() const
{
	const AAIRECompanionCharacter* CompanionCharacter =
		Cast<AAIRECompanionCharacter>(
			GetAvatarActorFromActorInfo());
	const UAIRECompanionInventoryComponent* Inventory =
		IsValid(CompanionCharacter)
			? CompanionCharacter->GetInventoryComponent()
			: nullptr;
	return ActiveConfig.IsValid() && IsValid(Inventory)
		? Inventory->FindItemDefinition(
			ActiveConfig->DefaultHealingItemId)
		: nullptr;
}

bool UAIRECompanionUseHealingItemAbility::IsTargetInSupportRange(
	const AActor* SourceActor,
	const AActor* TargetActor,
	const float SupportRange) const
{
	if (!IsValid(SourceActor)
		|| !IsValid(TargetActor)
		|| !FMath::IsFinite(SupportRange)
		|| SupportRange < 0.0f)
	{
		return false;
	}

	const float HorizontalDistance = FVector::Dist2D(
		SourceActor->GetActorLocation(),
		TargetActor->GetActorLocation());
	const float EffectiveDistance = FMath::Max(
		0.0f,
		HorizontalDistance
			- SourceActor->GetSimpleCollisionRadius()
			- TargetActor->GetSimpleCollisionRadius());
	return EffectiveDistance <= SupportRange;
}

void UAIRECompanionUseHealingItemAbility::ApplyHealingItem()
{
	if (bIsEnding || !ActiveConfig.IsValid())
	{
		return;
	}

	AAIRECompanionCharacter* CompanionCharacter =
		Cast<AAIRECompanionCharacter>(
			GetAvatarActorFromActorInfo());
	UAIRECompanionInventoryComponent* Inventory =
		IsValid(CompanionCharacter)
			? CompanionCharacter->GetInventoryComponent()
			: nullptr;
	UAIRECompanionSupportComponent* Support =
		IsValid(CompanionCharacter)
			? CompanionCharacter->GetSupportComponent()
			: nullptr;
	UAbilitySystemComponent* SourceAbilitySystem =
		GetAbilitySystemComponentFromActorInfo();
	AActor* TargetActor = GetEventTarget();
	const UAIRECompanionItemDefinitionDataAsset* HealingItem =
		GetHealingItem();
	float MissingHealth = 0.0f;
	if (!IsValid(CompanionCharacter)
		|| !IsValid(Inventory)
		|| !IsValid(Support)
		|| !IsValid(SourceAbilitySystem)
		|| !IsValid(TargetActor)
		|| Support->GetSupportTarget() != TargetActor
		|| !IsValid(HealingItem)
		|| !Inventory->HasItem(ActiveConfig->DefaultHealingItemId)
		|| !AIREHealingTarget::GetMissingHealth(
			TargetActor,
			CompanionCharacter,
			MissingHealth)
		|| !IsTargetInSupportRange(
			CompanionCharacter,
			TargetActor,
			HealingItem->SupportRange))
	{
		if (IsValid(Support))
		{
			Support->CancelSupportRequest();
		}
		FinishAbility(true);
		return;
	}

	FGameplayEffectContextHandle EffectContext =
		SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(HealingItem);
	FGameplayEffectSpecHandle EffectSpec =
		SourceAbilitySystem->MakeOutgoingSpec(
			UAIRECompanionHealingGameplayEffect::StaticClass(),
			GetAbilityLevel(),
			EffectContext);
	if (!EffectSpec.IsValid())
	{
		Support->CancelSupportRequest();
		FinishAbility(true);
		return;
	}
	EffectSpec.Data->SetSetByCallerMagnitude(
		AIRECompanionGameplayTags::DataHealing,
		HealingItem->HealingAmount);

	if (!CommitAbilityCooldown(
			CurrentSpecHandle,
			GetCurrentActorInfo(),
			GetCurrentActivationInfo(),
			false)
		|| !Inventory->TryConsumeItem(
			ActiveConfig->DefaultHealingItemId,
			1))
	{
		Support->CancelSupportRequest();
		FinishAbility(true);
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(
			TargetActor,
			true);
	if (!IsValid(TargetAbilitySystem))
	{
		Inventory->TryAddItem(
			ActiveConfig->DefaultHealingItemId,
			1);
		Support->CancelSupportRequest();
		FinishAbility(true);
		return;
	}

	const FActiveGameplayEffectHandle AppliedEffect =
		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
			*EffectSpec.Data.Get(),
			TargetAbilitySystem);
	if (!AppliedEffect.WasSuccessfullyApplied())
	{
		Inventory->TryAddItem(
			ActiveConfig->DefaultHealingItemId,
			1);
		UE_LOG(
			LogAIRECompanionHealingItem,
			Error,
			TEXT("Companion healing effect failed after item consumption. The item was restored. Companion=%s Target=%s ItemId=%s"),
			*GetNameSafe(CompanionCharacter),
			*GetNameSafe(TargetActor),
			*ActiveConfig->DefaultHealingItemId.ToString());
		Support->CancelSupportRequest();
		FinishAbility(true);
		return;
	}

	Support->CompleteSupportRequest(TargetActor);
	FinishAbility(false);
}

void UAIRECompanionUseHealingItemAbility::FinishAbility(
	const bool bWasCancelled)
{
	if (bIsEnding)
	{
		return;
	}

	EndAbility(
		CurrentSpecHandle,
		GetCurrentActorInfo(),
		GetCurrentActivationInfo(),
		true,
		bWasCancelled);
}

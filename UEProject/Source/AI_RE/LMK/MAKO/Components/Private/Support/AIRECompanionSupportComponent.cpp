#include "Support/AIRECompanionSupportComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "AI_REAbilitySetDataAsset.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Inventory/AIRECompanionItemDefinitionDataAsset.h"
#include "LocalAI/Support/AIREHealingTargetInterface.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionSupport, Log, All);

UAIRECompanionSupportComponent::UAIRECompanionSupportComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

bool UAIRECompanionSupportComponent::InitializeSupport(
	const UAIRECompanionConfigDataAsset* CompanionConfig,
	UAIRECompanionInventoryComponent* InInventoryComponent,
	UAbilitySystemComponent* InAbilitySystem)
{
	ShutdownSupport();
	if (!IsValid(CompanionConfig)
		|| !IsValid(InInventoryComponent)
		|| !IsValid(InAbilitySystem))
	{
		return false;
	}

	ActiveConfig = const_cast<UAIRECompanionConfigDataAsset*>(
		CompanionConfig);
	InventoryComponent = InInventoryComponent;
	AbilitySystem = InAbilitySystem;
	DeadStateChangedDelegateHandle = AbilitySystem
		->RegisterGameplayTagEvent(
			AIRECompanionGameplayTags::StateDisabledDead,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(
			this,
			&UAIRECompanionSupportComponent::HandleDeadStateChanged);
	AttackStateChangedDelegateHandle = AbilitySystem
		->RegisterGameplayTagEvent(
			AIRECompanionGameplayTags::StateActionAttacking,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(
			this,
			&UAIRECompanionSupportComponent::HandleAttackStateChanged);

	if (AbilitySystem->HasMatchingGameplayTag(
		AIRECompanionGameplayTags::StateDisabledDead))
	{
		return true;
	}

	if (!GrantSupportAbilities())
	{
		ShutdownSupport();
		return false;
	}

	return true;
}

void UAIRECompanionSupportComponent::ShutdownSupport()
{
	CancelSupportRequest();
	ReleaseSupportAbilities();
	if (DeadStateChangedDelegateHandle.IsValid()
		&& AbilitySystem.IsValid())
	{
		AbilitySystem->UnregisterGameplayTagEvent(
			DeadStateChangedDelegateHandle,
			AIRECompanionGameplayTags::StateDisabledDead,
			EGameplayTagEventType::NewOrRemoved);
	}
	DeadStateChangedDelegateHandle.Reset();
	if (AttackStateChangedDelegateHandle.IsValid()
		&& AbilitySystem.IsValid())
	{
		AbilitySystem->UnregisterGameplayTagEvent(
			AttackStateChangedDelegateHandle,
			AIRECompanionGameplayTags::StateActionAttacking,
			EGameplayTagEventType::NewOrRemoved);
	}
	AttackStateChangedDelegateHandle.Reset();
	ActiveConfig.Reset();
	InventoryComponent.Reset();
	AbilitySystem.Reset();
}

bool UAIRECompanionSupportComponent::RequestSupport(AActor* TargetActor)
{
	if (!ActiveConfig.IsValid()
		|| !InventoryComponent.IsValid()
		|| !AbilitySystem.IsValid()
		|| ActiveConfig->DefaultHealingItemId.IsNone()
		|| !InventoryComponent->HasItem(
			ActiveConfig->DefaultHealingItemId))
	{
		return false;
	}

	float MissingHealth = 0.0f;
	if (!AIREHealingTarget::GetMissingHealth(
			TargetActor,
			GetOwner(),
			MissingHealth))
	{
		return false;
	}

	CancelSupportRequest();
	SupportTarget = TargetActor;
	TargetActor->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIRECompanionSupportComponent::HandleSupportTargetDestroyed);
	return true;
}

void UAIRECompanionSupportComponent::CancelSupportRequest()
{
	if (AActor* TargetActor = SupportTarget.Get(); IsValid(TargetActor))
	{
		TargetActor->OnDestroyed.RemoveDynamic(
			this,
			&UAIRECompanionSupportComponent::HandleSupportTargetDestroyed);
	}
	SupportTarget.Reset();
	bAutoSupportRequestActive = false;

	if (AbilitySystem.IsValid())
	{
		const FGameplayAbilitySpecHandle AbilityHandle =
			FindSupportAbilityHandle();
		if (AbilityHandle.IsValid())
		{
			AbilitySystem->CancelAbilityHandle(AbilityHandle);
		}
	}
}

AActor* UAIRECompanionSupportComponent::GetSupportTarget() const
{
	return SupportTarget.Get();
}

bool UAIRECompanionSupportComponent::IsSupportRequested() const
{
	if (!SupportTarget.IsValid()
		|| !ActiveConfig.IsValid()
		|| !InventoryComponent.IsValid()
		|| !InventoryComponent->HasItem(
			ActiveConfig->DefaultHealingItemId))
	{
		return false;
	}

	float MissingHealth = 0.0f;
	return AIREHealingTarget::GetMissingHealth(
		SupportTarget.Get(),
		GetOwner(),
		MissingHealth);
}

float UAIRECompanionSupportComponent::GetSupportRange() const
{
	const UAIRECompanionItemDefinitionDataAsset* HealingItem =
		ActiveConfig.IsValid() && InventoryComponent.IsValid()
			? InventoryComponent->FindItemDefinition(
				ActiveConfig->DefaultHealingItemId)
			: nullptr;
	return IsValid(HealingItem) ? HealingItem->SupportRange : 0.0f;
}

void UAIRECompanionSupportComponent::CompleteSupportRequest(
	AActor* TargetActor)
{
	if (SupportTarget.Get() != TargetActor)
	{
		return;
	}

	if (IsValid(TargetActor))
	{
		TargetActor->OnDestroyed.RemoveDynamic(
			this,
			&UAIRECompanionSupportComponent::HandleSupportTargetDestroyed);
	}
	SupportTarget.Reset();
	bAutoSupportRequestActive = false;
}

void UAIRECompanionSupportComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ActiveConfig.IsValid()
		|| !InventoryComponent.IsValid()
		|| !AbilitySystem.IsValid()
		|| AbilitySystem->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateDisabledDead))
	{
		return;
	}

	AActor* PlayerActor = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValid(PlayerActor))
	{
		return;
	}

	if (bAutoSupportRequestActive
		&& SupportTarget.Get() == PlayerActor
		&& !IsPlayerBelowAutoSupportThreshold(PlayerActor))
	{
		CancelSupportRequest();
		return;
	}

	if (!IsSupportRequested()
		&& IsPlayerBelowAutoSupportThreshold(PlayerActor)
		&& RequestSupport(PlayerActor))
	{
		bAutoSupportRequestActive = true;
	}
}

bool UAIRECompanionSupportComponent::IsPlayerBelowAutoSupportThreshold(
	AActor* PlayerActor) const
{
	if (!ActiveConfig.IsValid()
		|| !IsValid(PlayerActor))
	{
		return false;
	}

	const float Threshold = ActiveConfig->AutoSupportHealthPercent;
	if (!FMath::IsFinite(Threshold)
		|| Threshold <= 0.0f)
	{
		return false;
	}

	float MissingHealth = 0.0f;
	if (!AIREHealingTarget::GetMissingHealth(
			PlayerActor,
			GetOwner(),
			MissingHealth))
	{
		return false;
	}

	const IAIREHealingTargetInterface* HealingTarget =
		Cast<IAIREHealingTargetInterface>(PlayerActor);
	const UAbilitySystemComponent* PlayerAbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(
			PlayerActor,
			true);
	if (!HealingTarget || !IsValid(PlayerAbilitySystem))
	{
		return false;
	}

	const float Health = PlayerAbilitySystem->GetNumericAttribute(
		HealingTarget->GetHealingHealthAttribute());
	const float MaxHealth = PlayerAbilitySystem->GetNumericAttribute(
		HealingTarget->GetHealingMaxHealthAttribute());
	return FMath::IsFinite(Health)
		&& FMath::IsFinite(MaxHealth)
		&& MaxHealth > 0.0f
		&& Health / MaxHealth <= Threshold;
}

FGameplayAbilitySpecHandle
UAIRECompanionSupportComponent::FindSupportAbilityHandle() const
{
	if (!AbilitySystem.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	for (const FGameplayAbilitySpecHandle& AbilityHandle
		: GrantedAbilityHandles)
	{
		const FGameplayAbilitySpec* AbilitySpec =
			AbilitySystem->FindAbilitySpecFromHandle(AbilityHandle);
		if (AbilitySpec
			&& IsValid(AbilitySpec->Ability.Get())
			&& AbilitySpec->Ability->GetAssetTags().HasTagExact(
				AIRECompanionGameplayTags::AbilitySupportHealingItem))
		{
			return AbilityHandle;
		}
	}
	return FGameplayAbilitySpecHandle();
}

bool UAIRECompanionSupportComponent::GrantSupportAbilities()
{
	if (!ActiveConfig.IsValid()
		|| !IsValid(ActiveConfig->SupportAbilitySet)
		|| !InventoryComponent.IsValid()
		|| !AbilitySystem.IsValid())
	{
		return false;
	}

	FText ValidationError;
	if (!ActiveConfig->SupportAbilitySet->IsAbilitySetValid(
			ValidationError))
	{
		return false;
	}

	const UAIRECompanionItemDefinitionDataAsset* HealingItem =
		InventoryComponent->FindItemDefinition(
			ActiveConfig->DefaultHealingItemId);
	if (!IsValid(HealingItem)
		|| HealingItem->ItemType != EAI_REItemType::Consumable)
	{
		return false;
	}

	for (const FAIRECompanionAbilitySetEntry& Entry
		: ActiveConfig->SupportAbilitySet->Abilities)
	{
		FGameplayAbilitySpec AbilitySpec(
			Entry.AbilityClass,
			Entry.AbilityLevel,
			INDEX_NONE,
			ActiveConfig.Get());
		AbilitySpec.SetByCallerTagMagnitudes.Add(
			AIRECompanionGameplayTags::DataSupportCooldownDuration,
			HealingItem->CooldownDuration);
		const FGameplayAbilitySpecHandle GrantedHandle =
			AbilitySystem->GiveAbility(AbilitySpec);
		if (!GrantedHandle.IsValid())
		{
			ReleaseSupportAbilities();
			return false;
		}
		GrantedAbilityHandles.Add(GrantedHandle);
	}

	if (!FindSupportAbilityHandle().IsValid())
	{
		ReleaseSupportAbilities();
		return false;
	}

	return true;
}

void UAIRECompanionSupportComponent::ReleaseSupportAbilities()
{
	if (AbilitySystem.IsValid())
	{
		for (const FGameplayAbilitySpecHandle& AbilityHandle
			: GrantedAbilityHandles)
		{
			if (AbilityHandle.IsValid())
			{
				AbilitySystem->CancelAbilityHandle(AbilityHandle);
				AbilitySystem->ClearAbility(AbilityHandle);
			}
		}
	}
	GrantedAbilityHandles.Reset();
}

void UAIRECompanionSupportComponent::HandleDeadStateChanged(
	const FGameplayTag,
	const int32 NewCount)
{
	if (NewCount > 0)
	{
		CancelSupportRequest();
		ReleaseSupportAbilities();
		return;
	}

	if (GrantedAbilityHandles.IsEmpty())
	{
		GrantSupportAbilities();
	}
}

void UAIRECompanionSupportComponent::HandleAttackStateChanged(
	const FGameplayTag,
	const int32 NewCount)
{
	if (NewCount > 0)
	{
		CancelSupportRequest();
	}
}

void UAIRECompanionSupportComponent::HandleSupportTargetDestroyed(
	AActor* DestroyedActor)
{
	if (SupportTarget.Get() == DestroyedActor)
	{
		CancelSupportRequest();
	}
}

void UAIRECompanionSupportComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownSupport();
	Super::EndPlay(EndPlayReason);
}

#include "Equipment/AIRECompanionEquipmentComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Equipment/AIRECompanionAbilitySetDataAsset.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionEquipment, Log, All);

UAIRECompanionEquipmentComponent::UAIRECompanionEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIRECompanionEquipmentComponent::InitializeEquipment(
	UAbilitySystemComponent* InAbilitySystem)
{
	ShutdownEquipment();
	if (!IsValid(InAbilitySystem))
	{
		return false;
	}

	if (!IsValid(DefaultWeaponDefinition))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Companion %s has no default Weapon Definition. No weapon abilities were granted."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	AbilitySystem = InAbilitySystem;
	DeadStateChangedDelegateHandle = AbilitySystem
		->RegisterGameplayTagEvent(
			AIRECompanionGameplayTags::StateDisabledDead,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(
			this,
			&UAIRECompanionEquipmentComponent::HandleDeadStateChanged);
	if (!EquipWeapon(DefaultWeaponDefinition))
	{
		ShutdownEquipment();
		return false;
	}

	return true;
}

void UAIRECompanionEquipmentComponent::ShutdownEquipment()
{
	if (DeadStateChangedDelegateHandle.IsValid())
	{
		if (AbilitySystem.IsValid())
		{
			AbilitySystem->UnregisterGameplayTagEvent(
				DeadStateChangedDelegateHandle,
				AIRECompanionGameplayTags::StateDisabledDead,
				EGameplayTagEventType::NewOrRemoved);
		}
		DeadStateChangedDelegateHandle.Reset();
	}

	UnequipCurrentWeapon();
	AbilitySystem.Reset();
}

const UAIRECompanionWeaponDefinitionDataAsset*
UAIRECompanionEquipmentComponent::GetCurrentWeaponDefinition() const
{
	return CurrentWeaponDefinition;
}

FGameplayTag UAIRECompanionEquipmentComponent::GetCurrentWeaponTag() const
{
	return IsValid(CurrentWeaponDefinition)
		? CurrentWeaponDefinition->WeaponTag
		: FGameplayTag();
}

bool UAIRECompanionEquipmentComponent::IsCurrentWeaponInCategory(
	const FGameplayTag WeaponCategory) const
{
	const FGameplayTag CurrentWeaponTag = GetCurrentWeaponTag();
	return WeaponCategory.IsValid()
		&& CurrentWeaponTag.IsValid()
		&& CurrentWeaponTag.MatchesTag(WeaponCategory);
}

FGameplayAbilitySpecHandle UAIRECompanionEquipmentComponent::FindGrantedAbilityHandle(
	const FGameplayTag AbilityTag) const
{
	if (!AbilitySystem.IsValid() || !AbilityTag.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	for (const FGameplayAbilitySpecHandle& AbilityHandle : GrantedAbilityHandles)
	{
		const FGameplayAbilitySpec* AbilitySpec =
			AbilitySystem->FindAbilitySpecFromHandle(AbilityHandle);
		if (AbilitySpec
			&& IsValid(AbilitySpec->Ability.Get())
			&& AbilitySpec->Ability->GetAssetTags().HasTagExact(AbilityTag))
		{
			return AbilityHandle;
		}
	}

	return FGameplayAbilitySpecHandle();
}

void UAIRECompanionEquipmentComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownEquipment();
	Super::EndPlay(EndPlayReason);
}

bool UAIRECompanionEquipmentComponent::EquipWeapon(
	UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition)
{
	if (!AbilitySystem.IsValid() || !IsValid(WeaponDefinition))
	{
		return false;
	}

	FText ValidationError;
	if (!WeaponDefinition->IsWeaponDefinitionValid(ValidationError))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Weapon Definition %s is invalid: %s"),
			*GetNameSafe(WeaponDefinition),
			*ValidationError.ToString());
		return false;
	}

	DesiredWeaponDefinition = WeaponDefinition;
	ReleaseCurrentWeaponState();
	if (AbilitySystem->HasMatchingGameplayTag(
		AIRECompanionGameplayTags::StateDisabledDead))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Log,
			TEXT("Companion weapon equip deferred while dead. Companion=%s Weapon=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(WeaponDefinition));
		return true;
	}

	PendingWeaponDefinition = WeaponDefinition;

	TArray<FSoftObjectPath> AssetsToLoad;
	AssetsToLoad.Add(WeaponDefinition->AbilitySet.ToSoftObjectPath());
	if (!WeaponDefinition->LinkedAnimLayerClass.IsNull())
	{
		AssetsToLoad.AddUnique(
			WeaponDefinition->LinkedAnimLayerClass.ToSoftObjectPath());
	}
	if (!WeaponDefinition->AttackMontage.IsNull())
	{
		AssetsToLoad.AddUnique(WeaponDefinition->AttackMontage.ToSoftObjectPath());
	}

	const uint32 RequestId = ++EquipmentRequestId;
	PendingEquipmentLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateUObject(
			this,
			&UAIRECompanionEquipmentComponent::CompleteEquipWeapon,
			RequestId),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("AIRECompanionEquipment"));
	if (!PendingEquipmentLoadHandle.IsValid())
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Failed to start async asset loading for Weapon Definition %s."),
			*GetNameSafe(WeaponDefinition));
		CancelPendingEquipmentLoad();
		return false;
	}

	UE_LOG(
		LogAIRECompanionEquipment,
		Log,
		TEXT("Companion weapon equip requested. Companion=%s Weapon=%s Tag=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(WeaponDefinition),
		*WeaponDefinition->WeaponTag.ToString());
	return true;
}

void UAIRECompanionEquipmentComponent::CompleteEquipWeapon(const uint32 RequestId)
{
	if (RequestId != EquipmentRequestId
		|| !AbilitySystem.IsValid()
		|| !IsValid(GetOwner())
		|| !IsValid(PendingWeaponDefinition)
		|| !PendingEquipmentLoadHandle.IsValid())
	{
		return;
	}

	if (AbilitySystem->HasMatchingGameplayTag(
		AIRECompanionGameplayTags::StateDisabledDead))
	{
		ReleaseCurrentWeaponState();
		return;
	}

	UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition =
		PendingWeaponDefinition;
	ActiveEquipmentLoadHandle = MoveTemp(PendingEquipmentLoadHandle);
	PendingWeaponDefinition = nullptr;

	FText ValidationError;
	UAIRECompanionAbilitySetDataAsset* AbilitySet =
		WeaponDefinition->AbilitySet.Get();
	if (!IsValid(AbilitySet) || !AbilitySet->IsAbilitySetValid(ValidationError))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Weapon Definition %s could not load a valid Ability Set: %s"),
			*GetNameSafe(WeaponDefinition),
			*ValidationError.ToString());
		UnequipCurrentWeapon();
		return;
	}

	CurrentWeaponDefinition = WeaponDefinition;
	for (const FAIRECompanionAbilitySetEntry& Entry : AbilitySet->Abilities)
	{
		FGameplayAbilitySpec AbilitySpec(
			Entry.AbilityClass,
			Entry.AbilityLevel,
			INDEX_NONE,
			WeaponDefinition);
		AbilitySpec.SetByCallerTagMagnitudes.Add(
			AIRECompanionGameplayTags::DataAttackStaminaCost,
			-WeaponDefinition->StaminaCost);
		AbilitySpec.SetByCallerTagMagnitudes.Add(
			AIRECompanionGameplayTags::DataAttackCooldownDuration,
			WeaponDefinition->CooldownDuration);

		const FGameplayAbilitySpecHandle GrantedHandle =
			AbilitySystem->GiveAbility(AbilitySpec);
		if (!GrantedHandle.IsValid())
		{
			UE_LOG(
				LogAIRECompanionEquipment,
				Warning,
				TEXT("Failed to grant ability %s for Weapon Definition %s."),
				*GetNameSafe(Entry.AbilityClass.Get()),
				*GetNameSafe(WeaponDefinition));
			UnequipCurrentWeapon();
			return;
		}

		GrantedAbilityHandles.Add(GrantedHandle);
	}

	if (WeaponDefinition->IsMeleeWeapon()
		&& !FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatBasicAttack).IsValid())
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Melee Weapon Definition %s did not grant a Basic Attack ability."),
			*GetNameSafe(WeaponDefinition));
		UnequipCurrentWeapon();
		return;
	}

	LinkCurrentAnimLayer();

	if (!WeaponDefinition->AttackMontage.IsNull()
		&& !IsValid(WeaponDefinition->AttackMontage.Get()))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Weapon Definition %s could not load its Attack Montage. The Basic Attack fallback remains available."),
			*GetNameSafe(WeaponDefinition));
	}

	UE_LOG(
		LogAIRECompanionEquipment,
		Log,
		TEXT("Companion weapon equipped. Companion=%s Weapon=%s Tag=%s Abilities=%d LinkedLayer=%s MontageLoaded=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(CurrentWeaponDefinition),
		*CurrentWeaponDefinition->WeaponTag.ToString(),
		GrantedAbilityHandles.Num(),
		CurrentLinkedAnimLayerClass
			? *GetNameSafe(CurrentLinkedAnimLayerClass.Get())
			: TEXT("None"),
		IsValid(CurrentWeaponDefinition->AttackMontage.Get())
			? TEXT("true")
			: TEXT("false"));
}

void UAIRECompanionEquipmentComponent::CancelPendingEquipmentLoad()
{
	++EquipmentRequestId;
	if (PendingEquipmentLoadHandle.IsValid())
	{
		PendingEquipmentLoadHandle->CancelHandle();
		PendingEquipmentLoadHandle.Reset();
	}

	PendingWeaponDefinition = nullptr;
}

void UAIRECompanionEquipmentComponent::HandleDeadStateChanged(
	const FGameplayTag,
	const int32 NewCount)
{
	if (NewCount > 0)
	{
		ReleaseCurrentWeaponState();
		return;
	}

	if (IsValid(DesiredWeaponDefinition))
	{
		EquipWeapon(DesiredWeaponDefinition);
	}
}

void UAIRECompanionEquipmentComponent::LinkCurrentAnimLayer()
{
	if (!IsValid(CurrentWeaponDefinition)
		|| CurrentWeaponDefinition->LinkedAnimLayerClass.IsNull())
	{
		return;
	}

	CurrentLinkedAnimLayerClass =
		CurrentWeaponDefinition->LinkedAnimLayerClass.Get();
	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (!CurrentLinkedAnimLayerClass || !IsValid(AnimInstance))
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Warning,
			TEXT("Weapon Definition %s could not link its Anim Layer. The base animation and attack fallback remain available."),
			*GetNameSafe(CurrentWeaponDefinition));
		CurrentLinkedAnimLayerClass = nullptr;
		return;
	}

	AnimInstance->LinkAnimClassLayers(CurrentLinkedAnimLayerClass);
}

void UAIRECompanionEquipmentComponent::UnlinkCurrentAnimLayer()
{
	if (!CurrentLinkedAnimLayerClass)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetOwnerAnimInstance())
	{
		AnimInstance->UnlinkAnimClassLayers(CurrentLinkedAnimLayerClass);
	}
	CurrentLinkedAnimLayerClass = nullptr;
}

UAnimInstance* UAIRECompanionEquipmentComponent::GetOwnerAnimInstance() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	return IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
}

void UAIRECompanionEquipmentComponent::UnequipCurrentWeapon()
{
	DesiredWeaponDefinition = nullptr;
	ReleaseCurrentWeaponState();
}

void UAIRECompanionEquipmentComponent::ReleaseCurrentWeaponState()
{
	const bool bHadRuntimeState =
		IsValid(CurrentWeaponDefinition)
		|| IsValid(PendingWeaponDefinition)
		|| !GrantedAbilityHandles.IsEmpty()
		|| CurrentLinkedAnimLayerClass
		|| PendingEquipmentLoadHandle.IsValid()
		|| ActiveEquipmentLoadHandle.IsValid();
	const FString ReleasedWeaponName = IsValid(CurrentWeaponDefinition)
		? GetNameSafe(CurrentWeaponDefinition)
		: GetNameSafe(PendingWeaponDefinition);
	const int32 RemovedAbilityCount = GrantedAbilityHandles.Num();
	const bool bRemovedLinkedLayer = CurrentLinkedAnimLayerClass != nullptr;

	CancelPendingEquipmentLoad();

	if (AbilitySystem.IsValid())
	{
		for (const FGameplayAbilitySpecHandle& AbilityHandle : GrantedAbilityHandles)
		{
			if (AbilityHandle.IsValid())
			{
				AbilitySystem->CancelAbilityHandle(AbilityHandle);
				AbilitySystem->ClearAbility(AbilityHandle);
			}
		}
	}

	GrantedAbilityHandles.Reset();
	UnlinkCurrentAnimLayer();
	CurrentWeaponDefinition = nullptr;
	if (ActiveEquipmentLoadHandle.IsValid())
	{
		ActiveEquipmentLoadHandle->ReleaseHandle();
		ActiveEquipmentLoadHandle.Reset();
	}

	if (bHadRuntimeState)
	{
		UE_LOG(
			LogAIRECompanionEquipment,
			Log,
			TEXT("Companion weapon runtime state released. Companion=%s Weapon=%s AbilitiesRemoved=%d LayerUnlinked=%s"),
			*GetNameSafe(GetOwner()),
			*ReleasedWeaponName,
			RemovedAbilityCount,
			bRemovedLinkedLayer ? TEXT("true") : TEXT("false"));
	}
}

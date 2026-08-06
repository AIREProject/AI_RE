// Copyright MixUpProject. All Rights Reserved.

#include "AI_REPlayerCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AI_RECharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/AssetManager.h"
#include "../../LMK/MAKO/Public/Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "AI_REWeaponItemDataAsset.h"
#include "Engine/Engine.h"

UAI_REPlayerCombatComponent::UAI_REPlayerCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAI_REPlayerCombatComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAI_REPlayerCombatComponent::TryStartPrimaryAction()
{
	AAI_RECharacterBase* OwnerChar = Cast<AAI_RECharacterBase>(GetOwner());
	if (OwnerChar)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerChar))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.MeleeAttack"));
				bool bActivated = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(AttackTag));
				if (!bActivated)
				{
					FGameplayEventData Payload;
					Payload.EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.ComboInput"));
					ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
				}
			}
		}
	}
}

void UAI_REPlayerCombatComponent::EquipWeapon(UAI_REItemDataAsset* WeaponData)
{
	if (WeaponData)
	{
		EquippedWeapon = WeaponData;
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Equipped Weapon: %s"), *WeaponData->DisplayName.ToString()));
		
		// 비동기 로딩 초기화
		CachedAttackMontage = nullptr;
		if (MontageLoadHandle.IsValid())
		{
			MontageLoadHandle->CancelHandle();
			MontageLoadHandle.Reset();
		}

		if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(WeaponData))
		{
			if (WeaponItem->WeaponDefinition)
			{
				TSoftObjectPtr<UAnimMontage> SoftMontage = WeaponItem->WeaponDefinition->AttackMontage;
				if (!SoftMontage.IsNull())
				{
					UAssetManager& AssetManager = UAssetManager::Get();
					FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();
					
					MontageLoadHandle = StreamableManager.RequestAsyncLoad(SoftMontage.ToSoftObjectPath(), 
						FStreamableDelegate::CreateUObject(this, &UAI_REPlayerCombatComponent::OnWeaponMontageLoaded));
				}
			}
		}
	}
}

void UAI_REPlayerCombatComponent::OnWeaponMontageLoaded()
{
	if (EquippedWeapon)
	{
		if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(EquippedWeapon))
		{
			if (WeaponItem->WeaponDefinition)
			{
				CachedAttackMontage = WeaponItem->WeaponDefinition->AttackMontage.Get();
			}
		}
	}
}

void UAI_REPlayerCombatComponent::UnequipWeapon()
{
	EquippedWeapon = nullptr;
	CachedAttackMontage = nullptr;
	if (MontageLoadHandle.IsValid())
	{
		MontageLoadHandle->CancelHandle();
		MontageLoadHandle.Reset();
	}
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Unequipped Weapon"));
}

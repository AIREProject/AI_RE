// Copyright MixUpProject. All Rights Reserved.

#include "AI_REPlayerCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AI_RECharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/AssetManager.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "AI_REWeaponItemDataAsset.h"
#include "AI_REAbilitySetDataAsset.h"
#include "Engine/Engine.h"
#include "AI_RECharacter.h"
#include "AI_RETargetScannerComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/StaticMeshComponent.h"

UAI_REPlayerCombatComponent::UAI_REPlayerCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAI_REPlayerCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!EquippedWeapon && DefaultUnarmedWeapon)
	{
		EquipWeapon(DefaultUnarmedWeapon);
	}
}

void UAI_REPlayerCombatComponent::TryStartPrimaryAction()
{
	AAI_RECharacterBase* OwnerChar = Cast<AAI_RECharacterBase>(GetOwner());
	if (OwnerChar)
	{
		// [소프트 타겟팅] 공격 스킬 발동 직전 1틱 스캔 (TargetScannerComponent 사용)
		if (AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(OwnerChar))
		{
			if (UAI_RETargetScannerComponent* Scanner = PlayerChar->GetTargetScannerComponent())
			{
				// 반경 100(조금 넓게), 거리 500(5m) 내의 Pawn(캐릭터)을 스캔
				AActor* TargetActor = Scanner->ScanForwardForTarget(100.0f, 500.0f, ECC_Pawn, false);
				if (TargetActor && TargetActor != PlayerChar)
				{
					// 대상을 향한 회전값 계산 후 몸을 즉각적으로 돌림 (Z축 기준 Yaw만)
					FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(PlayerChar->GetActorLocation(), TargetActor->GetActorLocation());
					FRotator NewRotation = FRotator(0.f, LookAtRot.Yaw, 0.f);
					PlayerChar->SetActorRotation(NewRotation);
					
					if (GEngine) GEngine->AddOnScreenDebugMessage(12, 1.0f, FColor::Cyan, FString::Printf(TEXT("[Lock-On] 타겟: %s"), *TargetActor->GetName()));
				}
			}
		}

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

		// 1. 기존 장착 무기가 있다면 해제 (스킬 회수 및 외형 리셋)
		if (EquippedWeapon != nullptr)
		{
			ClearWeaponAbilities();
			ClearWeaponVisuals();
		}
		
		EquippedWeapon = WeaponData;

		if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(WeaponData))
		{
			if (WeaponItem->WeaponDefinition)
			{
				// 무기에 정의된 AbilitySet 스킬들 실제로 부여하기 (중요!)
				if (!WeaponItem->WeaponDefinition->AbilitySet.IsNull())
				{
					if (UAI_REAbilitySetDataAsset* LoadedAbilitySet = WeaponItem->WeaponDefinition->AbilitySet.LoadSynchronous())
					{
						AActor* OwnerActor = GetOwner();
						if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerActor))
						{
							if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
							{
								for (const FAIRECompanionAbilitySetEntry& Entry : LoadedAbilitySet->Abilities)
								{
									if (Entry.AbilityClass)
									{
										FGameplayAbilitySpec Spec(Entry.AbilityClass, Entry.AbilityLevel, -1, OwnerActor);
										FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
										GrantedWeaponAbilities.Add(Handle);
									}
								}
							}
						}
					}
				}

				TSoftObjectPtr<UAnimMontage> SoftMontage = WeaponItem->WeaponDefinition->AttackMontage;
				if (!SoftMontage.IsNull())
				{
					UAssetManager& AssetManager = UAssetManager::Get();
					FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();
					
					MontageLoadHandle = StreamableManager.RequestAsyncLoad(SoftMontage.ToSoftObjectPath(), 
						FStreamableDelegate::CreateUObject(this, &UAI_REPlayerCombatComponent::OnWeaponMontageLoaded));
				}
				
				// 1. 무기 외형(Mesh) 장착
				if (AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(GetOwner()))
				{
					if (UStaticMeshComponent* WeaponMeshComp = PlayerChar->GetWeaponMeshComponent())
					{
						WeaponMeshComp->SetStaticMesh(WeaponItem->WeaponMesh);
					}

					// 2. 레이어드 애니메이션(Linked Anim Layers) 적용 (오버라이드)
					if (!WeaponItem->WeaponDefinition->LinkedAnimLayerClass.IsNull())
					{
						UClass* LayerClass = WeaponItem->WeaponDefinition->LinkedAnimLayerClass.LoadSynchronous();
						if (LayerClass)
						{
							PlayerChar->GetMesh()->LinkAnimClassLayers(LayerClass);
						}
					}
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
	if (EquippedWeapon == nullptr)
		return;

	// 1. 현재 무기의 스킬 및 외형/애니메이션 해제
	ClearWeaponAbilities();
	ClearWeaponVisuals();
	
	EquippedWeapon = nullptr;
	CachedAttackMontage = nullptr;
	if (MontageLoadHandle.IsValid())
	{
		MontageLoadHandle->CancelHandle();
		MontageLoadHandle.Reset();
	}
	
	// 2. 맨손(기본 무기) 장착
	if (DefaultUnarmedWeapon)
	{
		EquipWeapon(DefaultUnarmedWeapon);
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Unequipped Weapon -> Fallback to Default"));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Unequipped Weapon"));
	}
}

void UAI_REPlayerCombatComponent::ClearWeaponAbilities()
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerActor))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				for (const FGameplayAbilitySpecHandle& Handle : GrantedWeaponAbilities)
				{
					ASC->ClearAbility(Handle);
				}
			}
		}
	}
	GrantedWeaponAbilities.Empty();
}

void UAI_REPlayerCombatComponent::ClearWeaponVisuals()
{
	if (AActor* OwnerActor = GetOwner())
	{
		// 1. 무기 외형(Mesh) 해제 및 애니메이션 레이어 해제
		if (AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(OwnerActor))
		{
			if (UStaticMeshComponent* WeaponMeshComp = PlayerChar->GetWeaponMeshComponent())
			{
				WeaponMeshComp->SetStaticMesh(nullptr);
			}

			// 맨손 상태로 복귀하기 위해 링크된 애니메이션 레이어를 모두 해제합니다.
			PlayerChar->GetMesh()->UnlinkAnimClassLayers(nullptr);
		}
	}
}

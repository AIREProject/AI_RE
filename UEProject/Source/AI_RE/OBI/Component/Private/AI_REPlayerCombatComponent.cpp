// Copyright MixUpProject. All Rights Reserved.

#include "AI_REPlayerCombatComponent.h"

#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "AI_REHarvestDamageTarget.h"
#include "AIRECombatDamageSubsystem.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AI_RECharacterBase.h"
#include "AI_REStatusComponent.h"
#include "AbilitySystemComponent.h"
#include "AI_REAttributeSet.h"
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

void UAI_REPlayerCombatComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	TryStopPrimaryAction();
	if (MontageLoadHandle.IsValid())
	{
		MontageLoadHandle->CancelHandle();
		MontageLoadHandle.Reset();
	}
	CachedAttackMontage = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UAI_REPlayerCombatComponent::TryStartPrimaryAction()
{
	if (bIsActionActive) return;

	AAI_RECharacterBase* OwnerChar = Cast<AAI_RECharacterBase>(GetOwner());
	if (OwnerChar)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerChar))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				if (ASC->GetNumericAttribute(UAI_REAttributeSet::GetSPAttribute()) < AttackStaminaCost)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Not enough SP to attack!"));
					return;
				}
				
				// Consume SP
				ASC->ApplyModToAttributeUnsafe(UAI_REAttributeSet::GetSPAttribute(), EGameplayModOp::Additive, -AttackStaminaCost);
			}
		}
	}

	bIsActionActive = true;
	ActiveExecutionId = FGuid::NewGuid();

	// Play async loaded montage if available
	if (CachedAttackMontage && OwnerChar)
	{
		OwnerChar->PlayAnimMontage(CachedAttackMontage);
	}

	// In single player, instantly perform the hit for maximum responsiveness.
	// We can hook up AnimNotifies to trigger PerformTraceHit() later for animation sync.
	PerformTraceHit();

	// Automatically end the action after a short duration (placeholder for animation length).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ActionTimerHandle, [this]() { TryStopPrimaryAction(); }, 0.5f, false);
	}
}

void UAI_REPlayerCombatComponent::TryStopPrimaryAction()
{
	bIsActionActive = false;
	ActiveExecutionId.Invalidate();
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimerHandle);
	}
}

void UAI_REPlayerCombatComponent::CancelPrimaryActionForEvade()
{
	TryStopPrimaryAction();
	if (AAI_RECharacterBase* OwnerCharacter =
		Cast<AAI_RECharacterBase>(GetOwner());
		IsValid(OwnerCharacter) && IsValid(CachedAttackMontage))
	{
		OwnerCharacter->StopAnimMontage(CachedAttackMontage);
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
	// TODO: Destroy/Hide Weapon Mesh
}

void UAI_REPlayerCombatComponent::PerformTraceHit()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	float TraceDist = 150.f; // Fallback
	float BaseDmg = 10.f;    // Fallback
	float StaggerValue = AttackStaggerValue;
	EAIRECombatTargetingMode TargetingMode =
		EAIRECombatTargetingMode::SingleTarget;

	if (EquippedWeapon)
	{
		if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(EquippedWeapon))
		{
			if (WeaponItem->WeaponDefinition)
			{
				TraceDist = WeaponItem->WeaponDefinition->AttackRange;
				BaseDmg = WeaponItem->WeaponDefinition->Damage;
				StaggerValue = WeaponItem->WeaponDefinition->StaggerValue;
				TargetingMode =
					WeaponItem->WeaponDefinition->TargetingMode;
			}
		}
	}

	FVector TraceStart = ViewLocation;
	FVector TraceEnd = TraceStart + (ViewRotation.Vector() * TraceDist);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			// Optional: draw debug line
			DrawDebugLine(GetWorld(), TraceStart, HitResult.ImpactPoint, FColor::Red, false, 2.0f);
			DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 10.0f, FColor::Green, false, 2.0f);

			if (HitActor->Implements<UAIRECombatDamageTargetInterface>()
				&& TargetingMode
					== EAIRECombatTargetingMode::SingleTarget)
			{
				if (UAIRECombatDamageSubsystem* DamageSubsystem =
					GetWorld()->GetSubsystem<UAIRECombatDamageSubsystem>())
				{
					FAIRECombatDamageRequest DamageRequest;
					DamageRequest.Source = OwnerPawn;
					DamageRequest.Target = HitActor;
					DamageRequest.Damage = BaseDmg;
					DamageRequest.StaggerValue = StaggerValue;
					DamageRequest.ExecutionId = ActiveExecutionId;
					DamageRequest.bHasHitResult = true;
					DamageRequest.HitResult = HitResult;
					DamageSubsystem->ApplyDamageRequest(DamageRequest);
				}
			}
			else if (HitActor->Implements<UAI_REHarvestDamageTarget>())
			{
				IAI_REHarvestDamageTarget::Execute_ApplyHarvestDamage(HitActor, BaseDmg, OwnerPawn);
			}
			
			OnPrimaryActionHit.Broadcast(HitActor);
		}
	}
}

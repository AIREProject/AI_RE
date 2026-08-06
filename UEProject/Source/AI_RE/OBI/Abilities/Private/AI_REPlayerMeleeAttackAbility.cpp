
#include "AI_REPlayerMeleeAttackAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "AI_REHarvestDamageTarget.h"
#include "AI_REPlayerCombatComponent.h"
#include "AI_REWeaponItemDataAsset.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"

UAI_REPlayerMeleeAttackAbility::UAI_REPlayerMeleeAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UAI_REPlayerMeleeAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Wait for Gameplay Event from AnimNotify
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FGameplayTag::RequestGameplayTag(FName("Event.Combat.MeleeHit")),
		nullptr,
		false,
		true);
		
	if (HitEventTask)
	{
		HitEventTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleHitEvent);
		HitEventTask->ReadyForActivation();
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAnimMontage* MontageToPlay = AttackMontage;
	
	if (Character)
	{
		if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
		{
			if (CombatComp->GetCachedAttackMontage())
			{
				MontageToPlay = CombatComp->GetCachedAttackMontage();
			}
		}
	}

	if (MontageToPlay)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("PlayerMeleeMontage"),
			MontageToPlay,
			1.0f,
			NAME_None,
			true,
			0.0f);

		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleMontageInterrupted);
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		// 몽타주가 없으면 바로 타격 처리
		PerformTraceHit();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UAI_REPlayerMeleeAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	MontageTask = nullptr;
	HitEventTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAI_REPlayerMeleeAttackAbility::HandleMontageCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UAI_REPlayerMeleeAttackAbility::HandleMontageInterrupted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UAI_REPlayerMeleeAttackAbility::HandleHitEvent(FGameplayEventData Payload)
{
	PerformTraceHit();
}

void UAI_REPlayerMeleeAttackAbility::PerformTraceHit()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	float TraceDist = TraceDistance;
	float Dmg = BaseDamage;
	
	// 무기에 설정된 사거리 및 대미지 동기화
	if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
	{
		if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(CombatComp->EquippedWeapon))
		{
			if (WeaponItem->WeaponDefinition)
			{
				TraceDist = WeaponItem->WeaponDefinition->AttackRange;
				Dmg = WeaponItem->WeaponDefinition->Damage;
			}
		}
	}

	FVector TraceStart = ViewLocation;
	FVector TraceEnd = TraceStart + (ViewRotation.Vector() * TraceDist);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 10.0f, FColor::Green, false, 2.0f);

			// 하이브리드 최적화 분기: ASC가 있는 대상 vs 단순 자원(나무/돌)
			UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor, true);
			
			if (TargetASC && DamageEffectClass)
			{
				// 1. 적(몬스터/동료) 타격: GAS 이펙트 적용
				UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
				if (SourceASC)
				{
					FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
					EffectContext.AddSourceObject(Character);
					
					FGameplayEffectSpecHandle DamageSpec = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
					if (DamageSpec.IsValid())
					{
						SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
					}
				}
			}
			else if (HitActor->Implements<UAI_REHarvestDamageTarget>())
			{
				// 2. 자원 타격: 무거운 GAS 안 거치고 인터페이스로 즉시 데미지 가함
				IAI_REHarvestDamageTarget::Execute_ApplyHarvestDamage(HitActor, Dmg, Character);
			}
		}
	}
}


#include "AI_REPlayerMeleeAttackAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "AI_REHarvestDamageTarget.h"
#include "AI_REPlayerCombatComponent.h"
#include "AI_REWeaponItemDataAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "../../LMK/MAKO/Public/Equipment/AIRECompanionWeaponDefinitionDataAsset.h"

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

	CurrentComboIndex = 1;
	bIsComboWindowOpen = false;
	bHasComboInput = false;

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

	// 연속 판정용 이벤트 수신 대기 (다단 히트/트레이스)
	ActiveHitStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Combat.ActiveHit.Start")), nullptr, false, false);
	if (ActiveHitStartTask)
	{
		ActiveHitStartTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleActiveHitStart);
		ActiveHitStartTask->ReadyForActivation();
	}

	ActiveHitEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Combat.ActiveHit.End")), nullptr, false, false);
	if (ActiveHitEndTask)
	{
		ActiveHitEndTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleActiveHitEnd);
		ActiveHitEndTask->ReadyForActivation();
	}

	ComboWindowOpenTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Combat.ComboWindowOpen")), nullptr, false, false);
	if (ComboWindowOpenTask)
	{
		ComboWindowOpenTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleComboWindowOpen);
		ComboWindowOpenTask->ReadyForActivation();
	}

	ComboWindowCloseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Combat.ComboWindowClose")), nullptr, false, false);
	if (ComboWindowCloseTask)
	{
		ComboWindowCloseTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleComboWindowClose);
		ComboWindowCloseTask->ReadyForActivation();
	}

	ComboInputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Combat.ComboInput")), nullptr, false, false);
	if (ComboInputTask)
	{
		ComboInputTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleComboInput);
		ComboInputTask->ReadyForActivation();
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
			FName("Combo1"),
			true,
			1.0f,
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
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ActiveHitTimerHandle);
	}
	HitActorsThisSwing.Empty();

	MontageTask = nullptr;
	HitEventTask = nullptr;
	ActiveHitStartTask = nullptr;
	ActiveHitEndTask = nullptr;
	ComboWindowOpenTask = nullptr;
	ComboWindowCloseTask = nullptr;
	ComboInputTask = nullptr;

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
	// 기존 단발성 타격 처리 (한 번만 검사)
	HitActorsThisSwing.Empty();
	PerformTraceHit();
}

void UAI_REPlayerMeleeAttackAbility::HandleActiveHitStart(FGameplayEventData Payload)
{
	// 연속 판정 시작 (배열 비우고 타이머 시작)
	HitActorsThisSwing.Empty();

	if (GetWorld())
	{
		// 0.05초마다 PerformTraceHit 실행
		GetWorld()->GetTimerManager().SetTimer(ActiveHitTimerHandle, this, &UAI_REPlayerMeleeAttackAbility::PerformTraceHit, 0.05f, true);
	}
}

void UAI_REPlayerMeleeAttackAbility::HandleActiveHitEnd(FGameplayEventData Payload)
{
	// 연속 판정 끝 (타이머 정지 및 배열 비우기)
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ActiveHitTimerHandle);
	}
	HitActorsThisSwing.Empty();
}

void UAI_REPlayerMeleeAttackAbility::HandleComboWindowOpen(FGameplayEventData Payload)
{
	bIsComboWindowOpen = true;
	bHasComboInput = false;
}

void UAI_REPlayerMeleeAttackAbility::HandleComboWindowClose(FGameplayEventData Payload)
{
	bIsComboWindowOpen = false;
	
	if (!bHasComboInput)
	{
		// 유저가 입력하지 않았다면, 여기서 몽타주를 멈춰서 Idle로 부드럽게 넘어가게 함 (후딜레이 모션이 없으므로)
		ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
		{
			UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance();
			// 0.25초의 Blend Out 시간을 주어 뚝 끊기지 않고 부드럽게 멈춥니다.
			AnimInst->Montage_Stop(0.25f, CurrentMontage);
		}
	}
	else
	{
		// 콤보 입력이 성공했다면, 점프하지 않고 '가만히' 둡니다!
		// 몽타주에 Linked 세팅이 되어있으므로 물 흐르듯 자연스럽게 다음 타격으로 넘어갑니다.
		// 다음 입력을 받기 위해 상태만 초기화합니다.
		bHasComboInput = false;
	}
}

void UAI_REPlayerMeleeAttackAbility::HandleComboInput(FGameplayEventData Payload)
{
	if (bIsComboWindowOpen && !bHasComboInput)
	{
		// 콤보 예약 확정 (점프는 Close 노티파이에서 수행함)
		bHasComboInput = true;
		
		ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
		{
			UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance();
			CurrentMontage = AnimInst->GetCurrentActiveMontage();
			
			CurrentComboIndex++;
		}
	}
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

	// 카메라 대신 '캐릭터의 가슴팍'에서 '캐릭터가 바라보는 앞방향'으로 쏩니다. (근접 공격에 훨씬 자연스러움)
	FVector TraceStart = Character->GetActorLocation() + FVector(0.f, 0.f, 30.f); 
	FVector TraceEnd = TraceStart + (Character->GetActorForwardVector() * TraceDist);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);

	// 얇은 선(Line) 대신 두꺼운 구체(Sphere)로 휩쓸어서(Sweep) 판정 크기를 대폭 키웁니다.
	FCollisionShape Sphere = FCollisionShape::MakeSphere(45.0f); 

	if (GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, Sphere, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();
		// 구체 트레이스 시각화 (빨간색 -> 닿으면 초록색)
		// DrawDebugCapsule(GetWorld(), TraceStart + (TraceEnd - TraceStart) * HitResult.Time, 45.0f, 45.0f, FQuat::Identity, FColor::Green, false, 2.0f);
		// DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 15.0f, FColor::Red, false, 2.0f);

		if (HitActor && !HitActorsThisSwing.Contains(HitActor))
		{
			// 이번 스윙에 처음 맞은 놈이라면 기록
			HitActorsThisSwing.Add(HitActor);

			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, FString::Printf(TEXT("💥 [Debug] 타격 성공! 대상: %s, 데미지: %.1f"), *HitActor->GetName(), Dmg));

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
	else
	{
		// 허공에 휘둘렀을 때 빨간 캡슐 표시
		DrawDebugCapsule(GetWorld(), TraceStart + (TraceEnd - TraceStart) * 0.5f, (TraceEnd - TraceStart).Size() * 0.5f, 45.0f, FRotationMatrix::MakeFromZ(TraceEnd - TraceStart).ToQuat(), FColor::Red, false, 1.0f);
	}
}

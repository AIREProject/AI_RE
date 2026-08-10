
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
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("[Debug] HandleHitEvent Fired! (Array Cleared)"));
	// 기존 단발성 타격 처리 (한 번만 검사)
	HitActorsThisSwing.Empty();
	PerformTraceHit();
}

void UAI_REPlayerMeleeAttackAbility::HandleActiveHitStart(FGameplayEventData Payload)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("[Debug] HandleActiveHitStart Fired! (Array Cleared)"));
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
	
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;
	
	UAnimInstance* AnimInst = nullptr;
	if (Character->GetMesh()) AnimInst = Character->GetMesh()->GetAnimInstance();
	if (!AnimInst) return;

	bool bIsWeapon = false;
	if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
	{
		// 장착된 무기가 '맨손 전용 기본 데이터 에셋'이 아닐 경우에만 무기로 판정합니다.
		if (CombatComp->EquippedWeapon != nullptr && CombatComp->EquippedWeapon != CombatComp->DefaultUnarmedWeapon)
		{
			bIsWeapon = true;
		}
	}

	if (!bHasComboInput)
	{
		// 유저가 입력하지 않았다면, 아무것도 하지 않습니다. (애니메이션 원본의 후딜레이가 끝까지 자연스럽게 재생됩니다)
	}
	else
	{
		// 콤보 입력이 성공했다면
		if (bIsWeapon)
		{
			// 대검 등: 한 섹션이 완전히 끝난 뒤 부드럽게 이어지도록 SetNextSection 사용
			FString CurrentSectionStr = FString::Printf(TEXT("Combo%d"), CurrentComboIndex - 1); 
			FString NextSectionStr = FString::Printf(TEXT("Combo%d"), CurrentComboIndex); 
			AnimInst->Montage_SetNextSection(FName(*CurrentSectionStr), FName(*NextSectionStr), CurrentMontage);
		}
		else
		{
			// 맨손 등: 기존처럼 즉시 가로채서(점프) 타격
			FString NextSectionStr = FString::Printf(TEXT("Combo%d"), CurrentComboIndex); 
			AnimInst->Montage_JumpToSection(FName(*NextSectionStr), CurrentMontage);
		}
		
		// 실제 다음 타격 애니메이션이 이어지거나 점프되는 시점(동작 수행 시점)에 스테미나 소모!
		CommitAbilityCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());

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
	float TraceRad = 15.0f; // 기본 맨손(주먹) 판정 반지름
	bool bIsWeapon = false;
	
	// 무기에 설정된 사거리 및 대미지, 판정 크기 동기화
	if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
	{
		if (CombatComp->EquippedWeapon != nullptr && CombatComp->EquippedWeapon != CombatComp->DefaultUnarmedWeapon)
		{
			bIsWeapon = true;
		}
		
		if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(CombatComp->EquippedWeapon))
		{
			if (WeaponItem->WeaponDefinition)
			{
				TraceDist = WeaponItem->WeaponDefinition->AttackRange;
				Dmg = WeaponItem->WeaponDefinition->Damage;
				TraceRad = WeaponItem->WeaponDefinition->TraceRadius;
			}
		}
	}

	// 가슴팍에서 캐릭터 앞방향으로 쏨
	FVector TraceStart = Character->GetActorLocation() + FVector(0.f, 0.f, 30.f); 
	FVector TraceEnd = TraceStart + (Character->GetActorForwardVector() * TraceDist);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	FCollisionShape Sphere = FCollisionShape::MakeSphere(TraceRad); 

	if (bIsWeapon)
	{
		// [대검 등 무기] 광역 판정 (다수 적 긁기)
		TArray<FHitResult> HitResults;
		if (GetWorld()->SweepMultiByChannel(HitResults, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, Sphere, QueryParams))
		{
			for (const FHitResult& HitResult : HitResults)
			{
				AActor* HitActor = HitResult.GetActor();
				if (HitActor && !HitActorsThisSwing.Contains(HitActor))
				{
					// 이번 스윙에 처음 맞은 놈이라면 기록
					HitActorsThisSwing.Add(HitActor);
					ProcessHit(HitActor, Dmg, Character);
				}
			}
		}
	}
	else
	{
		// [주먹 등 맨손] 단일 판정 (가장 먼저 닿은 1명만)
		FHitResult HitResult;
		if (GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, Sphere, QueryParams))
		{
			AActor* HitActor = HitResult.GetActor();
			
			// 주먹이 닿은 정확한 지점에 디버그 시각화 (기획자님 요청 복구)
			DrawDebugCapsule(GetWorld(), TraceStart + (TraceEnd - TraceStart) * HitResult.Time, TraceRad, TraceRad, FQuat::Identity, FColor::Green, false, 2.0f);
			DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 15.0f, FColor::Red, false, 2.0f);

			if (HitActor && !HitActorsThisSwing.Contains(HitActor))
			{
				HitActorsThisSwing.Add(HitActor);
				ProcessHit(HitActor, Dmg, Character);
			}
		}
		else
		{
			// 허공에 주먹질 시 빨간 캡슐 표시
			DrawDebugCapsule(GetWorld(), TraceStart + (TraceEnd - TraceStart) * 0.5f, (TraceEnd - TraceStart).Size() * 0.5f, TraceRad, FRotationMatrix::MakeFromZ(TraceEnd - TraceStart).ToQuat(), FColor::Red, false, 1.0f);
		}
	}
}

void UAI_REPlayerMeleeAttackAbility::ProcessHit(AActor* HitActor, float Dmg, ACharacter* Character)
{
	if (GEngine && HitActor) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("[Debug] ProcessHit applied to: %s"), *HitActor->GetName()));
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, FString::Printf(TEXT("💥 [Debug] 타격 성공! 대상: %s, 데미지: %.1f"), *HitActor->GetName(), Dmg));

	// 하이브리드 최적화 분기: ASC가 있는 대상 vs 단순 자원(나무/돌)
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor, true);
	
	if (TargetASC && DamageEffectClass)
	{
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
		IAI_REHarvestDamageTarget::Execute_ApplyHarvestDamage(HitActor, Dmg, Character);
	}
}


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
#include "AI_RECharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "AbilitySystem/Combat/AIRECompanionCombatVFX.h"

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
	bIsActiveHitEnded = false;

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

	TraceEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Attack.TraceBegin"), false), nullptr, false, false);
	if (TraceEventTask)
	{
		TraceEventTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleTraceEvent);
		TraceEventTask->ReadyForActivation();
	}

	TraceSampleTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Attack.TraceSample"), false), nullptr, false, false);
	if (TraceSampleTask)
	{
		TraceSampleTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleTraceEvent);
		TraceSampleTask->ReadyForActivation();
	}

	TraceEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Attack.TraceEnd"), false), nullptr, false, false);
	if (TraceEndTask)
	{
		TraceEndTask->EventReceived.AddDynamic(this, &UAI_REPlayerMeleeAttackAbility::HandleTraceEvent);
		TraceEndTask->ReadyForActivation();
	}

	// [하위 호환] 구형 애니메이션 노티파이(ActiveHit) 이벤트는 TraceBegin/End 로 대체되었으므로 리스너 제거
	// ActiveHitStartTask, ActiveHitEndTask 리스너를 더 이상 생성하지 않음 (이중 호출 방지)


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

	HitActorsThisSwing.Empty();

	MontageTask = nullptr;
	HitEventTask = nullptr;
	TraceEventTask = nullptr;
	TraceSampleTask = nullptr;
	TraceEndTask = nullptr;
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

void UAI_REPlayerMeleeAttackAbility::HandleTraceEvent(FGameplayEventData Payload)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;
	
	USceneComponent* TraceMesh = nullptr;
	if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
	{
		if (CombatComp->EquippedWeapon && CombatComp->EquippedWeapon != CombatComp->DefaultUnarmedWeapon)
		{
			if (AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(Character))
			{
				TraceMesh = PlayerChar->GetWeaponMeshComponent();
			}
		}
	}
	
	if (!TraceMesh)
	{
		TraceMesh = Character->GetMesh();
	}

	FName ReceivedTagName = Payload.EventTag.GetTagName();

	if (ReceivedTagName == FName("Event.Attack.TraceBegin") ||
		ReceivedTagName == FName("Event.Combat.ActiveHit.Start"))
	{
		HitActorsThisSwing.Empty();
		bIsActiveHitEnded = false;
		TryBeginCurrentStepTrace(TraceMesh);
	}
	else if (ReceivedTagName == FName("Event.Attack.TraceSample"))
	{
		if (bUseFallbackTrace)
		{
			PerformTraceHit();
		}
		else
		{
			FHitResult TargetHit;
			EAIRECombatMeleeTraceResult TraceResult = SampleCurrentStepCombatTrace(TraceMesh, TargetHit);
			ResolveCurrentStepTraceSample(TraceResult, TargetHit);
		}
	}
	else if (ReceivedTagName == FName("Event.Attack.TraceEnd") ||
			 ReceivedTagName == FName("Event.Combat.ActiveHit.End"))
	{
		if (bUseFallbackTrace)
		{
			PerformTraceHit();
		}
		else
		{
			FHitResult TargetHit;
			EAIRECombatMeleeTraceResult TraceResult = SampleCurrentStepCombatTrace(TraceMesh, TargetHit);
			ResolveCurrentStepTraceSample(TraceResult, TargetHit);
			CloseCurrentStepTrace();
		}
		
		HitActorsThisSwing.Empty();
		bIsActiveHitEnded = true;

		if (bHasComboInput)
		{
			TryComboTransition();
		}
	}
}

void UAI_REPlayerMeleeAttackAbility::TryBeginCurrentStepTrace(USceneComponent* MeshComponent)
{
	bTraceWindowOpen = true;
	ActiveTraceMesh = MeshComponent;
	
	CurrentTraceStartSocket = NAME_None;
	CurrentTraceEndSocket = NAME_None;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character)
	{
		if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
		{
			if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(CombatComp->EquippedWeapon))
			{
				// [종속성 제거] MAKO의 WeaponDefinition이 아닌 플레이어 WeaponItem에서 직접 소켓 이름을 가져옵니다.
				CurrentTraceStartSocket = WeaponItem->TraceStartSocket;
				CurrentTraceEndSocket = WeaponItem->TraceEndSocket;
			}
		}
	}

	// Fallback to default names if trace sockets were not configured
	if (CurrentTraceStartSocket.IsNone()) CurrentTraceStartSocket = FName("TraceStart");
	if (CurrentTraceEndSocket.IsNone()) CurrentTraceEndSocket = FName("TraceEnd");

	// If the specified sockets don't exist, try the universal defaults "TraceStart" and "TraceEnd"
	if (!MeshComponent->DoesSocketExist(CurrentTraceStartSocket) || !MeshComponent->DoesSocketExist(CurrentTraceEndSocket))
	{
		if (MeshComponent->DoesSocketExist(FName("TraceStart")) && MeshComponent->DoesSocketExist(FName("TraceEnd")))
		{
			CurrentTraceStartSocket = FName("TraceStart");
			CurrentTraceEndSocket = FName("TraceEnd");
		}
	}

	if (!MeshComponent->DoesSocketExist(CurrentTraceStartSocket) || !MeshComponent->DoesSocketExist(CurrentTraceEndSocket))
	{
		bUseFallbackTrace = true;
	}

	if (MeshComponent->DoesSocketExist(CurrentTraceStartSocket) && MeshComponent->DoesSocketExist(CurrentTraceEndSocket))
	{
		bUseFallbackTrace = false;
		PreviousTraceStart = MeshComponent->GetSocketLocation(CurrentTraceStartSocket);
		PreviousTraceEnd = MeshComponent->GetSocketLocation(CurrentTraceEndSocket);
	}
	else
	{
		bUseFallbackTrace = true;
	}
	
	FHitResult TargetHit;
	EAIRECombatMeleeTraceResult TraceResult = SampleCurrentStepCombatTrace(MeshComponent, TargetHit);
	ResolveCurrentStepTraceSample(TraceResult, TargetHit);
}

EAIRECombatMeleeTraceResult UAI_REPlayerMeleeAttackAbility::SampleCurrentStepCombatTrace(USceneComponent* MeshComponent, FHitResult& OutTargetHit)
{
	if (!bTraceWindowOpen)
	{
		return EAIRECombatMeleeTraceResult::Invalid;
	}
	if (ActiveTraceMesh.Get() != MeshComponent)
	{
		return EAIRECombatMeleeTraceResult::Invalid;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return EAIRECombatMeleeTraceResult::Invalid;
	}

	if (!MeshComponent->DoesSocketExist(CurrentTraceStartSocket) || !MeshComponent->DoesSocketExist(CurrentTraceEndSocket))
	{
		return EAIRECombatMeleeTraceResult::Invalid;
	}

	FVector TraceStart = MeshComponent->GetSocketLocation(CurrentTraceStartSocket);
	FVector TraceEnd = MeshComponent->GetSocketLocation(CurrentTraceEndSocket);

	float TraceRad = 15.0f;
	if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
	{
		if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(CombatComp->EquippedWeapon))
		{
			if (WeaponItem->WeaponDefinition)
			{
				TraceRad = WeaponItem->WeaponDefinition->TraceRadius;
			}
		}
	} 

	FAIRECombatMeleeTraceRequest TraceRequest;
	TraceRequest.World = GetWorld();
	TraceRequest.Source = Character;
	TraceRequest.Target = nullptr;
	TraceRequest.Shape = EAIRECombatMeleeTraceShape::Capsule;
	TraceRequest.Radius = TraceRad;
	TraceRequest.TraceChannel = ECC_Pawn; 
	
	for (const TWeakObjectPtr<AActor>& WeakActor : HitActorsThisSwing)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			TraceRequest.IgnoredActors.Add(Actor);
		}
	}

	float Distance = FVector::Distance(TraceStart, TraceEnd);
	TraceRequest.CapsuleHalfHeight = (Distance * 0.5f) + TraceRad;

	FVector CapsuleCenter = (TraceStart + TraceEnd) * 0.5f;
	
	FVector TraceDir = TraceEnd - TraceStart;
	if (TraceDir.IsNearlyZero())
	{
		TraceDir = MeshComponent->GetUpVector(); // Fallback if sockets are at the exact same location
	}
	FQuat CapsuleRot = FRotationMatrix::MakeFromZ(TraceDir).ToQuat();

	FVector PrevCapsuleCenter = (PreviousTraceStart + PreviousTraceEnd) * 0.5f;

	TraceRequest.Segments.Emplace(PrevCapsuleCenter, CapsuleCenter, CapsuleRot);

	const FAIRECombatMeleeTraceResolution Resolution = FAIRECombatMeleeTraceResolver::Resolve(TraceRequest);
	OutTargetHit = Resolution.HitResult;

	PreviousTraceStart = TraceStart;
	PreviousTraceEnd = TraceEnd;

	return Resolution.Result;
}

void UAI_REPlayerMeleeAttackAbility::ResolveCurrentStepTraceSample(EAIRECombatMeleeTraceResult TraceResult, const FHitResult& TargetHit)
{
	if (TraceResult == EAIRECombatMeleeTraceResult::TargetHit || TraceResult == EAIRECombatMeleeTraceResult::Blocked)
	{
		AActor* HitActor = TargetHit.GetActor();
		if (HitActor && !HitActorsThisSwing.Contains(HitActor))
		{
			HitActorsThisSwing.Add(HitActor);
			
			float Dmg = BaseDamage;
			ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
			if (Character)
			{
				if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
				{
					if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(CombatComp->EquippedWeapon))
					{
						if (WeaponItem->WeaponDefinition)
						{
							Dmg = WeaponItem->WeaponDefinition->Damage;
						}
					}
				}
				ProcessHit(TargetHit, Dmg, Character);
			}
		}
	}
}

void UAI_REPlayerMeleeAttackAbility::CloseCurrentStepTrace()
{
	bTraceWindowOpen = false;
	ActiveTraceMesh.Reset();
	PreviousTraceStart = FVector::ZeroVector;
	PreviousTraceEnd = FVector::ZeroVector;
}

void UAI_REPlayerMeleeAttackAbility::HandleComboWindowOpen(FGameplayEventData Payload)
{
	bIsComboWindowOpen = true;
	// 미리 입력해둔 버퍼를 날려버리지 않도록 초기화 제거
}

void UAI_REPlayerMeleeAttackAbility::HandleComboWindowClose(FGameplayEventData Payload)
{
	bIsComboWindowOpen = false;
	
	if (!bHasComboInput) return;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;

	bool bIsWeapon = false;
	if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
	{
		if (CombatComp->EquippedWeapon != nullptr && CombatComp->EquippedWeapon != CombatComp->DefaultUnarmedWeapon)
		{
			bIsWeapon = true;
		}
	}

	if (!bIsWeapon)
	{
		// 맨손 애니메이션은 ActiveHitEnd 노티파이가 없으므로, 
		// 콤보 창이 닫히는 이 시점에 다음 콤보로 넘깁니다. (이때 변수들도 초기화됨)
		TryComboTransition();
	}
}

void UAI_REPlayerMeleeAttackAbility::TryComboTransition()
{
	if (!bHasComboInput) return;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;
	
	UAnimInstance* AnimInst = nullptr;
	if (Character->GetMesh()) AnimInst = Character->GetMesh()->GetAnimInstance();
	if (!AnimInst) return;

	// 맨손이든 무기든 이 함수가 호출되었다면 '즉시' 점프합니다. (JumpToSection 사용)
	FString NextSectionStr = FString::Printf(TEXT("Combo%d"), CurrentComboIndex); 
	AnimInst->Montage_JumpToSection(FName(*NextSectionStr), CurrentMontage);
	
	CommitAbilityCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
	
	// 다음 타격을 위해 모든 상태를 깨끗하게 초기화 (이전 섹션의 노티파이 잔여물 제거)
	bHasComboInput = false;
	bIsActiveHitEnded = false;
	bIsComboWindowOpen = false;
}

void UAI_REPlayerMeleeAttackAbility::HandleComboInput(FGameplayEventData Payload)
{
	// 콤보 예약이 아직 안 되어있을 때만 허용 (창이 열려있든 아니든 미리 버퍼링 가능)
	if (!bHasComboInput)
	{
		bHasComboInput = true;
		
		ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
		{
			UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance();
			CurrentMontage = AnimInst->GetCurrentActiveMontage();
			
			CurrentComboIndex++;

			// ActiveHit이 이미 끝난 후딜레이 상태에서 입력이 들어왔다면 대기 없이 즉시 점프
			if (bIsActiveHitEnded)
			{
				TryComboTransition();
			}
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
		if (GetWorld()->SweepMultiByChannel(HitResults, TraceStart, TraceEnd, FQuat::Identity, ECC_Pawn, Sphere, QueryParams))
		{
			for (const FHitResult& HitResult : HitResults)
			{
				AActor* HitActor = HitResult.GetActor();
				if (HitActor && !HitActorsThisSwing.Contains(HitActor))
				{
					// 이번 스윙에 처음 맞은 놈이라면 기록
					HitActorsThisSwing.Add(HitActor);
					ProcessHit(HitResult, Dmg, Character);
				}
			}
		}
	}
	else
	{
		// [주먹 등 맨손] 단일 판정 (가장 먼저 닿은 1명만)
		FHitResult HitResult;
		if (GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Pawn, Sphere, QueryParams))
		{
			AActor* HitActor = HitResult.GetActor();
			
			// 주먹이 닿은 정확한 지점에 디버그 시각화
			DrawDebugCapsule(GetWorld(), TraceStart + (TraceEnd - TraceStart) * HitResult.Time, TraceRad, TraceRad, FQuat::Identity, FColor::Green, false, 2.0f);
			DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 15.0f, FColor::Red, false, 2.0f);

			if (HitActor && !HitActorsThisSwing.Contains(HitActor))
			{
				HitActorsThisSwing.Add(HitActor);
				ProcessHit(HitResult, Dmg, Character);
			}
		}
		else
		{
			// 허공에 주먹질 시 빨간 캡슐 표시
			DrawDebugCapsule(GetWorld(), TraceStart + (TraceEnd - TraceStart) * 0.5f, (TraceEnd - TraceStart).Size() * 0.5f, TraceRad, FRotationMatrix::MakeFromZ(TraceEnd - TraceStart).ToQuat(), FColor::Red, false, 1.0f);
		}
	}
}

void UAI_REPlayerMeleeAttackAbility::ProcessHit(const FHitResult& HitResult, float Dmg, ACharacter* Character)
{
	AActor* HitActor = HitResult.GetActor();

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
				DamageSpec.Data.Get()->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), Dmg);
				SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
			}
		}
	}
	else if (HitActor->Implements<UAI_REHarvestDamageTarget>())
	{
		IAI_REHarvestDamageTarget::Execute_ApplyHarvestDamage(HitActor, Dmg, Character);
	}

	// [추가된 로직] 무기 데이터에서 이펙트(BossHitSlashEffect) 읽어와서 스폰하기
	if (UAI_REPlayerCombatComponent* CombatComp = Character->GetComponentByClass<UAI_REPlayerCombatComponent>())
	{
		if (UAI_REWeaponItemDataAsset* WeaponItem = Cast<UAI_REWeaponItemDataAsset>(CombatComp->EquippedWeapon))
		{
			if (WeaponItem->WeaponDefinition)
			{
				AIRECompanionCombatVFX::SpawnBossHitSlash(WeaponItem->WeaponDefinition, Character, HitActor, HitResult);
			}
		}
	}
}

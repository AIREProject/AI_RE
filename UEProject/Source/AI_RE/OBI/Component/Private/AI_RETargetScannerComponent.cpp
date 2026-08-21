// Copyright MixUpProject. All Rights Reserved.

#include "AI_RETargetScannerComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "AI_REInteractableInterface.h"
#include "AI_REHarvestDamageTarget.h"
#include "AI_REHarvestableResourceComponent.h"
#include "AIRECombatDamageTargetInterface.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

namespace
{
	constexpr int32 InteractionStencilValue = 1;

	bool IsPlayerTargetCandidate(const AActor* Candidate)
	{
		if (!IsValid(Candidate))
		{
			return false;
		}
		if (Candidate->Implements<UAI_REHarvestDamageTarget>())
		{
			const UAI_REHarvestableResourceComponent* ResourceComponent =
				Candidate->FindComponentByClass<UAI_REHarvestableResourceComponent>();
			return !IsValid(ResourceComponent) || !ResourceComponent->IsDepleted();
		}
		if (!Candidate->Implements<UAIRECombatDamageTargetInterface>())
		{
			return false;
		}
		const IAIRECombatDamageTargetInterface* CombatTarget =
			Cast<IAIRECombatDamageTargetInterface>(Candidate);
		return CombatTarget
			&& CombatTarget->GetCombatAffiliation()
				== EAIRECombatAffiliation::Enemy
			&& AIRECombatDamageTarget::IsAlive(Candidate);
	}
}

UAI_RETargetScannerComponent::UAI_RETargetScannerComponent()
{
	// 틱(Tick)을 꺼서 엔진 부하를 원천 차단합니다. (대신 FTimerManager 사용)
	PrimaryComponentTick.bCanEverTick = false;
	
	ScanInterval = 0.5f;
}

void UAI_RETargetScannerComponent::BeginPlay()
{
	Super::BeginPlay();

	// 상호작용 프리체크 타이머 등록
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(InteractionScanTimerHandle, this, &UAI_RETargetScannerComponent::PerformInteractionPrecheck, ScanInterval, true);
	}
}

void UAI_RETargetScannerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InteractionScanTimerHandle);
	}
	SetCachedInteractableTarget(nullptr);
	
	Super::EndPlay(EndPlayReason);
}

AActor* UAI_RETargetScannerComponent::ScanForwardForTarget(float Radius, float Distance, ECollisionChannel TraceChannel, bool bDrawDebug)
{
	return ScanForward(Radius, Distance, TraceChannel, bDrawDebug, false, false);
}

AActor* UAI_RETargetScannerComponent::ScanForwardForPlayerTarget(
	const float Radius,
	const float Distance,
	const ECollisionChannel TraceChannel,
	const bool bDrawDebug)
{
	return ScanForward(Radius, Distance, TraceChannel, bDrawDebug, true, false);
}

AActor* UAI_RETargetScannerComponent::ScanForward(
	const float Radius,
	const float Distance,
	const ECollisionChannel TraceChannel,
	const bool bDrawDebug,
	const bool bRequirePlayerTarget,
	const bool bRequireInteractable)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return nullptr;

	FVector Start = OwnerActor->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	
	// 소유자의 시선(Control Rotation)이 필요할 수 있으므로, 폰(Pawn)인지 확인
	FVector ForwardDir = OwnerActor->GetActorForwardVector();
	if (APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		ForwardDir = OwnerPawn->GetControlRotation().Vector();
	}

	FVector End = Start + (ForwardDir * Distance);

	FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerActor); // 나 자신은 검사에서 제외

	GetWorld()->SweepMultiByChannel(
		HitResults, 
		Start, 
		End, 
		FQuat::Identity, 
		TraceChannel, 
		Sphere,
		QueryParams
	);

	if (bDrawDebug)
	{
		DrawDebugCapsule(GetWorld(), Start + (End - Start) * 0.5f, Distance * 0.5f, Radius, FRotationMatrix::MakeFromZ(End - Start).ToQuat(), FColor::Cyan, false, 0.5f);
	}

	for (const FHitResult& Hit : HitResults)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			if (bRequireInteractable
				&& !HitActor->Implements<UAI_REInteractableInterface>())
			{
				continue;
			}

			if (bRequirePlayerTarget && !IsPlayerTargetCandidate(HitActor))
			{
				continue;
			}

			// 전투 타겟팅용 스캔일 때 바닥(지형)이 잡히는 것을 방지합니다.
			// 폰이거나 AbilitySystemComponent를 가진 개체(나무, 자원 등)만 타겟으로 인정합니다.
			if (!bRequirePlayerTarget && TraceChannel == ECC_Pawn)
			{
				bool bHasASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor) != nullptr;
				if (!bHasASC && !HitActor->IsA(APawn::StaticClass()))
				{
					continue; // 폰도 아니고 ASC도 없다면 (예: 일반 바닥) 무시
				}
			}

			return HitActor;
		}
	}
	
	return nullptr;
}

void UAI_RETargetScannerComponent::PerformInteractionPrecheck()
{
	// 거리 3m(300)로 상시 스캔 (UI 프리체크용)
	AActor* HitActor = ScanForward(
		45.0f,
		300.0f,
		ECC_Visibility,
		false,
		false,
		true);
	
	if (HitActor && HitActor->Implements<UAI_REInteractableInterface>())
	{
		if (CachedInteractableTarget.Get() != HitActor)
		{
			SetCachedInteractableTarget(HitActor);
			// TODO: UI 띄우기 로직 연동 (나중에 UIManager에서 캐싱된 타겟을 확인하여 띄우게 됩니다)
			if (GEngine) GEngine->AddOnScreenDebugMessage(11, 0.5f, FColor::Yellow, FString::Printf(TEXT("[상호작용 가능] %s (by Component)"), *HitActor->GetName()));
		}

		// 적일 경우 자동 락온 방송
		if (IsPlayerTargetCandidate(HitActor))
		{
			if (!bIsCombatState || CurrentCombatTarget.Get() != HitActor)
			{
				bIsCombatState = true;
				CurrentCombatTarget = HitActor;
				OnCombatStateChanged.Broadcast(true, HitActor);
			}
		}
	}
	else
	{
		if (CachedInteractableTarget.IsValid())
		{
			CachedInteractableTarget.Reset();
			// TODO: UI 숨기기 연동
		}

		// 적이 사라졌으면 락온 해제 방송
		if (bIsCombatState)
		{
			bIsCombatState = false;
			CurrentCombatTarget.Reset();
			OnCombatStateChanged.Broadcast(false, nullptr);
		}
		SetCachedInteractableTarget(nullptr);
		// TODO: UI 숨기기 연동
	}
}

AActor* UAI_RETargetScannerComponent::GetCachedInteractableTarget() const
{
	return CachedInteractableTarget.Get();
}

void UAI_RETargetScannerComponent::RefreshInteractableTarget()
{
	PerformInteractionPrecheck();
	if (!CachedInteractableTarget.IsValid())
	{
		SetCachedInteractableTarget(FindBestInteractableInFront(300.0f));
	}
}

AActor* UAI_RETargetScannerComponent::FindBestInteractableInFront(
	const float MaxDistance) const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!IsValid(OwnerActor) || !IsValid(World) || MaxDistance <= 0.0f)
	{
		return nullptr;
	}

	FVector ForwardDirection = OwnerActor->GetActorForwardVector();
	if (const APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		ForwardDirection = OwnerPawn->GetControlRotation().Vector();
	}
	ForwardDirection = ForwardDirection.GetSafeNormal();

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const float MaxDistanceSquared = FMath::Square(MaxDistance);
	float BestScore = -1.0f;
	TWeakObjectPtr<AActor> BestTarget;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate)
			|| Candidate == OwnerActor
			|| !Candidate->Implements<UAI_REInteractableInterface>())
		{
			continue;
		}

		const FBox CandidateBounds = Candidate->GetComponentsBoundingBox(true);
		const FVector CandidatePoint = CandidateBounds.IsValid
			? CandidateBounds.GetClosestPointTo(OwnerLocation)
			: Candidate->GetActorLocation();
		const FVector ToCandidate = CandidatePoint - OwnerLocation;
		const float DistanceSquared = ToCandidate.SizeSquared();
		if (DistanceSquared > MaxDistanceSquared)
		{
			continue;
		}

		const float Distance = FMath::Sqrt(DistanceSquared);
		const FVector Direction = Distance > UE_KINDA_SMALL_NUMBER
			? ToCandidate / Distance
			: ForwardDirection;
		const float ViewAlignment = FVector::DotProduct(
			ForwardDirection,
			Direction);
		if (ViewAlignment < 0.0f)
		{
			continue;
		}

		const float DistanceScore = 1.0f - (Distance / MaxDistance);
		const float Score = ViewAlignment * 0.75f + DistanceScore * 0.25f;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget.Get();
}

void UAI_RETargetScannerComponent::SetCachedInteractableTarget(
	AActor* NewTarget)
{
	AActor* PreviousTarget = CachedInteractableTarget.Get();
	if (PreviousTarget == NewTarget)
	{
		return;
	}

	SetInteractionOutlineEnabled(PreviousTarget, false);
	CachedInteractableTarget = NewTarget;
	SetInteractionOutlineEnabled(NewTarget, true);
}

void UAI_RETargetScannerComponent::SetInteractionOutlineEnabled(
	AActor* Target,
	const bool bEnabled)
{
	if (!IsValid(Target))
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	Target->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		if (bEnabled)
		{
			MeshComponent->SetCustomDepthStencilValue(
				InteractionStencilValue);
		}
		MeshComponent->SetRenderCustomDepth(bEnabled);
	}
}

void UAI_RETargetScannerComponent::ResetCachedTarget()
{
	SetCachedInteractableTarget(nullptr);
}

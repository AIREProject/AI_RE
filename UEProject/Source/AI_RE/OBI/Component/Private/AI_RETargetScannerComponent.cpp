// Copyright MixUpProject. All Rights Reserved.

#include "AI_RETargetScannerComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "AI_REInteractableInterface.h"

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
	
	Super::EndPlay(EndPlayReason);
}

AActor* UAI_RETargetScannerComponent::ScanForwardForTarget(float Radius, float Distance, ECollisionChannel TraceChannel, bool bDrawDebug)
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
			return HitActor;
		}
	}
	
	return nullptr;
}

void UAI_RETargetScannerComponent::PerformInteractionPrecheck()
{
	// 기획자님 요청에 따라 거리 3m(300)로 상시 스캔 (UI 프리체크용)
	AActor* HitActor = ScanForwardForTarget(45.0f, 300.0f, ECC_Visibility, false); 
	
	if (HitActor && HitActor->Implements<UAI_REInteractableInterface>())
	{
		if (CachedInteractableTarget.Get() != HitActor)
		{
			CachedInteractableTarget = HitActor;
			// TODO: UI 띄우기 로직 연동 (나중에 UIManager에서 캐싱된 타겟을 확인하여 띄우게 됩니다)
			if (GEngine) GEngine->AddOnScreenDebugMessage(11, 0.5f, FColor::Yellow, FString::Printf(TEXT("[상호작용 가능] %s (by Component)"), *HitActor->GetName()));
		}
	}
	else
	{
		if (CachedInteractableTarget.IsValid())
		{
			CachedInteractableTarget.Reset();
			// TODO: UI 숨기기 연동
		}
	}
}

AActor* UAI_RETargetScannerComponent::GetCachedInteractableTarget() const
{
	return CachedInteractableTarget.Get();
}

void UAI_RETargetScannerComponent::ResetCachedTarget()
{
	CachedInteractableTarget.Reset();
}

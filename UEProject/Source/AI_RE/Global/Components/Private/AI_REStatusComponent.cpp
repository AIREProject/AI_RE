#include "AI_REStatusComponent.h"

#include "TimerManager.h"
#include "Engine/Engine.h"

UAI_REStatusComponent::UAI_REStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

    // 디폴트 스탯 세팅
	MaxHP = 100.f;
	MaxSP = 100.f;
	MaxHunger = 100.f;
	MaxThirsty = 100.f;
	
	Attack = 10.f;
	Defense = 10.f;
	WorkSpeed = 1.0f;
}

void UAI_REStatusComponent::BeginPlay()
{
	Super::BeginPlay();

	// 에디터(블루프린트)에서 수정된 Max 값을 기준으로 Current 값을 꽉 채워줍니다.
	CurrentHP = MaxHP;
	CurrentSP = MaxSP;
	CurrentHunger = MaxHunger;
	CurrentThirsty = MaxThirsty;

    // 시작 시 UI 업데이트용 델리게이트 브로드캐스트
	OnHPChanged.Broadcast(CurrentHP, MaxHP);
	OnSPChanged.Broadcast(CurrentSP, MaxSP);
	OnHungerChanged.Broadcast(CurrentHunger, MaxHunger);
	OnThirstyChanged.Broadcast(CurrentThirsty, MaxThirsty);

	// 생존 스탯(허기, 목마름) 자동 감소 타이머 실행 (2초 주기)
	GetWorld()->GetTimerManager().SetTimer(SurvivalTimerHandle, this, &UAI_REStatusComponent::HandleSurvivalStats, SurvivalTickRate, true);
}

void UAI_REStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAI_REStatusComponent::ConsumeSP(float Amount)
{
    CurrentSP = FMath::Clamp(CurrentSP - Amount, 0.f, MaxSP);
    OnSPChanged.Broadcast(CurrentSP, MaxSP);
}

void UAI_REStatusComponent::ApplyDamage(float Amount)
{
    float ActualDamage = FMath::Max(Amount - Defense, 1.f);
    CurrentHP = FMath::Clamp(CurrentHP - ActualDamage, 0.f, MaxHP);
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
}

void UAI_REStatusComponent::HandleSurvivalStats()
{
	float Multiplier = IsOwnerRunning() ? RunMultiplier : 1.0f;

	CurrentHunger = FMath::Clamp(CurrentHunger - (BaseHungerDepleteRate * Multiplier), 0.f, MaxHunger);
	CurrentThirsty = FMath::Clamp(CurrentThirsty - (BaseThirstyDepleteRate * Multiplier), 0.f, MaxThirsty);

	OnHungerChanged.Broadcast(CurrentHunger, MaxHunger);
	OnThirstyChanged.Broadcast(CurrentThirsty, MaxThirsty);
}

bool UAI_REStatusComponent::IsOwnerRunning() const
{
	if (AActor* Owner = GetOwner())
	{
		// 달리기 판정: 속도가 400.0f 이상이면 달리는 것으로 간주
		return Owner->GetVelocity().SizeSquared() > 600.f;
	}
	return false;
}


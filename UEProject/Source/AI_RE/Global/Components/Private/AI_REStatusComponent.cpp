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

	// 에디터에서 실수로 0으로 설정되는 것을 방지 (Division by Zero 오류 및 UI 미갱신 원인)
	if (MaxHP <= 0.f) MaxHP = 100.f;
	if (MaxSP <= 0.f) MaxSP = 100.f;
	if (MaxHunger <= 0.f) MaxHunger = 100.f;
	if (MaxThirsty <= 0.f) MaxThirsty = 100.f;

	// 에디터(블루프린트)에서 수정된 Max 값을 기준으로 Current 값을 꽉 채워줍니다.
	CurrentHP = MaxHP;
	CurrentSP = MaxSP;
	CurrentHunger = MaxHunger;
	CurrentThirsty = MaxThirsty;

    // 시작 시 UI 업데이트용 델리게이트 브로드캐스트
	BroadcastCurrentStats();

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

void UAI_REStatusComponent::RecoverHP(float Amount)
{
    CurrentHP = FMath::Clamp(CurrentHP + Amount, 0.f, MaxHP);
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
}

void UAI_REStatusComponent::RecoverSP(float Amount)
{
    CurrentSP = FMath::Clamp(CurrentSP + Amount, 0.f, MaxSP);
    OnSPChanged.Broadcast(CurrentSP, MaxSP);
}

void UAI_REStatusComponent::RecoverHunger(float Amount)
{
    CurrentHunger = FMath::Clamp(CurrentHunger + Amount, 0.f, MaxHunger);
    OnHungerChanged.Broadcast(CurrentHunger, MaxHunger);
}

void UAI_REStatusComponent::RecoverThirsty(float Amount)
{
    CurrentThirsty = FMath::Clamp(CurrentThirsty + Amount, 0.f, MaxThirsty);
    OnThirstyChanged.Broadcast(CurrentThirsty, MaxThirsty);
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

void UAI_REStatusComponent::AddGradualRecovery(float HP, float SP, float Hunger, float Thirsty, float Duration)
{
	if (Duration <= 0.f) return;

	// 0.5초마다 틱이 돈다고 가정
	float TickInterval = 0.5f;
	int32 TotalTicks = FMath::CeilToInt(Duration / TickInterval);

	if (TotalTicks > 0)
	{
		FGradualRecovery Recovery;
		Recovery.HPPerTick = HP / TotalTicks;
		Recovery.SPPerTick = SP / TotalTicks;
		Recovery.HungerPerTick = Hunger / TotalTicks;
		Recovery.ThirstyPerTick = Thirsty / TotalTicks;
		Recovery.TicksRemaining = TotalTicks;

		ActiveRecoveries.Add(Recovery);

		// 타이머가 돌고 있지 않다면 시작
		if (!GetWorld()->GetTimerManager().IsTimerActive(RecoveryTimerHandle))
		{
			GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this, &UAI_REStatusComponent::ProcessGradualRecovery, TickInterval, true);
		}
	}
}

void UAI_REStatusComponent::ProcessGradualRecovery()
{
	if (ActiveRecoveries.Num() == 0)
	{
		// 틱 배열이 비었으면 타이머 중지
		GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		return;
	}

	float TotalHP = 0.f;
	float TotalSP = 0.f;
	float TotalHunger = 0.f;
	float TotalThirsty = 0.f;

	for (int32 i = ActiveRecoveries.Num() - 1; i >= 0; --i)
	{
		FGradualRecovery& Rec = ActiveRecoveries[i];
		
		TotalHP += Rec.HPPerTick;
		TotalSP += Rec.SPPerTick;
		TotalHunger += Rec.HungerPerTick;
		TotalThirsty += Rec.ThirstyPerTick;

		Rec.TicksRemaining--;
		if (Rec.TicksRemaining <= 0)
		{
			ActiveRecoveries.RemoveAtSwap(i);
		}
	}

	// 일괄 적용 (기존 Recover 함수 재사용하여 이벤트 브로드캐스트)
	if (TotalHP > 0.f) RecoverHP(TotalHP);
	if (TotalSP > 0.f) RecoverSP(TotalSP);
	if (TotalHunger > 0.f) RecoverHunger(TotalHunger);
	if (TotalThirsty > 0.f) RecoverThirsty(TotalThirsty);

	if (ActiveRecoveries.Num() == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}
}

void UAI_REStatusComponent::BroadcastCurrentStats()
{
	OnHPChanged.Broadcast(CurrentHP, MaxHP);
	OnSPChanged.Broadcast(CurrentSP, MaxSP);
	OnHungerChanged.Broadcast(CurrentHunger, MaxHunger);
	OnThirstyChanged.Broadcast(CurrentThirsty, MaxThirsty);
}


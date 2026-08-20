#include "AI_REStatusComponent.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AI_REAttributeSet.h"

UAI_REStatusComponent::UAI_REStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAI_REStatusComponent::BeginPlay()
{
	Super::BeginPlay();

	// 생존 스탯(허기, 목마름) 자동 감소 타이머 실행 (2초 주기)
	GetWorld()->GetTimerManager().SetTimer(SurvivalTimerHandle, this, &UAI_REStatusComponent::HandleSurvivalStats, SurvivalTickRate, true);
}

void UAI_REStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAI_REStatusComponent::ConsumeSP(float Amount)
{
    // Deprecated: Use GAS (ApplyGameplayEffect) instead.
    UE_LOG(LogTemp, Warning, TEXT("UAI_REStatusComponent::ConsumeSP is deprecated. Please use GAS Gameplay Effects."));
}

void UAI_REStatusComponent::ApplyDamage(float Amount)
{
    // Deprecated: Use GAS (ApplyGameplayEffect) instead.
    UE_LOG(LogTemp, Warning, TEXT("UAI_REStatusComponent::ApplyDamage is deprecated. Please use GAS Gameplay Effects."));
}

void UAI_REStatusComponent::RecoverHP(float Amount)
{
    // Deprecated: Use GAS (ApplyGameplayEffect) instead.
}

void UAI_REStatusComponent::RecoverSP(float Amount)
{
    // Deprecated: Use GAS (ApplyGameplayEffect) instead.
}

void UAI_REStatusComponent::RecoverHunger(float Amount)
{
    // Deprecated: Use GAS (ApplyGameplayEffect) instead.
}

void UAI_REStatusComponent::RecoverThirsty(float Amount)
{
    // Deprecated: Use GAS (ApplyGameplayEffect) instead.
}

void UAI_REStatusComponent::HandleSurvivalStats()
{
	float Multiplier = IsOwnerRunning() ? RunMultiplier : 1.0f;

	if (AActor* Owner = GetOwner())
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				float HungerDeplete = -(BaseHungerDepleteRate * Multiplier);
				float ThirstyDeplete = -(BaseThirstyDepleteRate * Multiplier);

				ASC->ApplyModToAttributeUnsafe(UAI_REAttributeSet::GetHungerAttribute(), EGameplayModOp::Additive, HungerDeplete);
				ASC->ApplyModToAttributeUnsafe(UAI_REAttributeSet::GetThirstyAttribute(), EGameplayModOp::Additive, ThirstyDeplete);
			}
		}
	}
}

bool UAI_REStatusComponent::IsOwnerRunning() const
{
	if (AActor* Owner = GetOwner())
	{
		// 달리기 판정: 속도가 600.0f 이상이면 달리는 것으로 간주
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

	// 일괄 적용 (GAS를 통해)
	if (AActor* Owner = GetOwner())
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				if (TotalHP > 0.f) ASC->ApplyModToAttributeUnsafe(UAI_REAttributeSet::GetHPAttribute(), EGameplayModOp::Additive, TotalHP);
				if (TotalSP > 0.f) ASC->ApplyModToAttributeUnsafe(UAI_REAttributeSet::GetSPAttribute(), EGameplayModOp::Additive, TotalSP);
				if (TotalHunger > 0.f) ASC->ApplyModToAttributeUnsafe(UAI_REAttributeSet::GetHungerAttribute(), EGameplayModOp::Additive, TotalHunger);
				if (TotalThirsty > 0.f) ASC->ApplyModToAttributeUnsafe(UAI_REAttributeSet::GetThirstyAttribute(), EGameplayModOp::Additive, TotalThirsty);
			}
		}
	}

	if (ActiveRecoveries.Num() == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}
}

void UAI_REStatusComponent::BroadcastCurrentStats()
{
	// Deprecated: UI directly binds to GAS.
}

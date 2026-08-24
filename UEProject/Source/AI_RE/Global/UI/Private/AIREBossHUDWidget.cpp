#include "AIREBossHUDWidget.h"

#include "AIREBossEnemy.h"
#include "AIREEnemyReactionComponent.h"
#include "AIREEnemyVitalityComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

namespace
{
	float CalculatePercent(const float CurrentValue, const float MaxValue)
	{
		return MaxValue > 0.0f
			? FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f)
			: 0.0f;
	}
}

void UAIREBossHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetHUDVisibility(ESlateVisibility::Collapsed);
	if (IsValid(HealthProgressBar))
	{
		HealthProgressBar->SetPercent(0.0f);
	}
	if (IsValid(GroggyProgressBar))
	{
		GroggyProgressBar->SetPercent(0.0f);
	}

	if (BoundBoss.IsValid())
	{
		RefreshHUD();
	}
}

void UAIREBossHUDWidget::NativeDestruct()
{
	UnbindBoss();
	Super::NativeDestruct();
}

void UAIREBossHUDWidget::BindBoss(AAIREBossEnemy* InBoss)
{
	if (!IsValid(InBoss) || !InBoss->IsCombatTargetAlive())
	{
		UnbindBoss();
		return;
	}

	if (BoundBoss.Get() == InBoss)
	{
		RefreshHUD();
		return;
	}

	UnbindBoss();
	BoundBoss = InBoss;
	InBoss->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIREBossHUDWidget::HandleBossDestroyed);

	if (UAIREEnemyVitalityComponent* Vitality =
		InBoss->GetEnemyVitalityComponent())
	{
		Vitality->OnHealthChanged.AddUniqueDynamic(
			this,
			&UAIREBossHUDWidget::HandleHealthChanged);
	}
	if (UAIREEnemyReactionComponent* Reaction =
		InBoss->GetEnemyReactionComponent())
	{
		Reaction->OnGroggyChanged.AddUniqueDynamic(
			this,
			&UAIREBossHUDWidget::HandleGroggyChanged);
	}

	RefreshHUD();
}

void UAIREBossHUDWidget::UnbindBoss()
{
	if (AAIREBossEnemy* Boss = BoundBoss.Get())
	{
		Boss->OnDestroyed.RemoveDynamic(
			this,
			&UAIREBossHUDWidget::HandleBossDestroyed);
		if (UAIREEnemyVitalityComponent* Vitality =
			Boss->GetEnemyVitalityComponent())
		{
			Vitality->OnHealthChanged.RemoveDynamic(
				this,
				&UAIREBossHUDWidget::HandleHealthChanged);
		}
		if (UAIREEnemyReactionComponent* Reaction =
			Boss->GetEnemyReactionComponent())
		{
			Reaction->OnGroggyChanged.RemoveDynamic(
				this,
				&UAIREBossHUDWidget::HandleGroggyChanged);
		}
	}

	BoundBoss.Reset();
	SetHUDVisibility(ESlateVisibility::Collapsed);
	if (IsValid(HealthProgressBar))
	{
		HealthProgressBar->SetPercent(0.0f);
	}
	if (IsValid(GroggyProgressBar))
	{
		GroggyProgressBar->SetPercent(0.0f);
	}
}

void UAIREBossHUDWidget::RefreshHUD()
{
	AAIREBossEnemy* Boss = BoundBoss.Get();
	if (!IsValid(Boss))
	{
		SetHUDVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (IsValid(BossNameText))
	{
		BossNameText->SetText(Boss->GetBossDisplayName());
	}

	const UAIREEnemyVitalityComponent* Vitality =
		Boss->GetEnemyVitalityComponent();
	const FAIREEnemyVitalitySnapshot VitalitySnapshot = IsValid(Vitality)
		? Vitality->GetVitalitySnapshot()
		: FAIREEnemyVitalitySnapshot();
	if (IsValid(HealthProgressBar))
	{
		HealthProgressBar->SetPercent(
			CalculatePercent(
				VitalitySnapshot.Health,
				VitalitySnapshot.MaxHealth));
	}

	const UAIREEnemyReactionComponent* Reaction =
		Boss->GetEnemyReactionComponent();
	const FAIREEnemyReactionSnapshot ReactionSnapshot = IsValid(Reaction)
		? Reaction->GetReactionSnapshot()
		: FAIREEnemyReactionSnapshot();
	if (IsValid(GroggyProgressBar))
	{
		GroggyProgressBar->SetPercent(
			1.0f - CalculatePercent(
				ReactionSnapshot.StunGauge,
				ReactionSnapshot.StunThreshold));
	}

	SetHUDVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UAIREBossHUDWidget::HandleHealthChanged(
	const float OldHealth,
	const float NewHealth)
{
	(void)OldHealth;
	(void)NewHealth;
	RefreshHUD();
}

void UAIREBossHUDWidget::HandleGroggyChanged(
	const float CurrentGroggy,
	const float MaxGroggy)
{
	(void)CurrentGroggy;
	(void)MaxGroggy;
	RefreshHUD();
}

void UAIREBossHUDWidget::HandleBossDestroyed(AActor* DestroyedActor)
{
	if (!BoundBoss.IsValid() || DestroyedActor == BoundBoss.Get())
	{
		UnbindBoss();
	}
}

void UAIREBossHUDWidget::SetHUDVisibility(
	const ESlateVisibility InVisibility)
{
	if (IsValid(BossHUDRoot))
	{
		BossHUDRoot->SetVisibility(InVisibility);
	}
}

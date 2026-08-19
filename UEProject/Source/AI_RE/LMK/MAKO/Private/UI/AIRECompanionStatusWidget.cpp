#include "UI/AIRECompanionStatusWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "Core/AIRECompanionCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace
{
	constexpr float DistanceRefreshInterval = 0.25f;
	constexpr float UnrealUnitsPerMeter = 100.0f;

	float CalculateAttributePercent(
		const float CurrentValue,
		const float MaxValue)
	{
		return MaxValue > 0.0f
			? FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f)
			: 0.0f;
	}
}

void UAIRECompanionStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetStatusCardVisibility(ESlateVisibility::Collapsed);
	if (IsValid(HealthProgressBar))
	{
		HealthProgressBar->SetPercent(0.0f);
	}
	if (IsValid(StaminaProgressBar))
	{
		StaminaProgressBar->SetPercent(0.0f);
	}
	if (IsValid(DistanceText))
	{
		DistanceText->SetText(FText::FromString(TEXT("--m")));
	}

	if (IsValid(BoundCompanion.Get()))
	{
		if (IsValid(CompanionNameText))
		{
			CompanionNameText->SetText(
				FText::FromString(
					BoundCompanion.Get()->GetCompanionId()));
		}
		SetStatusCardVisibility(ESlateVisibility::SelfHitTestInvisible);
		RefreshStatus();
		RefreshDistance();
		StartDistanceRefreshTimer();
	}
}

void UAIRECompanionStatusWidget::NativeDestruct()
{
	UnbindCompanion();
	Super::NativeDestruct();
}

void UAIRECompanionStatusWidget::BindCompanion(
	AAIRECompanionCharacter* InCompanion)
{
	if (BoundCompanion.Get() == InCompanion
		&& IsValid(InCompanion))
	{
		SetStatusCardVisibility(ESlateVisibility::SelfHitTestInvisible);
		RefreshStatus();
		RefreshDistance();
		StartDistanceRefreshTimer();
		return;
	}

	UnbindCompanion();
	if (!IsValid(InCompanion))
	{
		return;
	}

	BoundCompanion = InCompanion;
	BoundAbilitySystemComponent =
		InCompanion->GetAbilitySystemComponent();
	InCompanion->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIRECompanionStatusWidget::HandleCompanionDestroyed);

	if (UAbilitySystemComponent* AbilitySystemComponent =
			BoundAbilitySystemComponent.Get())
	{
		HealthChangedDelegateHandle = AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UAIRECompanionAttributeSet::GetHealthAttribute())
			.AddUObject(
				this,
				&UAIRECompanionStatusWidget::HandleAttributeChanged);
		MaxHealthChangedDelegateHandle = AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UAIRECompanionAttributeSet::GetMaxHealthAttribute())
			.AddUObject(
				this,
				&UAIRECompanionStatusWidget::HandleAttributeChanged);
		StaminaChangedDelegateHandle = AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UAIRECompanionAttributeSet::GetStaminaAttribute())
			.AddUObject(
				this,
				&UAIRECompanionStatusWidget::HandleAttributeChanged);
		MaxStaminaChangedDelegateHandle = AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UAIRECompanionAttributeSet::GetMaxStaminaAttribute())
			.AddUObject(
				this,
				&UAIRECompanionStatusWidget::HandleAttributeChanged);
	}

	if (IsValid(CompanionNameText))
	{
		CompanionNameText->SetText(
			FText::FromString(InCompanion->GetCompanionId()));
	}
	SetStatusCardVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshStatus();
	RefreshDistance();
	StartDistanceRefreshTimer();
}

void UAIRECompanionStatusWidget::UnbindCompanion()
{
	StopDistanceRefreshTimer();
	UnbindAttributeDelegates();

	if (AAIRECompanionCharacter* Companion = BoundCompanion.Get())
	{
		Companion->OnDestroyed.RemoveDynamic(
			this,
			&UAIRECompanionStatusWidget::HandleCompanionDestroyed);
	}

	BoundAbilitySystemComponent.Reset();
	BoundCompanion.Reset();
	SetStatusCardVisibility(ESlateVisibility::Collapsed);

	if (IsValid(HealthProgressBar))
	{
		HealthProgressBar->SetPercent(0.0f);
	}
	if (IsValid(StaminaProgressBar))
	{
		StaminaProgressBar->SetPercent(0.0f);
	}
	if (IsValid(DistanceText))
	{
		DistanceText->SetText(FText::FromString(TEXT("--m")));
	}
}

void UAIRECompanionStatusWidget::RefreshStatus()
{
	AAIRECompanionCharacter* Companion = BoundCompanion.Get();
	if (!IsValid(Companion))
	{
		SetStatusCardVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetStatusCardVisibility(ESlateVisibility::SelfHitTestInvisible);
	UAbilitySystemComponent* AbilitySystemComponent =
		BoundAbilitySystemComponent.Get();
	if (!IsValid(AbilitySystemComponent))
	{
		if (IsValid(HealthProgressBar))
		{
			HealthProgressBar->SetPercent(0.0f);
		}
		if (IsValid(StaminaProgressBar))
		{
			StaminaProgressBar->SetPercent(0.0f);
		}
		return;
	}

	const float Health = AbilitySystemComponent->GetNumericAttribute(
		UAIRECompanionAttributeSet::GetHealthAttribute());
	const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(
		UAIRECompanionAttributeSet::GetMaxHealthAttribute());
	const float Stamina = AbilitySystemComponent->GetNumericAttribute(
		UAIRECompanionAttributeSet::GetStaminaAttribute());
	const float MaxStamina = AbilitySystemComponent->GetNumericAttribute(
		UAIRECompanionAttributeSet::GetMaxStaminaAttribute());

	if (IsValid(HealthProgressBar))
	{
		HealthProgressBar->SetPercent(
			CalculateAttributePercent(Health, MaxHealth));
	}
	if (IsValid(StaminaProgressBar))
	{
		StaminaProgressBar->SetPercent(
			CalculateAttributePercent(Stamina, MaxStamina));
	}
}

void UAIRECompanionStatusWidget::RefreshDistance()
{
	if (!IsValid(DistanceText))
	{
		return;
	}

	AAIRECompanionCharacter* Companion = BoundCompanion.Get();
	if (!IsValid(Companion))
	{
		DistanceText->SetText(FText::FromString(TEXT("--m")));
		SetStatusCardVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const APawn* PlayerPawn = GetOwningPlayerPawn();
	if (!IsValid(PlayerPawn))
	{
		DistanceText->SetText(FText::FromString(TEXT("--m")));
		return;
	}

	const float DistanceMeters = FVector::Dist(
		PlayerPawn->GetActorLocation(),
		Companion->GetActorLocation())
		/ UnrealUnitsPerMeter;
	const int32 RoundedDistanceMeters = FMath::RoundToInt(DistanceMeters);
	DistanceText->SetText(
		FText::FromString(
			FString::Printf(TEXT("%dm"), RoundedDistanceMeters)));
}

void UAIRECompanionStatusWidget::HandleAttributeChanged(
	const FOnAttributeChangeData& ChangeData)
{
	(void)ChangeData;
	RefreshStatus();
}

void UAIRECompanionStatusWidget::HandleCompanionDestroyed(
	AActor* DestroyedActor)
{
	if (!BoundCompanion.IsValid()
		|| DestroyedActor == BoundCompanion.Get())
	{
		UnbindCompanion();
	}
}

void UAIRECompanionStatusWidget::UnbindAttributeDelegates()
{
	UAbilitySystemComponent* AbilitySystemComponent =
		BoundAbilitySystemComponent.Get();
	if (!IsValid(AbilitySystemComponent))
	{
		HealthChangedDelegateHandle.Reset();
		MaxHealthChangedDelegateHandle.Reset();
		StaminaChangedDelegateHandle.Reset();
		MaxStaminaChangedDelegateHandle.Reset();
		return;
	}

	if (HealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UAIRECompanionAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}
	if (MaxHealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UAIRECompanionAttributeSet::GetMaxHealthAttribute())
			.Remove(MaxHealthChangedDelegateHandle);
		MaxHealthChangedDelegateHandle.Reset();
	}
	if (StaminaChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UAIRECompanionAttributeSet::GetStaminaAttribute())
			.Remove(StaminaChangedDelegateHandle);
		StaminaChangedDelegateHandle.Reset();
	}
	if (MaxStaminaChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UAIRECompanionAttributeSet::GetMaxStaminaAttribute())
			.Remove(MaxStaminaChangedDelegateHandle);
		MaxStaminaChangedDelegateHandle.Reset();
	}
}

void UAIRECompanionStatusWidget::StartDistanceRefreshTimer()
{
	StopDistanceRefreshTimer();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		DistanceRefreshTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&UAIRECompanionStatusWidget::RefreshDistance),
		DistanceRefreshInterval,
		true);
}

void UAIRECompanionStatusWidget::StopDistanceRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DistanceRefreshTimerHandle);
	}
	DistanceRefreshTimerHandle.Invalidate();
}

void UAIRECompanionStatusWidget::SetStatusCardVisibility(
	const ESlateVisibility InVisibility)
{
	if (IsValid(StatusCardRoot))
	{
		StatusCardRoot->SetVisibility(InVisibility);
	}
}

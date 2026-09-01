#include "AIRETargetLockMarkerWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"

void UAIRETargetLockMarkerWidget::SetLockedTarget(AActor* TargetActor)
{
	LockedTarget = TargetActor;
	SetVisibility(IsValid(TargetActor)
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	SetRenderOpacity(IsValid(TargetActor) ? 1.0f : 0.0f);

	if (IsValid(TargetActor))
	{
		UpdateMarkerPosition();
	}
}

void UAIRETargetLockMarkerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	SetVisibility(ESlateVisibility::Collapsed);
}

void UAIRETargetLockMarkerWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateMarkerPosition();
}

void UAIRETargetLockMarkerWidget::UpdateMarkerPosition()
{
	AActor* TargetActor = LockedTarget.Get();
	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(TargetActor) || !IsValid(PlayerController))
	{
		LockedTarget.Reset();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FBox TargetBounds = TargetActor->GetComponentsBoundingBox(true);
	const FVector MarkerLocation = TargetBounds.IsValid
		? TargetBounds.GetCenter()
		: TargetActor->GetActorLocation();

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		MarkerLocation,
		ScreenPosition,
		true))
	{
		SetRenderOpacity(0.0f);
		return;
	}

	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	if (ScreenPosition.X < 0.0f
		|| ScreenPosition.Y < 0.0f
		|| ScreenPosition.X > ViewportSize.X
		|| ScreenPosition.Y > ViewportSize.Y)
	{
		SetRenderOpacity(0.0f);
		return;
	}

	SetPositionInViewport(ScreenPosition, false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(1.0f);
}

#include "LocalAI/UI/AIRECompanionPolicyPanelWidget.h"

#include "Core/AIRECompanionCharacter.h"
#include "Policy/AIRECompanionLocalBehaviorPolicyComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "AIRECompanionPolicyPanel"

namespace
{
	FText GetEngagementPolicyText(
		const EAIRECompanionEngagementPolicy EngagementPolicy)
	{
		switch (EngagementPolicy)
		{
		case EAIRECompanionEngagementPolicy::HoldFire:
			return LOCTEXT("HoldFirePolicy", "전투 금지");
		case EAIRECompanionEngagementPolicy::DefendPlayer:
			return LOCTEXT("DefendPlayerPolicy", "내 주변 전투");
		case EAIRECompanionEngagementPolicy::Aggressive:
			return LOCTEXT("AggressivePolicy", "적극적 전투");
		default:
			return LOCTEXT("UnknownEngagementPolicy", "알 수 없는 교전 정책");
		}
	}

	FText GetRolePreferenceText(
		const EAIRECompanionRolePreference RolePreference)
	{
		switch (RolePreference)
		{
		case EAIRECompanionRolePreference::Balanced:
			return LOCTEXT("BalancedRole", "균형");
		case EAIRECompanionRolePreference::SupportPriority:
			return LOCTEXT("SupportPriorityRole", "회복 우선");
		default:
			return LOCTEXT("UnknownRolePreference", "알 수 없는 역할 선호");
		}
	}

	FText GetWheelSelectionText(
		const EAIRECompanionPolicyWheelSelection Selection)
	{
		switch (Selection)
		{
		case EAIRECompanionPolicyWheelSelection::Balanced:
			return LOCTEXT("BalancedSelection", "균형");
		case EAIRECompanionPolicyWheelSelection::SupportPriority:
			return LOCTEXT("SupportPrioritySelection", "지원 우선");
		case EAIRECompanionPolicyWheelSelection::HoldFire:
			return LOCTEXT("HoldFireSelection", "전투 금지");
		case EAIRECompanionPolicyWheelSelection::DefendPlayer:
			return LOCTEXT("DefendPlayerSelection", "호위");
		case EAIRECompanionPolicyWheelSelection::Aggressive:
			return LOCTEXT("AggressiveSelection", "적극 교전");
		default:
			return FText::GetEmpty();
		}
	}

	EAIRECompanionPolicyWheelSelection ResolveWheelSelection(
		const FVector2D& LocalCursorPosition,
		const FVector2D& LocalSize)
	{
		const float OuterRadius = FMath::Min(
			340.0f,
			static_cast<float>(FMath::Min(LocalSize.X, LocalSize.Y)) * 0.34f);
		const float DeadZoneRadius = FMath::Max(96.0f, OuterRadius * 0.38f);
		const FVector2D Delta = LocalCursorPosition - (LocalSize * 0.5);
		if (Delta.SquaredLength() < FMath::Square(DeadZoneRadius))
		{
			return EAIRECompanionPolicyWheelSelection::None;
		}

		float AngleDegrees = FMath::RadiansToDegrees(
			FMath::Atan2(
				static_cast<float>(Delta.Y),
				static_cast<float>(Delta.X)));
		if (AngleDegrees < 0.0f)
		{
			AngleDegrees += 360.0f;
		}

		if (AngleDegrees >= 270.0f)
		{
			return EAIRECompanionPolicyWheelSelection::SupportPriority;
		}
		if (AngleDegrees >= 180.0f)
		{
			return EAIRECompanionPolicyWheelSelection::Balanced;
		}
		if (AngleDegrees >= 120.0f)
		{
			return EAIRECompanionPolicyWheelSelection::HoldFire;
		}
		if (AngleDegrees >= 60.0f)
		{
			return EAIRECompanionPolicyWheelSelection::DefendPlayer;
		}
		return EAIRECompanionPolicyWheelSelection::Aggressive;
	}

	void DrawWheelArc(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& Geometry,
		const int32 LayerId,
		const FVector2D& Center,
		const float Radius,
		const float StartDegrees,
		const float EndDegrees,
		const FLinearColor& Color,
		const float Thickness)
	{
		constexpr int32 SegmentCount = 24;
		TArray<FVector2f> Points;
		Points.Reserve(SegmentCount + 1);
		for (int32 SegmentIndex = 0;
			SegmentIndex <= SegmentCount;
			++SegmentIndex)
		{
			const float Alpha =
				static_cast<float>(SegmentIndex) / SegmentCount;
			const float AngleRadians = FMath::DegreesToRadians(
				FMath::Lerp(StartDegrees, EndDegrees, Alpha));
			const FVector2D Point = Center + FVector2D(
				FMath::Cos(AngleRadians) * Radius,
				FMath::Sin(AngleRadians) * Radius);
			Points.Add(FVector2f(
				static_cast<float>(Point.X),
				static_cast<float>(Point.Y)));
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	}
}

void UAIRECompanionPolicyPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(BalancedButton))
	{
		BalancedButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleBalancedClicked);
	}
	if (IsValid(HoldFireButton))
	{
		HoldFireButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleHoldFireClicked);
	}
	if (IsValid(DefendPlayerButton))
	{
		DefendPlayerButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleDefendPlayerClicked);
	}
	if (IsValid(AggressiveButton))
	{
		AggressiveButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleAggressiveClicked);
	}
	if (IsValid(SupportPriorityButton))
	{
		SupportPriorityButton->OnClicked.AddUniqueDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleSupportPriorityClicked);
	}

	SetPanelOpen(false);
}

void UAIRECompanionPolicyPanelWidget::NativeDestruct()
{
	if (IsValid(BalancedButton))
	{
		BalancedButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleBalancedClicked);
	}
	if (IsValid(HoldFireButton))
	{
		HoldFireButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleHoldFireClicked);
	}
	if (IsValid(DefendPlayerButton))
	{
		DefendPlayerButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleDefendPlayerClicked);
	}
	if (IsValid(AggressiveButton))
	{
		AggressiveButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleAggressiveClicked);
	}
	if (IsValid(SupportPriorityButton))
	{
		SupportPriorityButton->OnClicked.RemoveDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleSupportPriorityClicked);
	}

	UnbindCompanion();
	Super::NativeDestruct();
}

void UAIRECompanionPolicyPanelWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bPanelOpen)
	{
		UpdateWheelSelection(MyGeometry);
	}
}

int32 UAIRECompanionPolicyPanelWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	if (!bPanelOpen)
	{
		return Super::NativePaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId,
			InWidgetStyle,
			bParentEnabled);
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D Center = LocalSize * 0.5;
	const float OuterRadius = FMath::Min(
		340.0f,
		static_cast<float>(FMath::Min(LocalSize.X, LocalSize.Y)) * 0.34f);
	const float InnerRadius = OuterRadius * 0.49f;
	const float ArcRadius = (OuterRadius + InnerRadius) * 0.5f;
	const float ArcThickness = OuterRadius - InnerRadius - 8.0f;
	const FLinearColor BaseColor(0.015f, 0.055f, 0.085f, 0.96f);
	const FLinearColor ActiveColor(0.01f, 0.31f, 0.43f, 0.92f);
	const FLinearColor HoveredColor(0.01f, 0.72f, 0.92f, 0.98f);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None,
		FLinearColor(0.004f, 0.012f, 0.024f, 0.68f));

	FAIRECompanionLocalBehaviorPolicy CurrentPolicy;
	const bool bHasCurrentPolicy = BoundPolicyComponent.IsValid();
	if (bHasCurrentPolicy)
	{
		CurrentPolicy = BoundPolicyComponent->GetLocalBehaviorPolicy();
	}

	const auto IsActiveSelection =
		[bHasCurrentPolicy, &CurrentPolicy](
			const EAIRECompanionPolicyWheelSelection Selection)
		{
			if (!bHasCurrentPolicy)
			{
				return false;
			}

			switch (Selection)
			{
			case EAIRECompanionPolicyWheelSelection::Balanced:
				return CurrentPolicy.RolePreference
					== EAIRECompanionRolePreference::Balanced;
			case EAIRECompanionPolicyWheelSelection::SupportPriority:
				return CurrentPolicy.RolePreference
					== EAIRECompanionRolePreference::SupportPriority;
			case EAIRECompanionPolicyWheelSelection::HoldFire:
				return CurrentPolicy.EngagementPolicy
					== EAIRECompanionEngagementPolicy::HoldFire;
			case EAIRECompanionPolicyWheelSelection::DefendPlayer:
				return CurrentPolicy.EngagementPolicy
					== EAIRECompanionEngagementPolicy::DefendPlayer;
			case EAIRECompanionPolicyWheelSelection::Aggressive:
				return CurrentPolicy.EngagementPolicy
					== EAIRECompanionEngagementPolicy::Aggressive;
			default:
				return false;
			}
		};

	const auto DrawSelection =
		[&](
			const EAIRECompanionPolicyWheelSelection Selection,
			const float StartDegrees,
			const float EndDegrees)
		{
			const FLinearColor Color =
				WheelSelection == Selection
					? HoveredColor
					: IsActiveSelection(Selection)
						? ActiveColor
						: BaseColor;
			DrawWheelArc(
				OutDrawElements,
				AllottedGeometry,
				LayerId + 1,
				Center,
				ArcRadius,
				StartDegrees,
				EndDegrees,
				Color,
				ArcThickness);
		};

	DrawSelection(EAIRECompanionPolicyWheelSelection::Aggressive, 2.0f, 58.0f);
	DrawSelection(EAIRECompanionPolicyWheelSelection::DefendPlayer, 62.0f, 118.0f);
	DrawSelection(EAIRECompanionPolicyWheelSelection::HoldFire, 122.0f, 178.0f);
	DrawSelection(EAIRECompanionPolicyWheelSelection::Balanced, 182.0f, 268.0f);
	DrawSelection(EAIRECompanionPolicyWheelSelection::SupportPriority, 272.0f, 358.0f);

	return Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId + 2,
		InWidgetStyle,
		bParentEnabled);
}

void UAIRECompanionPolicyPanelWidget::BeginPolicySelection()
{
	WheelSelection = EAIRECompanionPolicyWheelSelection::None;
	SetPanelOpen(true);
}

bool UAIRECompanionPolicyPanelWidget::CommitPolicySelection()
{
	if (!bPanelOpen)
	{
		return false;
	}

	UpdateWheelSelection(GetCachedGeometry());
	const EAIRECompanionPolicyWheelSelection Selection = WheelSelection;
	WheelSelection = EAIRECompanionPolicyWheelSelection::None;
	if (Selection == EAIRECompanionPolicyWheelSelection::None)
	{
		RefreshPolicyDisplay();
		return false;
	}

	return ApplySelection(Selection);
}

void UAIRECompanionPolicyPanelWidget::SetPanelOpen(const bool bOpen)
{
	bPanelOpen = bOpen;
	if (!bPanelOpen)
	{
		WheelSelection = EAIRECompanionPolicyWheelSelection::None;
	}
	if (IsValid(PolicyPanel))
	{
		PolicyPanel->SetVisibility(
			bPanelOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (IsValid(CollapsedHint))
	{
		CollapsedHint->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (bPanelOpen && !BoundCompanion.IsValid())
	{
		RefreshCompanionBinding();
	}
	InvalidateLayoutAndVolatility();
}

bool UAIRECompanionPolicyPanelWidget::IsPanelOpen() const
{
	return bPanelOpen;
}

void UAIRECompanionPolicyPanelWidget::RefreshCompanionBinding()
{
	AAIRECompanionCharacter* FoundCompanion = nullptr;
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		for (TActorIterator<AAIRECompanionCharacter> It(World); It; ++It)
		{
			if (AAIRECompanionCharacter* Companion = *It;
				IsValid(Companion)
				&& Companion->GetCompanionId() == TEXT("MAKO"))
			{
				FoundCompanion = Companion;
				break;
			}
		}
	}

	if (BoundCompanion.Get() != FoundCompanion
		|| !BoundPolicyComponent.IsValid())
	{
		UnbindCompanion();
		BindCompanion(FoundCompanion);
	}
	RefreshPolicyDisplay();
}

void UAIRECompanionPolicyPanelWidget::HandleBalancedClicked()
{
	ApplySelection(EAIRECompanionPolicyWheelSelection::Balanced);
}

void UAIRECompanionPolicyPanelWidget::HandleHoldFireClicked()
{
	ApplySelection(EAIRECompanionPolicyWheelSelection::HoldFire);
}

void UAIRECompanionPolicyPanelWidget::HandleDefendPlayerClicked()
{
	ApplySelection(EAIRECompanionPolicyWheelSelection::DefendPlayer);
}

void UAIRECompanionPolicyPanelWidget::HandleAggressiveClicked()
{
	ApplySelection(EAIRECompanionPolicyWheelSelection::Aggressive);
}

void UAIRECompanionPolicyPanelWidget::HandleSupportPriorityClicked()
{
	ApplySelection(EAIRECompanionPolicyWheelSelection::SupportPriority);
}

void UAIRECompanionPolicyPanelWidget::HandlePolicyChanged(
	const FAIRECompanionLocalBehaviorPolicy PreviousPolicy,
	const FAIRECompanionLocalBehaviorPolicy CurrentPolicy)
{
	(void)PreviousPolicy;
	(void)CurrentPolicy;
	RefreshPolicyDisplay();
}

void UAIRECompanionPolicyPanelWidget::HandleCompanionDestroyed(
	AActor* DestroyedActor)
{
	(void)DestroyedActor;
	UnbindCompanion();
	RefreshPolicyDisplay();
}

bool UAIRECompanionPolicyPanelWidget::ApplySelection(
	const EAIRECompanionPolicyWheelSelection Selection)
{
	if (!BoundPolicyComponent.IsValid())
	{
		RefreshCompanionBinding();
	}
	if (!BoundPolicyComponent.IsValid())
	{
		if (IsValid(StatusText))
		{
			StatusText->SetText(
				LOCTEXT("CompanionUnavailable", "MAKO를 찾을 수 없습니다."));
		}
		return false;
	}

	FAIRECompanionLocalBehaviorPolicy NewPolicy =
		BoundPolicyComponent->GetLocalBehaviorPolicy();
	switch (Selection)
	{
	case EAIRECompanionPolicyWheelSelection::Balanced:
		NewPolicy.RolePreference = EAIRECompanionRolePreference::Balanced;
		break;
	case EAIRECompanionPolicyWheelSelection::SupportPriority:
		NewPolicy.RolePreference =
			EAIRECompanionRolePreference::SupportPriority;
		break;
	case EAIRECompanionPolicyWheelSelection::HoldFire:
		NewPolicy.EngagementPolicy =
			EAIRECompanionEngagementPolicy::HoldFire;
		break;
	case EAIRECompanionPolicyWheelSelection::DefendPlayer:
		NewPolicy.EngagementPolicy =
			EAIRECompanionEngagementPolicy::DefendPlayer;
		break;
	case EAIRECompanionPolicyWheelSelection::Aggressive:
		NewPolicy.EngagementPolicy =
			EAIRECompanionEngagementPolicy::Aggressive;
		break;
	default:
		return false;
	}

	const bool bApplied =
		BoundPolicyComponent->SetLocalBehaviorPolicy(NewPolicy);
	RefreshPolicyDisplay();
	if (IsValid(StatusText))
	{
		StatusText->SetText(
			bApplied
				? FText::Format(
					LOCTEXT("PolicyApplied", "{0} 적용 완료"),
					GetWheelSelectionText(Selection))
				: LOCTEXT("PolicyRejected", "정책 적용이 거부되었습니다."));
	}
	InvalidateLayoutAndVolatility();
	return bApplied;
}

void UAIRECompanionPolicyPanelWidget::UpdateWheelSelection(
	const FGeometry& Geometry)
{
	if (!bPanelOpen || !FSlateApplication::IsInitialized())
	{
		return;
	}

	const FVector2D LocalCursorPosition = Geometry.AbsoluteToLocal(
		FSlateApplication::Get().GetCursorPos());
	const EAIRECompanionPolicyWheelSelection NewSelection =
		ResolveWheelSelection(LocalCursorPosition, Geometry.GetLocalSize());
	if (WheelSelection == NewSelection)
	{
		return;
	}

	WheelSelection = NewSelection;
	RefreshPolicyDisplay();
	InvalidateLayoutAndVolatility();
}

FAIRECompanionLocalBehaviorPolicy
UAIRECompanionPolicyPanelWidget::GetPreviewPolicy(
	const FAIRECompanionLocalBehaviorPolicy& CurrentPolicy) const
{
	FAIRECompanionLocalBehaviorPolicy PreviewPolicy = CurrentPolicy;
	switch (WheelSelection)
	{
	case EAIRECompanionPolicyWheelSelection::Balanced:
		PreviewPolicy.RolePreference = EAIRECompanionRolePreference::Balanced;
		break;
	case EAIRECompanionPolicyWheelSelection::SupportPriority:
		PreviewPolicy.RolePreference =
			EAIRECompanionRolePreference::SupportPriority;
		break;
	case EAIRECompanionPolicyWheelSelection::HoldFire:
		PreviewPolicy.EngagementPolicy =
			EAIRECompanionEngagementPolicy::HoldFire;
		break;
	case EAIRECompanionPolicyWheelSelection::DefendPlayer:
		PreviewPolicy.EngagementPolicy =
			EAIRECompanionEngagementPolicy::DefendPlayer;
		break;
	case EAIRECompanionPolicyWheelSelection::Aggressive:
		PreviewPolicy.EngagementPolicy =
			EAIRECompanionEngagementPolicy::Aggressive;
		break;
	default:
		break;
	}
	return PreviewPolicy;
}

void UAIRECompanionPolicyPanelWidget::BindCompanion(
	AAIRECompanionCharacter* Companion)
{
	if (!IsValid(Companion))
	{
		return;
	}

	UAIRECompanionLocalBehaviorPolicyComponent* PolicyComponent =
		Companion->GetLocalBehaviorPolicyComponent();
	if (!IsValid(PolicyComponent))
	{
		return;
	}

	BoundCompanion = Companion;
	BoundPolicyComponent = PolicyComponent;
	Companion->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIRECompanionPolicyPanelWidget::HandleCompanionDestroyed);
	PolicyComponent->OnLocalBehaviorPolicyChanged.AddUniqueDynamic(
		this,
		&UAIRECompanionPolicyPanelWidget::HandlePolicyChanged);
}

void UAIRECompanionPolicyPanelWidget::UnbindCompanion()
{
	if (BoundPolicyComponent.IsValid())
	{
		BoundPolicyComponent->OnLocalBehaviorPolicyChanged.RemoveDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandlePolicyChanged);
	}
	if (BoundCompanion.IsValid())
	{
		BoundCompanion->OnDestroyed.RemoveDynamic(
			this,
			&UAIRECompanionPolicyPanelWidget::HandleCompanionDestroyed);
	}
	BoundPolicyComponent.Reset();
	BoundCompanion.Reset();
}

void UAIRECompanionPolicyPanelWidget::RefreshPolicyDisplay()
{
	if (!BoundPolicyComponent.IsValid())
	{
		if (IsValid(CurrentPolicyText))
		{
			CurrentPolicyText->SetText(
				LOCTEXT("NoCurrentPolicy", "현재: MAKO 없음"));
		}
		if (IsValid(StatusText))
		{
			StatusText->SetText(
				LOCTEXT("WaitingForCompanion", "MAKO 생성을 기다리는 중입니다."));
		}
		SetPolicyButtonsEnabled(false);
		return;
	}

	const FAIRECompanionLocalBehaviorPolicy Policy =
		BoundPolicyComponent->GetLocalBehaviorPolicy();
	const bool bHasSelection =
		WheelSelection != EAIRECompanionPolicyWheelSelection::None;
	const FAIRECompanionLocalBehaviorPolicy DisplayPolicy =
		bHasSelection ? GetPreviewPolicy(Policy) : Policy;
	if (IsValid(CurrentPolicyText))
	{
		CurrentPolicyText->SetText(FText::Format(
			bHasSelection
				? LOCTEXT("PreviewPolicyFormat", "적용 예정: {0} · {1}")
				: LOCTEXT("CurrentPolicyFormat", "현재: {0} · {1}"),
			GetEngagementPolicyText(DisplayPolicy.EngagementPolicy),
			GetRolePreferenceText(DisplayPolicy.RolePreference)));
	}
	if (IsValid(StatusText))
	{
		StatusText->SetText(bHasSelection
			? FText::Format(
				LOCTEXT(
					"SelectionReady",
					"선택: {0} · P를 놓으면 적용"),
				GetWheelSelectionText(WheelSelection))
			: LOCTEXT(
				"PolicyReady",
				"방향을 선택하고 P를 놓으면 적용 · 중앙에서 놓으면 취소"));
	}
	SetPolicyButtonsEnabled(true);
}

void UAIRECompanionPolicyPanelWidget::SetPolicyButtonsEnabled(
	const bool bEnabled)
{
	if (IsValid(BalancedButton))
	{
		BalancedButton->SetIsEnabled(bEnabled);
	}
	if (IsValid(HoldFireButton))
	{
		HoldFireButton->SetIsEnabled(bEnabled);
	}
	if (IsValid(DefendPlayerButton))
	{
		DefendPlayerButton->SetIsEnabled(bEnabled);
	}
	if (IsValid(AggressiveButton))
	{
		AggressiveButton->SetIsEnabled(bEnabled);
	}
	if (IsValid(SupportPriorityButton))
	{
		SupportPriorityButton->SetIsEnabled(bEnabled);
	}
}

#undef LOCTEXT_NAMESPACE

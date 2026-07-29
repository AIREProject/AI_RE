#include "Chat/UI/AIREResponseStackWidget.h"

#include "Chat/UI/AIREResponseCardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftClassPath ResponseCardClassPath(
		TEXT("/Game/Work/LMK/UI/Chat/WBP_AIREResponseCard.WBP_AIREResponseCard_C"));
}

void UAIREResponseStackWidget::AddResponse(
	const FAIREChatLogEntry& Entry)
{
	UVerticalBox* ResponseList = WidgetTree
		? Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("ResponseList")))
		: nullptr;
	if (!IsValid(ResponseList))
	{
		return;
	}

	UClass* ResponseCardClass =
		ResponseCardClassPath.TryLoadClass<UAIREResponseCardWidget>();
	if (!IsValid(ResponseCardClass))
	{
		return;
	}

	if (ResponseCards.Contains(Entry.Sequence))
	{
		return;
	}

	UAIREResponseCardWidget* ResponseCard =
		CreateWidget<UAIREResponseCardWidget>(
			GetOwningPlayer(),
			ResponseCardClass);
	if (!IsValid(ResponseCard))
	{
		return;
	}

	ResponseCard->InitializeResponse(Entry, this);
	UVerticalBoxSlot* CardSlot =
		ResponseList->AddChildToVerticalBox(ResponseCard);
	if (IsValid(CardSlot))
	{
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}
	ResponseCards.Add(Entry.Sequence, ResponseCard);
	RefreshStackVisibility();
}

void UAIREResponseStackWidget::DismissResponse(
	const int64 ResponseId)
{
	TObjectPtr<UAIREResponseCardWidget>* ResponseCard =
		ResponseCards.Find(ResponseId);
	if (!ResponseCard || !IsValid(*ResponseCard))
	{
		HandleCardDismissed(ResponseId);
		return;
	}

	(*ResponseCard)->Dismiss();
}

void UAIREResponseStackWidget::ResetResponses()
{
	for (const TPair<int64, TObjectPtr<UAIREResponseCardWidget>>& Pair
		: ResponseCards)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->RemoveFromParent();
		}
	}
	ResponseCards.Reset();
	RefreshStackVisibility();
}

void UAIREResponseStackWidget::HandleCardDismissed(
	const int64 ResponseId)
{
	TObjectPtr<UAIREResponseCardWidget> ResponseCard;
	if (ResponseCards.RemoveAndCopyValue(ResponseId, ResponseCard)
		&& IsValid(ResponseCard))
	{
		ResponseCard->RemoveFromParent();
	}
	RefreshStackVisibility();
}

void UAIREResponseStackWidget::RefreshStackVisibility()
{
	SetVisibility(
		ResponseCards.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
}

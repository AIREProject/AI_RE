#include "Chat/UI/AIREResponseCardWidget.h"

#include "Chat/UI/AIREResponseStackWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void UAIREResponseCardWidget::InitializeResponse(
	const FAIREChatLogEntry& Entry,
	UAIREResponseStackWidget* InOwnerStack)
{
	ResponseId = Entry.Sequence;
	OwnerStack = InOwnerStack;
	bIsDismissing = false;

	UTextBlock* ResponseText = WidgetTree
		? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ResponseText")))
		: nullptr;
	UTextBlock* TimestampText = WidgetTree
		? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TimestampText")))
		: nullptr;
	if (IsValid(ResponseText))
	{
		ResponseText->SetText(FText::FromString(Entry.Text));
	}
	if (IsValid(TimestampText))
	{
		TimestampText->SetText(
			FText::FromString(
				Entry.Timestamp.ToString(TEXT("%H:%M:%S"))));
	}

	if (UWidgetAnimation* ResponseIn =
		FindAnimationByPrefix(TEXT("ResponseIn")))
	{
		PlayAnimation(ResponseIn);
	}
}

void UAIREResponseCardWidget::Dismiss()
{
	if (bIsDismissing)
	{
		return;
	}
	bIsDismissing = true;

	UWidgetAnimation* ResponseOut =
		FindAnimationByPrefix(TEXT("ResponseOut"));
	if (!IsValid(ResponseOut))
	{
		HandleDismissAnimationFinished();
		return;
	}

	FWidgetAnimationDynamicEvent FinishedEvent;
	FinishedEvent.BindDynamic(
		this,
		&UAIREResponseCardWidget::HandleDismissAnimationFinished);
	BindToAnimationFinished(ResponseOut, FinishedEvent);
	PlayAnimation(ResponseOut);
}

int64 UAIREResponseCardWidget::GetResponseId() const
{
	return ResponseId;
}

void UAIREResponseCardWidget::HandleDismissAnimationFinished()
{
	if (IsValid(OwnerStack))
	{
		OwnerStack->HandleCardDismissed(ResponseId);
	}
	else
	{
		RemoveFromParent();
	}
	OwnerStack = nullptr;
}

UWidgetAnimation* UAIREResponseCardWidget::FindAnimationByPrefix(
	const FString& Prefix) const
{
	const UWidgetBlueprintGeneratedClass* WidgetClass =
		Cast<UWidgetBlueprintGeneratedClass>(GetClass());
	if (!IsValid(WidgetClass))
	{
		return nullptr;
	}

	for (UWidgetAnimation* Animation : WidgetClass->Animations)
	{
		if (IsValid(Animation)
			&& Animation->GetName().StartsWith(Prefix))
		{
			return Animation;
		}
	}

	return nullptr;
}

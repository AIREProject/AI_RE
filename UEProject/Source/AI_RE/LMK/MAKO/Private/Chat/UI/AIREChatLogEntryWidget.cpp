#include "Chat/UI/AIREChatLogEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void UAIREChatLogEntryWidget::ApplyEntry(
	const FAIREChatLogEntry& Entry,
	const bool bCompactPlayerEntry)
{
	UTextBlock* SenderText = WidgetTree
		? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("LogSenderText")))
		: nullptr;
	UTextBlock* MessageText = WidgetTree
		? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("LogMessageText")))
		: nullptr;
	UTextBlock* TimestampText = WidgetTree
		? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("LogTimestampText")))
		: nullptr;

	const bool bIsPlayer =
		Entry.Author == EAIREChatMessageAuthor::Player;
	if (IsValid(SenderText))
	{
		SenderText->SetText(
			FText::FromString(bIsPlayer ? TEXT("나") : TEXT("AI")));
		SenderText->SetColorAndOpacity(
			bIsPlayer ? PlayerTextColor : CompanionTextColor);
		SenderText->SetVisibility(
			bCompactPlayerEntry
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible);
	}
	if (IsValid(MessageText))
	{
		MessageText->SetText(FText::FromString(Entry.Text));
		MessageText->SetColorAndOpacity(
			bIsPlayer ? PlayerTextColor : CompanionTextColor);
	}
	if (IsValid(TimestampText))
	{
		TimestampText->SetText(
			FText::FromString(
				Entry.Timestamp.ToString(TEXT("%H:%M:%S"))));
		TimestampText->SetVisibility(
			bCompactPlayerEntry
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible);
	}
}

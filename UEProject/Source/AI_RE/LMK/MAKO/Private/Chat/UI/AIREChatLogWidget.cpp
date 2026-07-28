#include "Chat/UI/AIREChatLogWidget.h"

#include "Chat/UI/AIREChatHUDWidget.h"
#include "Chat/UI/AIREChatLogEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftClassPath ChatLogEntryClassPath(
		TEXT("/Game/Work/LMK/UI/Chat/WBP_AIREChatLogEntry.WBP_AIREChatLogEntry_C"));
}

void UAIREChatLogWidget::InitializeChatLog(
	UAIREChatHUDWidget* InOwnerHUD)
{
	OwnerHUD = InOwnerHUD;
	ResolveRuntimeWidgets();
	SetIsFocusable(true);
	SetChatLogOpen(false);
}

void UAIREChatLogWidget::SetChatLogOpen(const bool bOpen)
{
	SetVisibility(
		bOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (IsValid(ChatLogRoot))
	{
		ChatLogRoot->SetVisibility(
			bOpen
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (bOpen)
	{
		SetKeyboardFocus();
	}
}

bool UAIREChatLogWidget::IsChatLogOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

void UAIREChatLogWidget::RefreshEntries(
	const TArray<FAIREChatLogEntry>& Entries)
{
	if (!IsValid(ChatLogList))
	{
		ResolveRuntimeWidgets();
	}
	if (!IsValid(ChatLogList))
	{
		return;
	}

	UClass* EntryClass =
		ChatLogEntryClassPath.TryLoadClass<UAIREChatLogEntryWidget>();
	if (!IsValid(EntryClass))
	{
		return;
	}

	ChatLogList->ClearChildren();
	for (const FAIREChatLogEntry& Entry : Entries)
	{
		UAIREChatLogEntryWidget* EntryWidget =
			CreateWidget<UAIREChatLogEntryWidget>(
				GetOwningPlayer(),
				EntryClass);
		if (!IsValid(EntryWidget))
		{
			continue;
		}

		EntryWidget->ApplyEntry(Entry, false);
		UVerticalBoxSlot* EntrySlot =
			ChatLogList->AddChildToVerticalBox(EntryWidget);
		if (IsValid(EntrySlot))
		{
			EntrySlot->SetHorizontalAlignment(
				Entry.Author == EAIREChatMessageAuthor::Player
					? HAlign_Right
					: HAlign_Left);
			EntrySlot->SetPadding(FMargin(0.0f, 4.0f));
		}
	}

	if (IsValid(ChatLogScroll))
	{
		ChatLogScroll->ScrollToEnd();
	}
}

UWidget* UAIREChatLogWidget::GetLogFocusTarget()
{
	return this;
}

FReply UAIREChatLogWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape && IsValid(OwnerHUD))
	{
		OwnerHUD->CloseChatLog();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UAIREChatLogWidget::NativeDestruct()
{
	OwnerHUD = nullptr;
	ChatLogRoot = nullptr;
	ChatLogScroll = nullptr;
	ChatLogList = nullptr;
	Super::NativeDestruct();
}

void UAIREChatLogWidget::ResolveRuntimeWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	ChatLogRoot = WidgetTree->FindWidget(TEXT("ChatLogRoot"));
	ChatLogScroll = Cast<UScrollBox>(
		WidgetTree->FindWidget(TEXT("ChatLogScroll")));
	ChatLogList = Cast<UVerticalBox>(
		WidgetTree->FindWidget(TEXT("ChatLogList")));
}

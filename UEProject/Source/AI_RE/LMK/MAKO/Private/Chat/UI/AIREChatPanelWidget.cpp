#include "Chat/UI/AIREChatPanelWidget.h"

#include "Chat/UI/AIREChatHUDWidget.h"
#include "Chat/UI/AIREChatLogEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftClassPath ChatLogEntryClassPath(
		TEXT("/Game/Work/LMK/UI/Chat/WBP_AIREChatLogEntry.WBP_AIREChatLogEntry_C"));
	constexpr int32 MaxCompactHistoryEntries = 20;
}

void UAIREChatPanelWidget::InitializeChatPanel(
	UAIREChatHUDWidget* InOwnerHUD)
{
	OwnerHUD = InOwnerHUD;
	ResolveRuntimeWidgets();
	SetChatInputOpen(false);
}

void UAIREChatPanelWidget::SetChatInputOpen(const bool bOpen)
{
	if (!IsValid(ChatPanelRoot))
	{
		ResolveRuntimeWidgets();
	}
	if (IsValid(ChatPanelRoot))
	{
		ChatPanelRoot->SetVisibility(
			bOpen
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (bOpen && IsValid(MessageInput))
	{
		MessageInput->SetKeyboardFocus();
	}
}

bool UAIREChatPanelWidget::IsChatInputOpen() const
{
	return IsValid(ChatPanelRoot)
		&& ChatPanelRoot->GetVisibility() != ESlateVisibility::Collapsed;
}

void UAIREChatPanelWidget::ClearMessageInput()
{
	if (IsValid(MessageInput))
	{
		MessageInput->SetText(FText::GetEmpty());
	}
}

void UAIREChatPanelWidget::RefreshPlayerHistory(
	const TArray<FAIREChatLogEntry>& Entries)
{
	if (!IsValid(ChatHistoryList))
	{
		ResolveRuntimeWidgets();
	}
	if (!IsValid(ChatHistoryList))
	{
		return;
	}

	UClass* EntryClass =
		ChatLogEntryClassPath.TryLoadClass<UAIREChatLogEntryWidget>();
	if (!IsValid(EntryClass))
	{
		return;
	}

	TArray<const FAIREChatLogEntry*> PlayerEntries;
	for (const FAIREChatLogEntry& Entry : Entries)
	{
		if (Entry.Author == EAIREChatMessageAuthor::Player)
		{
			PlayerEntries.Add(&Entry);
		}
	}

	const int32 FirstEntryIndex =
		FMath::Max(0, PlayerEntries.Num() - MaxCompactHistoryEntries);
	ChatHistoryList->ClearChildren();
	for (int32 Index = FirstEntryIndex; Index < PlayerEntries.Num(); ++Index)
	{
		UAIREChatLogEntryWidget* EntryWidget =
			CreateWidget<UAIREChatLogEntryWidget>(
				GetOwningPlayer(),
				EntryClass);
		if (!IsValid(EntryWidget))
		{
			continue;
		}

		EntryWidget->ApplyEntry(*PlayerEntries[Index], true);
		UVerticalBoxSlot* EntrySlot =
			ChatHistoryList->AddChildToVerticalBox(EntryWidget);
		if (IsValid(EntrySlot))
		{
			EntrySlot->SetPadding(FMargin(0.0f, 2.0f));
		}
	}

	if (IsValid(ChatHistoryScroll))
	{
		ChatHistoryScroll->ScrollToEnd();
	}
}

UWidget* UAIREChatPanelWidget::GetInputFocusTarget() const
{
	return MessageInput;
}

void UAIREChatPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveRuntimeWidgets();
	if (IsValid(MessageInput))
	{
		MessageInput->OnTextCommitted.AddUniqueDynamic(
			this,
			&UAIREChatPanelWidget::HandleMessageCommitted);
		MessageInput->SetHintText(
			FText::FromString(TEXT("메시지를 입력하세요")));
	}
}

void UAIREChatPanelWidget::NativeDestruct()
{
	if (IsValid(MessageInput))
	{
		MessageInput->OnTextCommitted.RemoveDynamic(
			this,
			&UAIREChatPanelWidget::HandleMessageCommitted);
	}
	OwnerHUD = nullptr;
	ChatPanelRoot = nullptr;
	MessageInput = nullptr;
	ChatHistoryScroll = nullptr;
	ChatHistoryList = nullptr;
	Super::NativeDestruct();
}

void UAIREChatPanelWidget::HandleMessageCommitted(
	const FText& Text,
	const ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter || !IsValid(OwnerHUD))
	{
		return;
	}

	OwnerHUD->HandlePlayerMessageCommitted(Text.ToString());
}

void UAIREChatPanelWidget::ResolveRuntimeWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	ChatPanelRoot = WidgetTree->FindWidget(TEXT("ChatPanelRoot"));
	MessageInput = Cast<UEditableTextBox>(
		WidgetTree->FindWidget(TEXT("MessageInput")));
	ChatHistoryScroll = Cast<UScrollBox>(
		WidgetTree->FindWidget(TEXT("ChatHistoryScroll")));
	ChatHistoryList = Cast<UVerticalBox>(
		WidgetTree->FindWidget(TEXT("ChatHistoryList")));
}

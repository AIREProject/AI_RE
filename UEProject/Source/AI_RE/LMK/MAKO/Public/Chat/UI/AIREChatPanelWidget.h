#pragma once

#include "CoreMinimal.h"
#include "Chat/UI/AIREChatHistorySubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "AIREChatPanelWidget.generated.h"

class UAIREChatHUDWidget;
class UEditableTextBox;
class UScrollBox;
class UVerticalBox;
class UWidget;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREChatPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeChatPanel(UAIREChatHUDWidget* InOwnerHUD);
	void SetChatInputOpen(bool bOpen);
	bool IsChatInputOpen() const;
	void ClearMessageInput();
	void RefreshPlayerHistory(const TArray<FAIREChatLogEntry>& Entries);
	UWidget* GetInputFocusTarget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleMessageCommitted(
		const FText& Text,
		ETextCommit::Type CommitMethod);

	void ResolveRuntimeWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatHUDWidget> OwnerHUD;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ChatPanelRoot;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> MessageInput;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ChatHistoryScroll;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChatHistoryList;
};

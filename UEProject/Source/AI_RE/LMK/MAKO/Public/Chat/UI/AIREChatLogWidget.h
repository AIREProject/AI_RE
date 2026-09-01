#pragma once

#include "CoreMinimal.h"
#include "Chat/UI/AIREChatHistorySubsystem.h"
#include "Blueprint/UserWidget.h"
#include "AIREChatLogWidget.generated.h"

class UAIREChatHUDWidget;
class UScrollBox;
class UVerticalBox;
class UWidget;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREChatLogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeChatLog(UAIREChatHUDWidget* InOwnerHUD);
	void SetChatLogOpen(bool bOpen);
	bool IsChatLogOpen() const;
	void RefreshEntries(const TArray<FAIREChatLogEntry>& Entries);
	UWidget* GetLogFocusTarget();

protected:
	virtual void NativeDestruct() override;

private:
	void ResolveRuntimeWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatHUDWidget> OwnerHUD;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ChatLogRoot;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ChatLogScroll;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChatLogList;
};

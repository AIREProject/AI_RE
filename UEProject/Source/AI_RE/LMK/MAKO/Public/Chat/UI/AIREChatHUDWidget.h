#pragma once

#include "CoreMinimal.h"
#include "Chat/Contracts/AIREChatTypes.h"
#include "Chat/UI/AIREChatHistorySubsystem.h"
#include "Blueprint/UserWidget.h"
#include "AIREChatHUDWidget.generated.h"

class UAIRECompanionChatComponent;
class UAIREChatLogWidget;
class UAIREChatPanelWidget;
class UAIREResponseStackWidget;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FAIREChatUIStateChanged);

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREChatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	void InitializeChatRuntime(UAIRECompanionChatComponent* InChatComponent);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	bool SubmitPlayerMessage(const FString& UserMessage);

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Chat")
	TArray<FString> GetVisibleResponseTexts() const;

	/** Displays a trusted local MAKO line without creating a server request. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	void ShowLocalCompanionMessage(const FString& Message);
	void HandlePlayerMessageCommitted(const FString& String);
	void CloseChatLog();

	void InitializeChatLogWidget(UAIREChatLogWidget* InChatLogWidget);
	void HandleGlobalEnterInput();
	void HandleGlobalLogInput();
	void CloseAllChatUI();
	bool IsChatInputOpen() const;
	bool IsChatLogOpen() const;
	UWidget* GetChatInputFocusTarget() const;
	UWidget* GetChatLogFocusTarget();

	FAIREChatUIStateChanged OnChatUIStateChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Chat", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxVisibleResponses = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Chat", meta = (ClampMin = "0.1"))
	float ResponseDisplaySeconds = 8.0f;

private:
	struct FVisibleResponse
	{
		int64 Id = 0;
		FAIREChatLogEntry Entry;
		FTimerHandle ExpirationHandle;
	};

	UFUNCTION()
	void HandleRuntimeChatResponse(const FAIREChatResult& Result);
	void AddCompanionResponse(const FString& DisplayText);

	UFUNCTION()
	void HandleRuntimeChatFailure(const FAIREChatError& Error);

	void RemoveResponse(int64 ResponseId);
	void RemoveOldestResponse();
	void RefreshVisibleResponseTexts();
	void RefreshChatHistoryViews();
	void OpenChatInput();
	void CloseChatInput();
	void OpenChatLog();
	void NotifyChatUIStateChanged();
	UAIREChatHistorySubsystem* GetChatHistory() const;
	void UnbindChatComponent();
	void ClearResponseTimers();

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionChatComponent> RuntimeChatComponent;

	UPROPERTY(Transient)
	TArray<FString> VisibleResponseTexts;

	UPROPERTY(Transient)
	TObjectPtr<UAIREResponseStackWidget> RuntimeResponseStack;

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatPanelWidget> RuntimeChatPanel;

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatLogWidget> RuntimeChatLog;

	TArray<FVisibleResponse> VisibleResponses;
};

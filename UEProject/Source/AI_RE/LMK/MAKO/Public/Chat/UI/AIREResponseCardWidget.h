#pragma once

#include "CoreMinimal.h"
#include "Chat/UI/AIREChatHistorySubsystem.h"
#include "Blueprint/UserWidget.h"
#include "AIREResponseCardWidget.generated.h"

class UAIREResponseStackWidget;
class UWidgetAnimation;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREResponseCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeResponse(
		const FAIREChatLogEntry& Entry,
		UAIREResponseStackWidget* InOwnerStack);
	void Dismiss();
	int64 GetResponseId() const;

private:
	UFUNCTION()
	void HandleDismissAnimationFinished();

	UWidgetAnimation* FindAnimationByPrefix(const FString& Prefix) const;

	UPROPERTY(Transient)
	TObjectPtr<UAIREResponseStackWidget> OwnerStack;

	int64 ResponseId = 0;
	bool bIsDismissing = false;
};

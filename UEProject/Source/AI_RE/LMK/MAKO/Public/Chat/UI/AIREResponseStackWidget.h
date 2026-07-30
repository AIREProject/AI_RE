#pragma once

#include "CoreMinimal.h"
#include "Chat/UI/AIREChatHistorySubsystem.h"
#include "Blueprint/UserWidget.h"
#include "AIREResponseStackWidget.generated.h"

class UAIREResponseCardWidget;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREResponseStackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void AddResponse(const FAIREChatLogEntry& Entry);
	void DismissResponse(int64 ResponseId);
	void ResetResponses();
	void HandleCardDismissed(int64 ResponseId);

private:
	void RefreshStackVisibility();

	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<UAIREResponseCardWidget>> ResponseCards;
};

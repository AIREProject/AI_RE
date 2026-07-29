#pragma once

#include "CoreMinimal.h"
#include "Chat/UI/AIREChatHistorySubsystem.h"
#include "Blueprint/UserWidget.h"
#include "AIREChatLogEntryWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREChatLogEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ApplyEntry(
		const FAIREChatLogEntry& Entry,
		bool bCompactPlayerEntry);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Chat|Style")
	FSlateColor PlayerTextColor = FLinearColor(0.36f, 0.76f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Chat|Style")
	FSlateColor CompanionTextColor = FLinearColor::White;
};

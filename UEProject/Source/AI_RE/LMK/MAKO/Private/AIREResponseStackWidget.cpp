#include "AIREResponseStackWidget.h"

#include "AIREResponseCardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftClassPath ResponseCardClassPath(
		TEXT("/Game/Work/LMK/UI/Chat/WBP_AIREResponseCard.WBP_AIREResponseCard_C"));
}

void UAIREResponseStackWidget::ApplyRuntimeResponses(
	const TArray<FString>& ResponseTexts)
{
	UVerticalBox* ResponseList = WidgetTree
		? Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("ResponseList")))
		: nullptr;
	if (!IsValid(ResponseList))
	{
		return;
	}

	UClass* ResponseCardClass =
		ResponseCardClassPath.TryLoadClass<UAIREResponseCardWidget>();
	if (!IsValid(ResponseCardClass))
	{
		return;
	}

	ResponseList->ClearChildren();
	for (const FString& ResponseText : ResponseTexts)
	{
		UAIREResponseCardWidget* ResponseCard =
			CreateWidget<UAIREResponseCardWidget>(
				GetOwningPlayer(),
				ResponseCardClass);
		if (!IsValid(ResponseCard))
		{
			continue;
		}

		ResponseCard->SetRuntimeResponseText(ResponseText);
		UVerticalBoxSlot* CardSlot = ResponseList->AddChildToVerticalBox(ResponseCard);
		if (IsValid(CardSlot))
		{
			CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}
	}
}

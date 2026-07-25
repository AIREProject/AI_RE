#include "AIREResponseCardWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void UAIREResponseCardWidget::SetRuntimeResponseText(const FString& DisplayText)
{
	UTextBlock* ResponseText = WidgetTree
		? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ResponseText")))
		: nullptr;
	UTextBlock* TimestampText = WidgetTree
		? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TimestampText")))
		: nullptr;
	if (IsValid(ResponseText))
	{
		ResponseText->SetText(FText::FromString(DisplayText));
	}
	if (IsValid(TimestampText))
	{
		TimestampText->SetText(
			FText::FromString(FDateTime::Now().ToString(TEXT("%H:%M:%S"))));
	}
	if (const UWidgetBlueprintGeneratedClass* WidgetClass =
		Cast<UWidgetBlueprintGeneratedClass>(GetClass()))
	{
		for (UWidgetAnimation* Animation : WidgetClass->Animations)
		{
			if (IsValid(Animation)
				&& Animation->GetName().StartsWith(TEXT("ResponseIn")))
			{
				PlayAnimation(Animation);
				break;
			}
		}
	}
}

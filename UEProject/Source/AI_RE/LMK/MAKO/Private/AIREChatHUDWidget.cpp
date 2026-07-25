#include "AIREChatHUDWidget.h"

#include "AIRECompanionChatComponent.h"
#include "AIRECompanionCharacter.h"
#include "AIREResponseStackWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void UAIREChatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RuntimeResponseStack = WidgetTree
		? Cast<UAIREResponseStackWidget>(
			WidgetTree->FindWidget(TEXT("ResponseStack")))
		: nullptr;

	if (IsValid(RuntimeChatComponent))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AAIRECompanionCharacter> It(World); It; ++It)
		{
			if (AAIRECompanionCharacter* Companion = *It; IsValid(Companion))
			{
				UAIRECompanionChatComponent* CompanionChat =
					Companion->GetChatComponent();
				if (IsValid(CompanionChat)
					&& !CompanionChat->HasInGameContext())
				{
					FAIREInGameChatContext DefaultContext;
					DefaultContext.SaveSlotId = TEXT("test-slot-01");
					DefaultContext.Day = 1;
					DefaultContext.Hour = 12.0f;
					DefaultContext.Period =
						EAIREGameWorldPeriod::Afternoon;
					CompanionChat->ConfigureInGameContext(DefaultContext);
				}
				InitializeChatRuntime(CompanionChat);
				break;
			}
		}
	}
}

void UAIREChatHUDWidget::InitializeChatRuntime(
	UAIRECompanionChatComponent* InChatComponent)
{
	if (RuntimeChatComponent == InChatComponent)
	{
		return;
	}

	UnbindChatComponent();
	RuntimeChatComponent = InChatComponent;
	if (!IsValid(RuntimeChatComponent))
	{
		return;
	}

	RuntimeChatComponent->OnResponseReceived.AddUniqueDynamic(
		this,
		&UAIREChatHUDWidget::HandleRuntimeChatResponse);
	RuntimeChatComponent->OnRequestFailed.AddUniqueDynamic(
		this,
		&UAIREChatHUDWidget::HandleRuntimeChatFailure);
}

bool UAIREChatHUDWidget::SubmitPlayerMessage(const FString& UserMessage)
{
	return IsValid(RuntimeChatComponent)
		&& RuntimeChatComponent->SendPlayerMessage(UserMessage);
}

TArray<FString> UAIREChatHUDWidget::GetVisibleResponseTexts() const
{
	return VisibleResponseTexts;
}

void UAIREChatHUDWidget::NativeDestruct()
{
	UnbindChatComponent();
	ClearResponseTimers();
	VisibleResponses.Reset();
	VisibleResponseTexts.Reset();
	RuntimeResponseStack = nullptr;
	Super::NativeDestruct();
}

void UAIREChatHUDWidget::HandleRuntimeChatResponse(const FAIREChatResult& Result)
{
	if (Result.DisplayText.IsEmpty())
	{
		return;
	}

	const int32 ResponseLimit = FMath::Max(1, MaxVisibleResponses);
	while (VisibleResponses.Num() >= ResponseLimit)
	{
		RemoveOldestResponse();
	}

	FVisibleResponse& Response = VisibleResponses.AddDefaulted_GetRef();
	Response.Id = NextResponseId++;
	Response.Text = Result.DisplayText;

	if (UWorld* World = GetWorld())
	{
		const int64 ResponseId = Response.Id;
		World->GetTimerManager().SetTimer(
			Response.ExpirationHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, ResponseId]()
			{
				RemoveResponse(ResponseId);
			}),
			ResponseDisplaySeconds,
			false);
	}

	RefreshVisibleResponseTexts();
}

void UAIREChatHUDWidget::HandleRuntimeChatFailure(const FAIREChatError& Error)
{
	(void)Error;
}

void UAIREChatHUDWidget::RemoveResponse(const int64 ResponseId)
{
	const int32 ResponseIndex = VisibleResponses.IndexOfByPredicate(
		[ResponseId](const FVisibleResponse& Response)
		{
			return Response.Id == ResponseId;
		});
	if (ResponseIndex == INDEX_NONE)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			VisibleResponses[ResponseIndex].ExpirationHandle);
	}
	VisibleResponses.RemoveAt(ResponseIndex);
	RefreshVisibleResponseTexts();
}

void UAIREChatHUDWidget::RemoveOldestResponse()
{
	if (VisibleResponses.IsEmpty())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			VisibleResponses[0].ExpirationHandle);
	}
	VisibleResponses.RemoveAt(0);
}

void UAIREChatHUDWidget::RefreshVisibleResponseTexts()
{
	VisibleResponseTexts.Reset(VisibleResponses.Num());
	for (const FVisibleResponse& Response : VisibleResponses)
	{
		VisibleResponseTexts.Add(Response.Text);
	}
	if (IsValid(RuntimeResponseStack))
	{
		RuntimeResponseStack->ApplyRuntimeResponses(VisibleResponseTexts);
	}
}

void UAIREChatHUDWidget::UnbindChatComponent()
{
	if (!IsValid(RuntimeChatComponent))
	{
		RuntimeChatComponent = nullptr;
		return;
	}

	RuntimeChatComponent->OnResponseReceived.RemoveDynamic(
		this,
		&UAIREChatHUDWidget::HandleRuntimeChatResponse);
	RuntimeChatComponent->OnRequestFailed.RemoveDynamic(
		this,
		&UAIREChatHUDWidget::HandleRuntimeChatFailure);
	RuntimeChatComponent = nullptr;
}

void UAIREChatHUDWidget::ClearResponseTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (FVisibleResponse& Response : VisibleResponses)
		{
			World->GetTimerManager().ClearTimer(Response.ExpirationHandle);
		}
	}
}

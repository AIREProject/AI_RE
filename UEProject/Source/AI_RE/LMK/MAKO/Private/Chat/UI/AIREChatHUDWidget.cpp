#include "Chat/UI/AIREChatHUDWidget.h"

#include "Chat/AIRECompanionChatComponent.h"
#include "Core/AIRECompanionCharacter.h"
#include "Chat/UI/AIREChatLogWidget.h"
#include "Chat/UI/AIREChatPanelWidget.h"
#include "Chat/UI/AIREResponseStackWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UAIREChatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RuntimeResponseStack = WidgetTree
		? Cast<UAIREResponseStackWidget>(
			WidgetTree->FindWidget(TEXT("ResponseStack")))
		: nullptr;
	RuntimeChatPanel = WidgetTree
		? Cast<UAIREChatPanelWidget>(
			WidgetTree->FindWidget(TEXT("ChatPanelView")))
		: nullptr;

	if (IsValid(RuntimeResponseStack))
	{
		RuntimeResponseStack->ResetResponses();
	}
	if (IsValid(RuntimeChatPanel))
	{
		RuntimeChatPanel->InitializeChatPanel(this);
	}

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
	const FString TrimmedMessage = UserMessage.TrimStartAndEnd();
	if (TrimmedMessage.IsEmpty()
		|| !IsValid(RuntimeChatComponent)
		|| !RuntimeChatComponent->SendPlayerMessage(TrimmedMessage))
	{
		return false;
	}

	if (UAIREChatHistorySubsystem* History = GetChatHistory())
	{
		History->AddEntry(
			EAIREChatMessageAuthor::Player,
			TrimmedMessage);
		RefreshChatHistoryViews();
	}
	return true;
}

TArray<FString> UAIREChatHUDWidget::GetVisibleResponseTexts() const
{
	return VisibleResponseTexts;
}

void UAIREChatHUDWidget::InitializeChatLogWidget(
	UAIREChatLogWidget* InChatLogWidget)
{
	RuntimeChatLog = InChatLogWidget;
	if (IsValid(RuntimeChatLog))
	{
		RuntimeChatLog->InitializeChatLog(this);
		RefreshChatHistoryViews();
	}
}

void UAIREChatHUDWidget::HandleGlobalEnterInput()
{
	if (IsValid(RuntimeChatLog) && RuntimeChatLog->IsChatLogOpen())
	{
		return;
	}
	if (IsValid(RuntimeChatPanel)
		&& !RuntimeChatPanel->IsChatInputOpen())
	{
		OpenChatInput();
	}
}

void UAIREChatHUDWidget::HandleGlobalLogInput()
{
	if (IsValid(RuntimeChatLog) && RuntimeChatLog->IsChatLogOpen())
	{
		CloseChatLog();
	}
	else
	{
		OpenChatLog();
	}
}

void UAIREChatHUDWidget::HandlePlayerMessageCommitted(
	const FString& UserMessage)
{
	const FString TrimmedMessage = UserMessage.TrimStartAndEnd();
	if (TrimmedMessage.IsEmpty())
	{
		CloseChatInput();
		return;
	}

	if (SubmitPlayerMessage(TrimmedMessage))
	{
		if (IsValid(RuntimeChatPanel))
		{
			RuntimeChatPanel->ClearMessageInput();
		}
		CloseChatInput();
	}
}

void UAIREChatHUDWidget::CloseChatLog()
{
	if (IsValid(RuntimeChatLog))
	{
		RuntimeChatLog->SetChatLogOpen(false);
	}
	RestoreGameInputMode();
}

void UAIREChatHUDWidget::CloseAllChatUI()
{
	if (IsValid(RuntimeChatPanel))
	{
		RuntimeChatPanel->SetChatInputOpen(false);
	}
	if (IsValid(RuntimeChatLog))
	{
		RuntimeChatLog->SetChatLogOpen(false);
	}
	RestoreGameInputMode();
}

void UAIREChatHUDWidget::NativeDestruct()
{
	CloseAllChatUI();
	UnbindChatComponent();
	ClearResponseTimers();
	VisibleResponses.Reset();
	VisibleResponseTexts.Reset();
	if (IsValid(RuntimeResponseStack))
	{
		RuntimeResponseStack->ResetResponses();
	}
	RuntimeChatPanel = nullptr;
	RuntimeChatLog = nullptr;
	RuntimeResponseStack = nullptr;
	Super::NativeDestruct();
}

void UAIREChatHUDWidget::HandleRuntimeChatResponse(const FAIREChatResult& Result)
{
	const FString DisplayText = Result.DisplayText.TrimStartAndEnd();
	if (DisplayText.IsEmpty())
	{
		return;
	}

	const int32 ResponseLimit = FMath::Max(1, MaxVisibleResponses);
	while (VisibleResponses.Num() >= ResponseLimit)
	{
		RemoveOldestResponse();
	}

	FAIREChatLogEntry LogEntry;
	if (UAIREChatHistorySubsystem* History = GetChatHistory())
	{
		LogEntry = History->AddEntry(
			EAIREChatMessageAuthor::Companion,
			DisplayText);
		RefreshChatHistoryViews();
	}
	else
	{
		LogEntry.Sequence = FDateTime::UtcNow().GetTicks();
		LogEntry.Author = EAIREChatMessageAuthor::Companion;
		LogEntry.Text = DisplayText;
		LogEntry.Timestamp = FDateTime::Now();
	}

	FVisibleResponse& Response = VisibleResponses.AddDefaulted_GetRef();
	Response.Id = LogEntry.Sequence;
	Response.Entry = LogEntry;

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

	if (IsValid(RuntimeResponseStack))
	{
		RuntimeResponseStack->AddResponse(Response.Entry);
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
	if (IsValid(RuntimeResponseStack))
	{
		RuntimeResponseStack->DismissResponse(ResponseId);
	}
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
	if (IsValid(RuntimeResponseStack))
	{
		RuntimeResponseStack->DismissResponse(
			VisibleResponses[0].Id);
	}
	VisibleResponses.RemoveAt(0);
	RefreshVisibleResponseTexts();
}

void UAIREChatHUDWidget::RefreshVisibleResponseTexts()
{
	VisibleResponseTexts.Reset(VisibleResponses.Num());
	for (const FVisibleResponse& Response : VisibleResponses)
	{
		VisibleResponseTexts.Add(Response.Entry.Text);
	}
}

void UAIREChatHUDWidget::RefreshChatHistoryViews()
{
	UAIREChatHistorySubsystem* History = GetChatHistory();
	if (!IsValid(History))
	{
		return;
	}

	const TArray<FAIREChatLogEntry>& Entries = History->GetEntries();
	if (IsValid(RuntimeChatPanel))
	{
		RuntimeChatPanel->RefreshPlayerHistory(Entries);
	}
	if (IsValid(RuntimeChatLog))
	{
		RuntimeChatLog->RefreshEntries(Entries);
	}
}

void UAIREChatHUDWidget::OpenChatInput()
{
	if (!IsValid(RuntimeChatPanel))
	{
		return;
	}

	if (IsValid(RuntimeChatLog))
	{
		RuntimeChatLog->SetChatLogOpen(false);
	}
	RefreshChatHistoryViews();
	RuntimeChatPanel->SetChatInputOpen(true);
	ApplyUIInputMode(RuntimeChatPanel->GetInputFocusTarget(), false);
}

void UAIREChatHUDWidget::CloseChatInput()
{
	if (IsValid(RuntimeChatPanel))
	{
		RuntimeChatPanel->SetChatInputOpen(false);
	}
	RestoreGameInputMode();
}

void UAIREChatHUDWidget::OpenChatLog()
{
	if (!IsValid(RuntimeChatLog))
	{
		return;
	}

	if (IsValid(RuntimeChatPanel))
	{
		RuntimeChatPanel->SetChatInputOpen(false);
	}
	RefreshChatHistoryViews();
	RuntimeChatLog->SetChatLogOpen(true);
	ApplyUIInputMode(RuntimeChatLog->GetLogFocusTarget(), true);
}

void UAIREChatHUDWidget::ApplyUIInputMode(
	UWidget* FocusTarget,
	const bool bShowMouseCursor)
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!bOwnsInputSuppression)
	{
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
		bOwnsInputSuppression = true;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(!bShowMouseCursor);
	if (IsValid(FocusTarget))
	{
		InputMode.SetWidgetToFocus(FocusTarget->TakeWidget());
	}
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(bShowMouseCursor);
}

void UAIREChatHUDWidget::RestoreGameInputMode()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		bOwnsInputSuppression = false;
		return;
	}

	if (bOwnsInputSuppression)
	{
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
		bOwnsInputSuppression = false;
	}

	FInputModeGameOnly InputMode;
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(false);
}

UAIREChatHistorySubsystem* UAIREChatHUDWidget::GetChatHistory() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance =
		IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREChatHistorySubsystem>()
		: nullptr;
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

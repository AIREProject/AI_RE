// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI_REPlayerController.h"

#include "AI_RE.h"
#include "AIREAggroSwapComponent.h"
#include "AI_RECharacter.h"
#include "AI_REMainUI.h"
#include "Blueprint/UserWidget.h"
#include "Chat/AIRECompanionChatComponent.h"
#include "Chat/UI/AIREChatHUDWidget.h"
#include "Chat/UI/AIREChatLogWidget.h"
#include "Components/Widget.h"
#include "Core/AIRECompanionCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputMappingContext.h"
#include "LocalAI/UI/AIRECompanionPolicyPanelWidget.h"
#include "UI/AIRECompanionStatusWidget.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
constexpr int32 MainHUDZOrder = 0;
constexpr int32 CompanionStatusZOrder = 10;
constexpr int32 ChatHUDZOrder = 100;
constexpr int32 ChatLogZOrder = 110;
constexpr int32 PolicyHUDZOrder = 120;
}

AAI_REPlayerController::AAI_REPlayerController()
{
	AggroSwapComponent =
		CreateDefaultSubobject<UAIREAggroSwapComponent>(TEXT("AggroSwap"));
}

UAIREAggroSwapComponent* AAI_REPlayerController::GetAggroSwapComponent() const
{
	return AggroSwapComponent;
}

void AAI_REPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (ShouldUseTouchControls())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (IsValid(MobileControlsWidget))
		{
			MobileControlsWidget->AddToPlayerScreen(MainHUDZOrder);
		}
		else
		{
			UE_LOG(LogAI_RE, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}

	CreateLocalHUD();
	RefreshPlayerHUD();
	FindAndBindCompanion();

	if (UWorld* World = GetWorld())
	{
		ActorSpawnedDelegateHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(
				this,
				&AAI_REPlayerController::HandleActorSpawned));
	}
}

void AAI_REPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActorSpawnedDelegateHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
		}
		ActorSpawnedDelegateHandle.Reset();
	}

	ShutdownLocalHUD();
	Super::EndPlay(EndPlayReason);
}

void AAI_REPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (IsLocalPlayerController())
	{
		CreateLocalHUD();
		RefreshPlayerHUD();
	}
}

void AAI_REPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);

	if (IsLocalPlayerController())
	{
		CreateLocalHUD();
		RefreshPlayerHUD();
	}
}

void AAI_REPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInput =
		Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IsValid(AggroSwapAction))
		{
			EnhancedInput->BindAction(
				AggroSwapAction,
				ETriggerEvent::Started,
				this,
				&AAI_REPlayerController::HandleAggroSwapInput);
		}

		if (IsValid(ChatEnterAction))
		{
			EnhancedInput->BindAction(
				ChatEnterAction,
				ETriggerEvent::Started,
				this,
				&AAI_REPlayerController::HandleChatEnterInput);
		}

		if (IsValid(ChatLogAction))
		{
			EnhancedInput->BindAction(
				ChatLogAction,
				ETriggerEvent::Started,
				this,
				&AAI_REPlayerController::HandleChatLogInput);
		}

		if (IsValid(CompanionPolicyAction))
		{
			EnhancedInput->BindAction(
				CompanionPolicyAction,
				ETriggerEvent::Started,
				this,
				&AAI_REPlayerController::HandlePolicyInputStarted);
			EnhancedInput->BindAction(
				CompanionPolicyAction,
				ETriggerEvent::Completed,
				this,
				&AAI_REPlayerController::HandlePolicyInputCompleted);
			EnhancedInput->BindAction(
				CompanionPolicyAction,
				ETriggerEvent::Canceled,
				this,
				&AAI_REPlayerController::HandlePolicyInputCanceled);
		}
	}

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			if (IsValid(CurrentContext))
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}

		if (!ShouldUseTouchControls())
		{
			for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
			{
				if (IsValid(CurrentContext))
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		if (IsValid(UserInterfaceMappingContext))
		{
			Subsystem->AddMappingContext(UserInterfaceMappingContext, 10);
		}
	}
}

bool AAI_REPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AAI_REPlayerController::HandleAggroSwapInput()
{
	if (IsValid(AggroSwapComponent))
	{
		AggroSwapComponent->TryAggroSwap();
	}
}

void AAI_REPlayerController::HandleChatEnterInput()
{
	if (!IsValid(ChatHUD) || IsCharacterModalUIOpen() || ChatHUD->IsChatInputOpen())
	{
		return;
	}

	if (IsValid(CompanionPolicyWidget) && CompanionPolicyWidget->IsPanelOpen())
	{
		CompanionPolicyWidget->SetPanelOpen(false);
	}

	ChatHUD->HandleGlobalEnterInput();
}

void AAI_REPlayerController::HandleChatLogInput()
{
	if (!IsValid(ChatHUD) || IsCharacterModalUIOpen())
	{
		return;
	}

	if (IsValid(CompanionPolicyWidget) && CompanionPolicyWidget->IsPanelOpen())
	{
		CompanionPolicyWidget->SetPanelOpen(false);
	}

	ChatHUD->HandleGlobalLogInput();
}

void AAI_REPlayerController::HandlePolicyInputStarted()
{
	if (!IsValid(CompanionPolicyWidget) || IsCharacterModalUIOpen())
	{
		return;
	}

	if (CompanionPolicyWidget->IsPanelOpen())
	{
		CompanionPolicyWidget->SetPanelOpen(false);
		ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
		return;
	}

	if (IsValid(ChatHUD))
	{
		ChatHUD->CloseAllChatUI();
	}

	CompanionPolicyWidget->BeginPolicySelection();
	ApplyLocalUIInputMode(
		EAIRELocalUIInputMode::PolicySelection,
		CompanionPolicyWidget);
}

void AAI_REPlayerController::HandlePolicyInputCompleted()
{
	if (!IsValid(CompanionPolicyWidget) || !CompanionPolicyWidget->IsPanelOpen())
	{
		return;
	}

	CompanionPolicyWidget->CommitPolicySelection();
	CompanionPolicyWidget->SetPanelOpen(false);
	ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
}

void AAI_REPlayerController::HandlePolicyInputCanceled()
{
	if (!IsValid(CompanionPolicyWidget) || !CompanionPolicyWidget->IsPanelOpen())
	{
		return;
	}

	CompanionPolicyWidget->SetPanelOpen(false);
	ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
}

void AAI_REPlayerController::HandleChatUIStateChanged()
{
	if (!IsValid(ChatHUD))
	{
		ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
		return;
	}

	if (ChatHUD->IsChatInputOpen())
	{
		ApplyLocalUIInputMode(
			EAIRELocalUIInputMode::ChatInput,
			ChatHUD->GetChatInputFocusTarget());
	}
	else if (ChatHUD->IsChatLogOpen())
	{
		ApplyLocalUIInputMode(
			EAIRELocalUIInputMode::ChatLog,
			ChatHUD->GetChatLogFocusTarget());
	}
	else
	{
		ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
	}
}

void AAI_REPlayerController::CreateLocalHUD()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!IsValid(MainHUD) && MainHUDClass)
	{
		MainHUD = CreateWidget<UAI_REMainUI>(this, MainHUDClass);
		if (IsValid(MainHUD))
		{
			MainHUD->AddToPlayerScreen(MainHUDZOrder);
		}
	}

	if (!IsValid(CompanionStatusWidget) && CompanionStatusWidgetClass)
	{
		CompanionStatusWidget = CreateWidget<UAIRECompanionStatusWidget>(
			this,
			CompanionStatusWidgetClass);
		if (IsValid(CompanionStatusWidget))
		{
			CompanionStatusWidget->AddToPlayerScreen(CompanionStatusZOrder);
		}
	}

	if (!IsValid(ChatHUD) && ChatHUDClass)
	{
		ChatHUD = CreateWidget<UAIREChatHUDWidget>(this, ChatHUDClass);
		if (IsValid(ChatHUD))
		{
			ChatHUD->AddToPlayerScreen(ChatHUDZOrder);
			ChatHUD->OnChatUIStateChanged.AddUObject(
				this,
				&AAI_REPlayerController::HandleChatUIStateChanged);
		}
	}

	if (!IsValid(ChatLog) && ChatLogClass)
	{
		ChatLog = CreateWidget<UAIREChatLogWidget>(this, ChatLogClass);
		if (IsValid(ChatLog))
		{
			ChatLog->AddToPlayerScreen(ChatLogZOrder);
		}
	}

	if (IsValid(ChatHUD) && IsValid(ChatLog))
	{
		ChatHUD->InitializeChatLogWidget(ChatLog);
	}

	if (!IsValid(CompanionPolicyWidget) && CompanionPolicyWidgetClass)
	{
		CompanionPolicyWidget = CreateWidget<UAIRECompanionPolicyPanelWidget>(
			this,
			CompanionPolicyWidgetClass);
		if (IsValid(CompanionPolicyWidget))
		{
			CompanionPolicyWidget->AddToPlayerScreen(PolicyHUDZOrder);
			CompanionPolicyWidget->SetPanelOpen(false);
		}
	}
}

void AAI_REPlayerController::RefreshPlayerHUD()
{
	if (!IsValid(MainHUD))
	{
		return;
	}

	AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(GetPawn());
	MainHUD->InitializeHUD(
		IsValid(PlayerCharacter) ? PlayerCharacter->GetStatusComponent().Get() : nullptr);
}

void AAI_REPlayerController::FindAndBindCompanion()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AAIRECompanionCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetCompanionId() == TEXT("MAKO"))
		{
			BindCompanion(*It);
			return;
		}
	}

	UnbindCompanion();
}

void AAI_REPlayerController::BindCompanion(AAIRECompanionCharacter* Companion)
{
	if (!IsValid(Companion) || Companion->GetCompanionId() != TEXT("MAKO"))
	{
		return;
	}

	if (BoundCompanion.Get() == Companion)
	{
		return;
	}

	UnbindCompanion();
	BoundCompanion = Companion;
	Companion->OnDestroyed.AddUniqueDynamic(
		this,
		&AAI_REPlayerController::HandleCompanionDestroyed);

	if (IsValid(CompanionStatusWidget))
	{
		CompanionStatusWidget->BindCompanion(Companion);
	}

	if (IsValid(CompanionPolicyWidget))
	{
		CompanionPolicyWidget->BindCompanion(Companion);
	}

	if (IsValid(ChatHUD))
	{
		if (UAIRECompanionChatComponent* ChatComponent = Companion->GetChatComponent())
		{
			if (!ChatComponent->HasInGameContext())
			{
				FAIREInGameChatContext Context;
				Context.SaveSlotId = TEXT("demo-slot-1");
				const FDateTime Now = FDateTime::Now();
				Context.Day = Now.GetDay();
				Context.Hour = static_cast<float>(Now.GetHour()) + static_cast<float>(Now.GetMinute()) / 60.0f;
				const int32 HourInt = Now.GetHour();
				if (HourInt >= 5 && HourInt < 8)
				{
					Context.Period = EAIREGameWorldPeriod::Dawn;
				}
				else if (HourInt >= 8 && HourInt < 12)
				{
					Context.Period = EAIREGameWorldPeriod::Morning;
				}
				else if (HourInt >= 12 && HourInt < 18)
				{
					Context.Period = EAIREGameWorldPeriod::Afternoon;
				}
				else if (HourInt >= 18 && HourInt < 22)
				{
					Context.Period = EAIREGameWorldPeriod::Evening;
				}
				else
				{
					Context.Period = EAIREGameWorldPeriod::Night;
				}
				ChatComponent->ConfigureInGameContext(Context);
			}
			ChatHUD->InitializeChatRuntime(ChatComponent);
		}
	}
}

void AAI_REPlayerController::UnbindCompanion()
{
	if (AAIRECompanionCharacter* Companion = BoundCompanion.Get())
	{
		Companion->OnDestroyed.RemoveDynamic(
			this,
			&AAI_REPlayerController::HandleCompanionDestroyed);
	}

	if (IsValid(CompanionStatusWidget))
	{
		CompanionStatusWidget->UnbindCompanion();
	}

	if (IsValid(CompanionPolicyWidget))
	{
		CompanionPolicyWidget->UnbindCompanion();
	}

	if (IsValid(ChatHUD))
	{
		ChatHUD->InitializeChatRuntime(nullptr);
	}

	BoundCompanion.Reset();
}

void AAI_REPlayerController::HandleActorSpawned(AActor* SpawnedActor)
{
	AAIRECompanionCharacter* Companion = Cast<AAIRECompanionCharacter>(SpawnedActor);
	if (!BoundCompanion.IsValid() && IsValid(Companion) &&
		Companion->GetCompanionId() == TEXT("MAKO"))
	{
		BindCompanion(Companion);
	}
}

void AAI_REPlayerController::HandleCompanionDestroyed(AActor* DestroyedActor)
{
	if (!BoundCompanion.IsValid() || DestroyedActor == BoundCompanion.Get())
	{
		UnbindCompanion();
	}
}

void AAI_REPlayerController::ShutdownLocalHUD()
{
	ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
	UnbindCompanion();

	if (IsValid(ChatHUD))
	{
		ChatHUD->OnChatUIStateChanged.RemoveAll(this);
		ChatHUD->CloseAllChatUI();
		ChatHUD->RemoveFromParent();
		ChatHUD = nullptr;
	}

	if (IsValid(ChatLog))
	{
		ChatLog->RemoveFromParent();
		ChatLog = nullptr;
	}

	if (IsValid(CompanionPolicyWidget))
	{
		CompanionPolicyWidget->RemoveFromParent();
		CompanionPolicyWidget = nullptr;
	}

	if (IsValid(CompanionStatusWidget))
	{
		CompanionStatusWidget->RemoveFromParent();
		CompanionStatusWidget = nullptr;
	}

	if (IsValid(MainHUD))
	{
		MainHUD->RemoveFromParent();
		MainHUD = nullptr;
	}

	if (IsValid(MobileControlsWidget))
	{
		MobileControlsWidget->RemoveFromParent();
		MobileControlsWidget = nullptr;
	}
}

void AAI_REPlayerController::ApplyLocalUIInputMode(
	const EAIRELocalUIInputMode NewMode,
	UWidget* FocusTarget)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const bool bNeedsUISuppression = NewMode != EAIRELocalUIInputMode::Gameplay;
	if (bNeedsUISuppression && !bOwnsUIInputSuppression)
	{
		bPreviousShowMouseCursor = ShouldShowMouseCursor();
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		bOwnsUIInputSuppression = true;
	}
	else if (!bNeedsUISuppression && bOwnsUIInputSuppression)
	{
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		bOwnsUIInputSuppression = false;
	}

	if (NewMode == EAIRELocalUIInputMode::Gameplay)
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		SetShowMouseCursor(bPreviousShowMouseCursor);
	}
	else
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		if (IsValid(FocusTarget))
		{
			InputMode.SetWidgetToFocus(FocusTarget->TakeWidget());
		}
		SetInputMode(InputMode);
		SetShowMouseCursor(
			NewMode == EAIRELocalUIInputMode::ChatLog
			|| NewMode == EAIRELocalUIInputMode::PolicySelection);
	}

	LocalUIInputMode = NewMode;
}

bool AAI_REPlayerController::IsCharacterModalUIOpen() const
{
	const AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(GetPawn());
	return IsValid(PlayerCharacter) &&
		(PlayerCharacter->IsInventoryUIOpen() || PlayerCharacter->IsCraftingUIOpen());
}

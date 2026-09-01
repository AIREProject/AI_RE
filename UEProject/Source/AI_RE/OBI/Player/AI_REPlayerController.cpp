// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI_REPlayerController.h"

#include "AI_RE.h"
#include "AIREAggroSwapComponent.h"
#include "AIREBossEnemy.h"
#include "AI_RECharacter.h"
#include "AI_REItemSubsystem.h"
#include "AI_REMainUI.h"
#include "AI_REPlayerCombatComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "AIREBossHUDWidget.h"
#include "AIREGameOverWidget.h"
#include "AIREGameplayGuidanceSubsystem.h"
#include "AIREGameplayInventorySubsystem.h"
#include "AIRELevelTransitionSubsystem.h"
#include "AIREQuitConfirmationWidget.h"
#include "AIRETargetLockMarkerWidget.h"
#include "AI_RETargetScannerComponent.h"
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
#include "Inventory/UI/AIRECompanionInventoryPanelWidget.h"
#include "Inventory/UI/AIREInventoryUIWorldSubsystem.h"
#include "Inventory/UI/AIREStorageInventoryPanelWidget.h"
#include "LocalAI/UI/AIRECompanionPolicyPanelWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/AIRECompanionStatusWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
constexpr int32 MainHUDZOrder = 0;
constexpr int32 CompanionStatusZOrder = 10;
constexpr int32 BossHUDZOrder = 20;
constexpr int32 ChatHUDZOrder = 100;
constexpr int32 ChatLogZOrder = 110;
constexpr int32 PolicyHUDZOrder = 120;
constexpr int32 TargetLockMarkerZOrder = 150;
constexpr int32 QuitConfirmationZOrder = 190;
constexpr int32 GameOverZOrder = 200;
}

AAI_REPlayerController::AAI_REPlayerController()
{
	AggroSwapComponent =
		CreateDefaultSubobject<UAIREAggroSwapComponent>(TEXT("AggroSwap"));

	static ConstructorHelpers::FClassFinder<UAIREStorageInventoryPanelWidget>
		StorageInventoryPanelFinder(
			TEXT("/Game/Work/LMK/UI/Inventory/WBP_AIREStorageInventoryPanel"));
	if (StorageInventoryPanelFinder.Succeeded())
	{
		StorageInventoryPanelClass = StorageInventoryPanelFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<UAIRECompanionInventoryPanelWidget>
		CompanionInventoryPanelFinder(
			TEXT("/Game/Work/LMK/UI/Inventory/WBP_AIRECompanionInventoryPanel"));
	if (CompanionInventoryPanelFinder.Succeeded())
	{
		CompanionInventoryPanelClass = CompanionInventoryPanelFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<UAIREBossHUDWidget>
		BossHUDWidgetFinder(
			TEXT("/Game/Work/LMK/UI/Boss/WBP_AIREBossHUD"));
	if (BossHUDWidgetFinder.Succeeded())
	{
		BossHUDClass = BossHUDWidgetFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<UAIREQuitConfirmationWidget>
		QuitConfirmationWidgetFinder(
			TEXT("/Game/Work/Global/UI/Quit/WBP_AIREQuitConfirmation"));
	if (QuitConfirmationWidgetFinder.Succeeded())
	{
		QuitConfirmationWidgetClass = QuitConfirmationWidgetFinder.Class;
	}
}

UAIREAggroSwapComponent* AAI_REPlayerController::GetAggroSwapComponent() const
{
	return AggroSwapComponent;
}

TSubclassOf<UAIREStorageInventoryPanelWidget>
AAI_REPlayerController::GetStorageInventoryPanelClass() const
{
	return StorageInventoryPanelClass;
}

TSubclassOf<UAIRECompanionInventoryPanelWidget>
AAI_REPlayerController::GetCompanionInventoryPanelClass() const
{
	return CompanionInventoryPanelClass;
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
	TryShowDeathReturnGuidance();
	if (UAIRELevelTransitionSubsystem* Transition =
		GetGameInstance()->GetSubsystem<UAIRELevelTransitionSubsystem>())
	{
		Transition->PreloadLevel(FName(TEXT("/Game/Levels/AIRE_TitleLevel")));
	}
	BindPlayerTargetScanner(GetPawn());
	RefreshPlayerHUD();
	FindAndBindCompanion();
	FindAndBindBoss();

	if (UWorld* World = GetWorld())
	{
		ActorSpawnedDelegateHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(
				this,
				&AAI_REPlayerController::HandleActorSpawned));
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&AAI_REPlayerController::FindAndBindBoss));
	}
}

void AAI_REPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DeathReturnGuidanceTimer);
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
		BindPlayerTargetScanner(InPawn);
		CreateLocalHUD();
		RefreshPlayerHUD();
	}
}

void AAI_REPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);

	if (IsLocalPlayerController())
	{
		BindPlayerTargetScanner(InPawn);
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

		if (IsValid(EscapeAction))
		{
			EnhancedInput->BindAction(
				EscapeAction,
				ETriggerEvent::Started,
				this,
				&AAI_REPlayerController::HandleEscapeInput);
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

void AAI_REPlayerController::HandleEscapeInput()
{
	if (!IsLocalPlayerController() || IsValid(GameOverWidget))
	{
		return;
	}

	if (IsValid(QuitConfirmationWidget))
	{
		CancelExitConfirmation();
		return;
	}

	if (AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(GetPawn()))
	{
		if (PlayerCharacter->TryCloseActiveModalUI())
		{
			return;
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UAIREInventoryUIWorldSubsystem* InventoryUISubsystem =
			World->GetSubsystem<UAIREInventoryUIWorldSubsystem>();
			IsValid(InventoryUISubsystem)
			&& InventoryUISubsystem->IsInventoryUIOpen())
		{
			InventoryUISubsystem->CloseInventoryUI();
			return;
		}
	}

	if (IsValid(CompanionPolicyWidget)
		&& CompanionPolicyWidget->IsPanelOpen())
	{
		CompanionPolicyWidget->SetPanelOpen(false);
		ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
		return;
	}

	if (IsValid(ChatHUD)
		&& (ChatHUD->IsChatInputOpen() || ChatHUD->IsChatLogOpen()))
	{
		ChatHUD->CloseAllChatUI();
		ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
		return;
	}

	OpenExitConfirmation();
}

void AAI_REPlayerController::HandleChatEnterInput()
{
	if (LocalUIInputMode == EAIRELocalUIInputMode::QuitConfirmation
		|| LocalUIInputMode == EAIRELocalUIInputMode::GameOver
		|| !IsValid(ChatHUD)
		|| IsCharacterModalUIOpen()
		|| ChatHUD->IsChatInputOpen())
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
	if (LocalUIInputMode == EAIRELocalUIInputMode::QuitConfirmation
		|| LocalUIInputMode == EAIRELocalUIInputMode::GameOver
		|| !IsValid(ChatHUD)
		|| IsCharacterModalUIOpen())
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
	if (LocalUIInputMode == EAIRELocalUIInputMode::QuitConfirmation
		|| LocalUIInputMode == EAIRELocalUIInputMode::GameOver
		|| !IsValid(CompanionPolicyWidget)
		|| IsCharacterModalUIOpen())
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

	if (!IsValid(BossHUD) && BossHUDClass)
	{
		BossHUD = CreateWidget<UAIREBossHUDWidget>(this, BossHUDClass);
		if (IsValid(BossHUD))
		{
			BossHUD->AddToPlayerScreen(BossHUDZOrder);
			if (AAIREBossEnemy* Boss = BoundBoss.Get())
			{
				BossHUD->BindBoss(Boss);
			}
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

	if (!IsValid(TargetLockMarkerWidget) && TargetLockMarkerWidgetClass)
	{
		TargetLockMarkerWidget = CreateWidget<UAIRETargetLockMarkerWidget>(
			this,
			TargetLockMarkerWidgetClass);
		if (IsValid(TargetLockMarkerWidget))
		{
			TargetLockMarkerWidget->AddToPlayerScreen(TargetLockMarkerZOrder);
			TargetLockMarkerWidget->SetLockedTarget(
				BoundTargetScanner.IsValid()
					? BoundTargetScanner->GetCurrentCombatTarget()
					: nullptr);
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

void AAI_REPlayerController::HandlePlayerDeath()
{
	if (!IsLocalPlayerController() || IsValid(GameOverWidget))
	{
		return;
	}
	if (IsValid(QuitConfirmationWidget))
	{
		CancelExitConfirmation();
	}
	if (AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(GetPawn()))
	{
		PlayerCharacter->TryCloseActiveModalUI();
	}
	if (IsValid(ChatHUD))
	{
		ChatHUD->CloseAllChatUI();
	}
	if (IsValid(CompanionPolicyWidget))
	{
		CompanionPolicyWidget->SetPanelOpen(false);
	}
	if (!GameOverWidgetClass)
	{
		UE_LOG(LogAI_RE, Error, TEXT("GameOverWidgetClass is not configured."));
		return;
	}

	GameOverWidget = CreateWidget<UAIREGameOverWidget>(this, GameOverWidgetClass);
	if (!IsValid(GameOverWidget))
	{
		UE_LOG(LogAI_RE, Error, TEXT("Could not create the game-over widget."));
		return;
	}
	GameOverWidget->AddToPlayerScreen(GameOverZOrder);
	ApplyLocalUIInputMode(
		EAIRELocalUIInputMode::GameOver,
		GameOverWidget->GetInitialFocusWidget());
}

void AAI_REPlayerController::ReturnToVillageAfterDeath()
{
	if (IsLocalPlayerController())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UAIREGameplayGuidanceSubsystem* Guidance =
				GameInstance->GetSubsystem<UAIREGameplayGuidanceSubsystem>())
			{
				Guidance->QueueDeathReturnGuidance();
			}
		}
		if (UAIRELevelTransitionSubsystem* Transition =
			GetGameInstance()->GetSubsystem<UAIRELevelTransitionSubsystem>())
		{
			Transition->RequestTravel(
				this, FName(TEXT("/Game/Levels/MainLevel_Top")));
		}
	}
}

void AAI_REPlayerController::TryShowDeathReturnGuidance()
{
	UGameInstance* GameInstance = GetGameInstance();
	UAIREGameplayGuidanceSubsystem* Guidance = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREGameplayGuidanceSubsystem>()
		: nullptr;
	UAIREGameplayInventorySubsystem* InventorySubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
		: nullptr;
	AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(GetPawn());
	UAI_REPlayerInventoryComponent* PlayerInventory = IsValid(PlayerCharacter)
		? PlayerCharacter->GetInventoryComponent()
		: nullptr;

	if (!IsValid(Guidance) || !IsValid(InventorySubsystem)
		|| !InventorySubsystem->IsPersistenceReady()
		|| !IsValid(ChatHUD) || !IsValid(PlayerInventory)
		|| !PlayerInventory->IsPersistenceReadyForGameplay())
	{
		GetWorldTimerManager().SetTimer(
			DeathReturnGuidanceTimer,
			this,
			&AAI_REPlayerController::TryShowDeathReturnGuidance,
			0.1f,
			false);
		return;
	}

	GetWorldTimerManager().ClearTimer(DeathReturnGuidanceTimer);
	if (!Guidance->ConsumeDeathReturnGuidance())
	{
		return;
	}

	UAI_REPlayerCombatComponent* PlayerCombat =
		PlayerCharacter->GetCombatComponent();
	bool bHasWeapon = !PlayerInventory->GetEquippedWeaponItemId().IsNone()
		|| (IsValid(PlayerCombat)
			&& IsValid(PlayerCombat->EquippedWeapon)
			&& PlayerCombat->EquippedWeapon
				!= PlayerCombat->DefaultUnarmedWeapon);
	if (!bHasWeapon)
	{
		if (UAI_REItemSubsystem* ItemSubsystem =
			GameInstance->GetSubsystem<UAI_REItemSubsystem>())
		{
			for (const FInventoryItemStack& Stack : PlayerInventory->Items)
			{
				const UAI_REItemDataAsset* Item =
					ItemSubsystem->GetItemDataAsset(Stack.ItemId);
				if (IsValid(Item) && Item->ItemType == EAI_REItemType::Weapon)
				{
					bHasWeapon = true;
					break;
				}
			}
		}
	}

	ChatHUD->ShowLocalCompanionMessage(
		bHasWeapon
			? TEXT("무기는 준비됐네. 다음 전투를 위해 기본 제작대에서 붕대를 만들거나, 나한테 붕대 제작을 부탁해 봐.")
			: TEXT("무기가 없네. 창고를 확인하거나 나한테 무기를 만들어 달라고 해 봐."));
}

void AAI_REPlayerController::OpenExitConfirmation()
{
	if (!IsLocalPlayerController()
		|| IsValid(GameOverWidget)
		|| IsValid(QuitConfirmationWidget))
	{
		return;
	}

	if (!QuitConfirmationWidgetClass)
	{
		QuitConfirmationWidgetClass = LoadClass<UAIREQuitConfirmationWidget>(
			nullptr,
			TEXT("/Game/Work/Global/UI/Quit/WBP_AIREQuitConfirmation.WBP_AIREQuitConfirmation_C"));
	}
	if (!QuitConfirmationWidgetClass)
	{
		UE_LOG(
			LogAI_RE,
			Error,
			TEXT("QuitConfirmationWidgetClass is not configured."));
		return;
	}

	QuitConfirmationWidget = CreateWidget<UAIREQuitConfirmationWidget>(
		this,
		QuitConfirmationWidgetClass);
	if (!IsValid(QuitConfirmationWidget))
	{
		UE_LOG(
			LogAI_RE,
			Error,
			TEXT("Could not create the quit-confirmation widget."));
		return;
	}

	bWasGamePausedBeforeQuitConfirmation = UGameplayStatics::IsGamePaused(this);
	if (!bWasGamePausedBeforeQuitConfirmation)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}

	QuitConfirmationWidget->AddToPlayerScreen(QuitConfirmationZOrder);
	ApplyLocalUIInputMode(
		EAIRELocalUIInputMode::QuitConfirmation,
		QuitConfirmationWidget->GetInitialFocusWidget());
	QuitConfirmationWidget->SetUserFocus(this);
}

void AAI_REPlayerController::ConfirmExitGame()
{
	if (!IsLocalPlayerController() || !IsValid(QuitConfirmationWidget))
	{
		return;
	}

	QuitConfirmationWidget->SetIsEnabled(false);
	UGameplayStatics::SetGamePaused(this, false);
	if (UAIRELevelTransitionSubsystem* Transition =
		GetGameInstance()->GetSubsystem<UAIRELevelTransitionSubsystem>())
	{
		Transition->RequestTravel(
			this, FName(TEXT("/Game/Levels/AIRE_TitleLevel")));
	}
}

void AAI_REPlayerController::CancelExitConfirmation()
{
	if (!IsValid(QuitConfirmationWidget))
	{
		return;
	}

	QuitConfirmationWidget->RemoveFromParent();
	QuitConfirmationWidget = nullptr;
	if (!bWasGamePausedBeforeQuitConfirmation)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}
	bWasGamePausedBeforeQuitConfirmation = false;
	ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
}

void AAI_REPlayerController::BindPlayerTargetScanner(APawn* PlayerPawn)
{
	AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(PlayerPawn);
	UAI_RETargetScannerComponent* TargetScanner = IsValid(PlayerCharacter)
		? PlayerCharacter->GetTargetScannerComponent()
		: nullptr;
	if (BoundTargetScanner.Get() == TargetScanner)
	{
		return;
	}

	UnbindPlayerTargetScanner();
	if (!IsValid(TargetScanner))
	{
		return;
	}

	BoundTargetScanner = TargetScanner;
	TargetScanner->OnCombatStateChanged.AddUniqueDynamic(
		this,
		&AAI_REPlayerController::HandlePlayerCombatStateChanged);
	HandlePlayerCombatStateChanged(
		IsValid(TargetScanner->GetCurrentCombatTarget()),
		TargetScanner->GetCurrentCombatTarget());
}

void AAI_REPlayerController::UnbindPlayerTargetScanner()
{
	if (UAI_RETargetScannerComponent* TargetScanner = BoundTargetScanner.Get())
	{
		TargetScanner->OnCombatStateChanged.RemoveDynamic(
			this,
			&AAI_REPlayerController::HandlePlayerCombatStateChanged);
	}
	BoundTargetScanner.Reset();

	if (IsValid(TargetLockMarkerWidget))
	{
		TargetLockMarkerWidget->SetLockedTarget(nullptr);
	}
}

void AAI_REPlayerController::HandlePlayerCombatStateChanged(
	const bool bIsCombat,
	AActor* CombatTarget)
{
	if (IsValid(TargetLockMarkerWidget))
	{
		TargetLockMarkerWidget->SetLockedTarget(
			bIsCombat ? CombatTarget : nullptr);
	}
}

bool AAI_REPlayerController::CanProcessGameplayActionInput() const
{
	const AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(GetPawn());
	const bool bChatOpen = IsValid(ChatHUD)
		&& (ChatHUD->IsChatInputOpen() || ChatHUD->IsChatLogOpen());
	const bool bPolicyOpen = IsValid(CompanionPolicyWidget)
		&& CompanionPolicyWidget->IsPanelOpen();
	return IsLocalPlayerController()
		&& IsValid(PlayerCharacter)
		&& !PlayerCharacter->IsDead()
		&& LocalUIInputMode == EAIRELocalUIInputMode::Gameplay
		&& !IsCharacterModalUIOpen()
		&& !bChatOpen
		&& !bPolicyOpen
		&& !IsValid(QuitConfirmationWidget)
		&& !IsValid(GameOverWidget);
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

void AAI_REPlayerController::FindAndBindBoss()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AAIREBossEnemy> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It)
			&& It->HasActorBegunPlay()
			&& It->IsCombatTargetAlive())
		{
			BindBoss(*It);
			return;
		}
	}

	UnbindBoss();
}

void AAI_REPlayerController::BindBoss(AAIREBossEnemy* Boss)
{
	if (!IsValid(Boss) || !Boss->IsCombatTargetAlive())
	{
		return;
	}

	if (BoundBoss.Get() == Boss)
	{
		if (IsValid(BossHUD))
		{
			BossHUD->BindBoss(Boss);
		}
		return;
	}

	UnbindBoss();
	BoundBoss = Boss;
	Boss->OnDestroyed.AddUniqueDynamic(
		this,
		&AAI_REPlayerController::HandleBossDestroyed);
	if (IsValid(BossHUD))
	{
		BossHUD->BindBoss(Boss);
	}
}

void AAI_REPlayerController::UnbindBoss()
{
	if (AAIREBossEnemy* Boss = BoundBoss.Get())
	{
		Boss->OnDestroyed.RemoveDynamic(
			this,
			&AAI_REPlayerController::HandleBossDestroyed);
	}

	if (IsValid(BossHUD))
	{
		BossHUD->UnbindBoss();
	}
	BoundBoss.Reset();
}

void AAI_REPlayerController::HandleActorSpawned(AActor* SpawnedActor)
{
	AAIRECompanionCharacter* Companion = Cast<AAIRECompanionCharacter>(SpawnedActor);
	if (!BoundCompanion.IsValid() && IsValid(Companion) &&
		Companion->GetCompanionId() == TEXT("MAKO"))
	{
		BindCompanion(Companion);
	}

	AAIREBossEnemy* Boss = Cast<AAIREBossEnemy>(SpawnedActor);
	if (!BoundBoss.IsValid() && IsValid(Boss))
	{
		if (Boss->HasActorBegunPlay() && Boss->IsCombatTargetAlive())
		{
			BindBoss(Boss);
		}
		else if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(
					this,
					&AAI_REPlayerController::FindAndBindBoss));
		}
	}
}

void AAI_REPlayerController::HandleCompanionDestroyed(AActor* DestroyedActor)
{
	if (!BoundCompanion.IsValid() || DestroyedActor == BoundCompanion.Get())
	{
		UnbindCompanion();
	}
}

void AAI_REPlayerController::HandleBossDestroyed(AActor* DestroyedActor)
{
	if (!BoundBoss.IsValid() || DestroyedActor == BoundBoss.Get())
	{
		UnbindBoss();
		FindAndBindBoss();
	}
}

void AAI_REPlayerController::ShutdownLocalHUD()
{
	if (IsValid(QuitConfirmationWidget))
	{
		QuitConfirmationWidget->RemoveFromParent();
		QuitConfirmationWidget = nullptr;
		if (!bWasGamePausedBeforeQuitConfirmation)
		{
			UGameplayStatics::SetGamePaused(this, false);
		}
		bWasGamePausedBeforeQuitConfirmation = false;
	}

	ApplyLocalUIInputMode(EAIRELocalUIInputMode::Gameplay);
	UnbindPlayerTargetScanner();
	UnbindCompanion();
	UnbindBoss();

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

	if (IsValid(BossHUD))
	{
		BossHUD->RemoveFromParent();
		BossHUD = nullptr;
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

	if (IsValid(GameOverWidget))
	{
		GameOverWidget->RemoveFromParent();
		GameOverWidget = nullptr;
	}

	if (IsValid(TargetLockMarkerWidget))
	{
		TargetLockMarkerWidget->RemoveFromParent();
		TargetLockMarkerWidget = nullptr;
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
			|| NewMode == EAIRELocalUIInputMode::PolicySelection
			|| NewMode == EAIRELocalUIInputMode::QuitConfirmation
			|| NewMode == EAIRELocalUIInputMode::GameOver);
	}

	LocalUIInputMode = NewMode;
}

bool AAI_REPlayerController::IsCharacterModalUIOpen() const
{
	const AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(GetPawn());
	const UAIREInventoryUIWorldSubsystem* InventoryUISubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UAIREInventoryUIWorldSubsystem>()
		: nullptr;
	return (IsValid(PlayerCharacter)
			&& (PlayerCharacter->IsInventoryUIOpen()
				|| PlayerCharacter->IsCraftingUIOpen()))
		|| (IsValid(InventoryUISubsystem)
			&& InventoryUISubsystem->IsInventoryUIOpen());
}

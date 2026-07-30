#include "Chat/UI/AIREChatHUDWorldSubsystem.h"

#include "Chat/UI/AIREChatLogWidget.h"
#include "Chat/UI/AIREChatHUDWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIREChatHUD, Log, All);

namespace
{
	const FSoftClassPath ChatHUDClassPath(
		TEXT("/Game/Work/LMK/UI/Chat/WBP_AIREChatHUD.WBP_AIREChatHUD_C"));
	const FSoftClassPath ChatLogClassPath(
		TEXT("/Game/Work/LMK/UI/Chat/WBP_AIREChatLog.WBP_AIREChatLog_C"));
}

void UAIREChatHUDWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	InWorld.GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			CreateChatHUD();
		}));
}

void UAIREChatHUDWorldSubsystem::Deinitialize()
{
	UnregisterChatInput();
	if (IsValid(ChatHUD))
	{
		ChatHUD->CloseAllChatUI();
	}
	if (IsValid(ChatLog))
	{
		ChatLog->RemoveFromParent();
		ChatLog = nullptr;
	}
	if (IsValid(ChatHUD))
	{
		ChatHUD->RemoveFromParent();
		ChatHUD = nullptr;
	}
	Super::Deinitialize();
}

void UAIREChatHUDWorldSubsystem::CreateChatHUD()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return;
	}

	UClass* ChatHUDClass = ChatHUDClassPath.TryLoadClass<UAIREChatHUDWidget>();
	if (!IsValid(ChatHUDClass))
	{
		UE_LOG(LogAIREChatHUD, Warning, TEXT("Chat HUD class could not be loaded."));
		return;
	}

	TArray<UUserWidget*> ExistingWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this,
		ExistingWidgets,
		ChatHUDClass,
		false);
	if (!ExistingWidgets.IsEmpty())
	{
		ChatHUD = Cast<UAIREChatHUDWidget>(ExistingWidgets[0]);
	}
	else
	{
		APlayerController* PlayerController =
			World->GetFirstPlayerController();
		if (!IsValid(PlayerController)
			|| !PlayerController->IsLocalController())
		{
			return;
		}

		ChatHUD = CreateWidget<UAIREChatHUDWidget>(
			PlayerController,
			ChatHUDClass);
		if (!IsValid(ChatHUD))
		{
			UE_LOG(
				LogAIREChatHUD,
				Warning,
				TEXT("Chat HUD could not be created."));
			return;
		}

		ChatHUD->AddToViewport(100);
		UE_LOG(
			LogAIREChatHUD,
			Log,
			TEXT("Chat HUD created for the current world."));
	}

	APlayerController* PlayerController =
		World->GetFirstPlayerController();
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(ChatHUD))
	{
		return;
	}

	CreateChatLog(PlayerController);
	RegisterChatInput(PlayerController);
}

void UAIREChatHUDWorldSubsystem::CreateChatLog(
	APlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || !IsValid(ChatHUD))
	{
		return;
	}

	UClass* ChatLogClass =
		ChatLogClassPath.TryLoadClass<UAIREChatLogWidget>();
	if (!IsValid(ChatLogClass))
	{
		UE_LOG(
			LogAIREChatHUD,
			Verbose,
			TEXT("Chat log WBP is not available yet."));
		return;
	}

	ChatLog = CreateWidget<UAIREChatLogWidget>(
		PlayerController,
		ChatLogClass);
	if (!IsValid(ChatLog))
	{
		return;
	}

	ChatHUD->InitializeChatLogWidget(ChatLog);
	ChatLog->AddToViewport(110);
}

void UAIREChatHUDWorldSubsystem::RegisterChatInput(
	APlayerController* PlayerController)
{
	if (!IsValid(PlayerController)
		|| !IsValid(ChatHUD)
		|| IsValid(ChatInputComponent))
	{
		return;
	}

	ChatInputComponent = NewObject<UInputComponent>(
		PlayerController,
		TEXT("AIREChatHUDInputComponent"));
	if (!IsValid(ChatInputComponent))
	{
		return;
	}

	ChatInputComponent->Priority = 100;
	ChatInputComponent->bBlockInput = false;
	ChatInputComponent->RegisterComponent();

	FInputKeyBinding& EnterBinding = ChatInputComponent->BindKey(
		EKeys::Enter,
		IE_Pressed,
		this,
		&UAIREChatHUDWorldSubsystem::HandleEnterInput);
	EnterBinding.bConsumeInput = false;

	FInputKeyBinding& LogBinding = ChatInputComponent->BindKey(
		EKeys::L,
		IE_Pressed,
		this,
		&UAIREChatHUDWorldSubsystem::HandleLogInput);
	LogBinding.bConsumeInput = true;

	PlayerController->PushInputComponent(ChatInputComponent);
	InputPlayerController = PlayerController;
}

void UAIREChatHUDWorldSubsystem::UnregisterChatInput()
{
	if (IsValid(InputPlayerController)
		&& IsValid(ChatInputComponent))
	{
		InputPlayerController->PopInputComponent(ChatInputComponent);
	}
	if (IsValid(ChatInputComponent))
	{
		ChatInputComponent->DestroyComponent();
		ChatInputComponent = nullptr;
	}
	InputPlayerController = nullptr;
}

void UAIREChatHUDWorldSubsystem::HandleEnterInput()
{
	if (IsValid(ChatHUD))
	{
		ChatHUD->HandleGlobalEnterInput();
	}
}

void UAIREChatHUDWorldSubsystem::HandleLogInput()
{
	if (IsValid(ChatHUD))
	{
		ChatHUD->HandleGlobalLogInput();
	}
}

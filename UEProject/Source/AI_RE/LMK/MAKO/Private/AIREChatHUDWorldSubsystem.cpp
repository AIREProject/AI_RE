#include "AIREChatHUDWorldSubsystem.h"

#include "AIREChatHUDWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIREChatHUD, Log, All);

namespace
{
	const FSoftClassPath ChatHUDClassPath(
		TEXT("/Game/Work/LMK/UI/Chat/WBP_AIREChatHUD.WBP_AIREChatHUD_C"));
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
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	ChatHUD = CreateWidget<UAIREChatHUDWidget>(
		PlayerController,
		ChatHUDClass);
	if (!IsValid(ChatHUD))
	{
		UE_LOG(LogAIREChatHUD, Warning, TEXT("Chat HUD could not be created."));
		return;
	}

	ChatHUD->AddToViewport(100);
	UE_LOG(LogAIREChatHUD, Log, TEXT("Chat HUD created for the current world."));
}

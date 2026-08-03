#include "LocalAI/UI/AIRECompanionPolicyHUDWorldSubsystem.h"

#include "LocalAI/UI/AIRECompanionPolicyPanelWidget.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionPolicyHUD, Log, All);

namespace
{
	const FSoftClassPath PolicyPanelClassPath(
		TEXT("/Game/Work/LMK/UI/Policy/WBP_AIRECompanionPolicyPanel.WBP_AIRECompanionPolicyPanel_C"));
}

void UAIRECompanionPolicyHUDWorldSubsystem::OnWorldBeginPlay(
	UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	InWorld.GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			CreatePolicyHUD();
		}));
}

void UAIRECompanionPolicyHUDWorldSubsystem::Deinitialize()
{
	SetPolicyPanelOpen(false);
	UnregisterPolicyInput();
	if (IsValid(PolicyPanelWidget))
	{
		PolicyPanelWidget->RemoveFromParent();
		PolicyPanelWidget = nullptr;
	}
	Super::Deinitialize();
}

void UAIRECompanionPolicyHUDWorldSubsystem::CreatePolicyHUD()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	UClass* PolicyPanelClass =
		PolicyPanelClassPath.TryLoadClass<UAIRECompanionPolicyPanelWidget>();
	if (!IsValid(PolicyPanelClass))
	{
		UE_LOG(
			LogAIRECompanionPolicyHUD,
			Warning,
			TEXT("Companion policy panel WBP could not be loaded."));
		return;
	}

	PolicyPanelWidget = CreateWidget<UAIRECompanionPolicyPanelWidget>(
		PlayerController,
		PolicyPanelClass);
	if (!IsValid(PolicyPanelWidget))
	{
		UE_LOG(
			LogAIRECompanionPolicyHUD,
			Warning,
			TEXT("Companion policy panel widget could not be created."));
		return;
	}

	PolicyPanelWidget->AddToViewport(120);
	PolicyPanelWidget->SetPanelOpen(false);
	RegisterPolicyInput(PlayerController);
	UE_LOG(
		LogAIRECompanionPolicyHUD,
		Log,
		TEXT("Companion policy HUD created. HoldKey=P"));
}

void UAIRECompanionPolicyHUDWorldSubsystem::RegisterPolicyInput(
	APlayerController* PlayerController)
{
	if (!IsValid(PlayerController)
		|| !IsValid(PolicyPanelWidget)
		|| IsValid(PolicyInputComponent))
	{
		return;
	}

	PolicyInputComponent = NewObject<UInputComponent>(
		PlayerController,
		TEXT("AIRECompanionPolicyInputComponent"));
	if (!IsValid(PolicyInputComponent))
	{
		return;
	}

	PolicyInputComponent->Priority = 120;
	PolicyInputComponent->bBlockInput = false;
	PolicyInputComponent->RegisterComponent();
	FInputKeyBinding& PolicyPressedBinding = PolicyInputComponent->BindKey(
		EKeys::P,
		IE_Pressed,
		this,
		&UAIRECompanionPolicyHUDWorldSubsystem::HandlePolicyInputPressed);
	PolicyPressedBinding.bConsumeInput = true;
	FInputKeyBinding& PolicyReleasedBinding = PolicyInputComponent->BindKey(
		EKeys::P,
		IE_Released,
		this,
		&UAIRECompanionPolicyHUDWorldSubsystem::HandlePolicyInputReleased);
	PolicyReleasedBinding.bConsumeInput = true;
	PlayerController->PushInputComponent(PolicyInputComponent);
	InputPlayerController = PlayerController;
}

void UAIRECompanionPolicyHUDWorldSubsystem::UnregisterPolicyInput()
{
	if (IsValid(InputPlayerController) && IsValid(PolicyInputComponent))
	{
		InputPlayerController->PopInputComponent(PolicyInputComponent);
	}
	if (IsValid(PolicyInputComponent))
	{
		PolicyInputComponent->DestroyComponent();
		PolicyInputComponent = nullptr;
	}
	InputPlayerController = nullptr;
}

void UAIRECompanionPolicyHUDWorldSubsystem::HandlePolicyInputPressed()
{
	if (!IsValid(PolicyPanelWidget))
	{
		return;
	}

	SetPolicyPanelOpen(!PolicyPanelWidget->IsPanelOpen());
}

void UAIRECompanionPolicyHUDWorldSubsystem::HandlePolicyInputReleased()
{
	if (!IsValid(PolicyPanelWidget) || !PolicyPanelWidget->IsPanelOpen())
	{
		return;
	}

	PolicyPanelWidget->CommitPolicySelection();
	SetPolicyPanelOpen(false);
}

void UAIRECompanionPolicyHUDWorldSubsystem::SetPolicyPanelOpen(
	const bool bOpen)
{
	if (!IsValid(PolicyPanelWidget))
	{
		if (!bOpen)
		{
			RestoreGameInputMode();
		}
		return;
	}

	if (bOpen)
	{
		PolicyPanelWidget->BeginPolicySelection();
	}
	else
	{
		PolicyPanelWidget->SetPanelOpen(false);
	}
	if (!bOpen)
	{
		RestoreGameInputMode();
		return;
	}

	APlayerController* PlayerController =
		IsValid(InputPlayerController)
			? InputPlayerController.Get()
			: PolicyPanelWidget->GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		PolicyPanelWidget->SetPanelOpen(false);
		return;
	}

	if (!bOwnsInputSuppression)
	{
		bPreviousShowMouseCursor =
			PlayerController->ShouldShowMouseCursor();
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
		bOwnsInputSuppression = true;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth > 0 && ViewportHeight > 0)
	{
		PlayerController->SetMouseLocation(
			ViewportWidth / 2,
			ViewportHeight / 2);
	}
}

void UAIRECompanionPolicyHUDWorldSubsystem::RestoreGameInputMode()
{
	APlayerController* PlayerController = InputPlayerController.Get();
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
	PlayerController->SetShowMouseCursor(bPreviousShowMouseCursor);
}

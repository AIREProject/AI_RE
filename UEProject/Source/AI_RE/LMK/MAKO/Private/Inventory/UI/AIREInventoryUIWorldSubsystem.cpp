#include "Inventory/UI/AIREInventoryUIWorldSubsystem.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AIRESharedStorageActor.h"
#include "AI_RECharacter.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Components/InputComponent.h"
#include "Core/AIRECompanionCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Inventory/UI/AIRECompanionInventoryPanelWidget.h"
#include "Inventory/UI/AIREStorageInventoryPanelWidget.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIREInventoryUI, Log, All);

namespace
{
	const FSoftClassPath StoragePanelClassPath(
		TEXT("/Game/Work/LMK/UI/Inventory/WBP_AIREStorageInventoryPanel.WBP_AIREStorageInventoryPanel_C"));
	const FSoftClassPath CompanionPanelClassPath(
		TEXT("/Game/Work/LMK/UI/Inventory/WBP_AIRECompanionInventoryPanel.WBP_AIRECompanionInventoryPanel_C"));
}

void UAIREInventoryUIWorldSubsystem::Deinitialize()
{
	CloseInventoryUI();
	Super::Deinitialize();
}

void UAIREInventoryUIWorldSubsystem::OpenStorageInventory(
	AAIRESharedStorageActor* Storage,
	AActor* Interactor)
{
	if (!IsValid(Storage) || !IsValid(Interactor))
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController =
		IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	const AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(Interactor);
	UAI_REPlayerInventoryComponent* PlayerInventory =
		IsValid(PlayerCharacter)
			? PlayerCharacter->GetInventoryComponent().Get()
			: nullptr;
	UAIREGameplayInventorySubsystem* Inventory =
		IsValid(World) && IsValid(World->GetGameInstance())
			? World->GetGameInstance()
				->GetSubsystem<UAIREGameplayInventorySubsystem>()
			: nullptr;
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(PlayerInventory)
		|| !IsValid(Inventory))
	{
		return;
	}

	CloseInventoryUI();
	UClass* PanelClass = StoragePanelClassPath
		.TryLoadClass<UAIREStorageInventoryPanelWidget>();
	if (!IsValid(PanelClass))
	{
		UE_LOG(
			LogAIREInventoryUI,
			Warning,
			TEXT("Storage inventory panel WBP could not be loaded."));
		return;
	}

	StoragePanel = CreateWidget<UAIREStorageInventoryPanelWidget>(
		PlayerController,
		PanelClass);
	if (!IsValid(StoragePanel))
	{
		return;
	}

	StoragePanel->OnCloseRequested().AddUObject(
		this,
		&UAIREInventoryUIWorldSubsystem::CloseInventoryUI);
	StoragePanel->InitializePanel(Inventory, PlayerInventory);
	StoragePanel->AddToViewport(130);
	StoragePanel->SetPanelOpen(true);

	TrackedActor = Storage;
	InteractionActor = Interactor;
	Storage->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIREInventoryUIWorldSubsystem::HandleTrackedActorDestroyed);
	Interactor->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIREInventoryUIWorldSubsystem::HandleTrackedActorDestroyed);
	RegisterEscapeInput(PlayerController);
	ApplyInventoryInput(PlayerController);
}

void UAIREInventoryUIWorldSubsystem::OpenCompanionInventory(
	AAIRECompanionCharacter* Companion,
	AActor* Interactor)
{
	if (!IsValid(Companion) || !IsValid(Interactor))
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController =
		IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	UAIRECompanionInventoryComponent* Inventory =
		Companion->GetInventoryComponent();
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(Inventory))
	{
		return;
	}

	CloseInventoryUI();
	UClass* PanelClass = CompanionPanelClassPath
		.TryLoadClass<UAIRECompanionInventoryPanelWidget>();
	if (!IsValid(PanelClass))
	{
		UE_LOG(
			LogAIREInventoryUI,
			Warning,
			TEXT("Companion inventory panel WBP could not be loaded."));
		return;
	}

	CompanionPanel = CreateWidget<UAIRECompanionInventoryPanelWidget>(
		PlayerController,
		PanelClass);
	if (!IsValid(CompanionPanel))
	{
		return;
	}

	CompanionPanel->OnCloseRequested().AddUObject(
		this,
		&UAIREInventoryUIWorldSubsystem::CloseInventoryUI);
	CompanionPanel->InitializePanel(Inventory);
	CompanionPanel->AddToViewport(130);
	CompanionPanel->SetPanelOpen(true);

	TrackedActor = Companion;
	InteractionActor = Interactor;
	Companion->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIREInventoryUIWorldSubsystem::HandleTrackedActorDestroyed);
	Interactor->OnDestroyed.AddUniqueDynamic(
		this,
		&UAIREInventoryUIWorldSubsystem::HandleTrackedActorDestroyed);
	RegisterEscapeInput(PlayerController);
	ApplyInventoryInput(PlayerController);
}

void UAIREInventoryUIWorldSubsystem::CloseInventoryUI()
{
	if (TrackedActor.IsValid())
	{
		TrackedActor->OnDestroyed.RemoveDynamic(
			this,
			&UAIREInventoryUIWorldSubsystem::HandleTrackedActorDestroyed);
	}
	if (InteractionActor.IsValid())
	{
		InteractionActor->OnDestroyed.RemoveDynamic(
			this,
			&UAIREInventoryUIWorldSubsystem::HandleTrackedActorDestroyed);
	}
	TrackedActor.Reset();
	InteractionActor.Reset();

	if (IsValid(StoragePanel))
	{
		StoragePanel->OnCloseRequested().RemoveAll(this);
		StoragePanel->RemoveFromParent();
		StoragePanel = nullptr;
	}
	if (IsValid(CompanionPanel))
	{
		CompanionPanel->OnCloseRequested().RemoveAll(this);
		CompanionPanel->RemoveFromParent();
		CompanionPanel = nullptr;
	}

	RestoreGameInput();
	UnregisterEscapeInput();
}

bool UAIREInventoryUIWorldSubsystem::IsInventoryUIOpen() const
{
	return IsValid(StoragePanel) || IsValid(CompanionPanel);
}

void UAIREInventoryUIWorldSubsystem::RegisterEscapeInput(
	APlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || IsValid(EscapeInputComponent))
	{
		return;
	}

	EscapeInputComponent = NewObject<UInputComponent>(
		PlayerController,
		TEXT("AIREInventoryUIInputComponent"));
	if (!IsValid(EscapeInputComponent))
	{
		return;
	}

	EscapeInputComponent->Priority = 130;
	EscapeInputComponent->bBlockInput = false;
	EscapeInputComponent->RegisterComponent();
	FInputKeyBinding& EscapeBinding = EscapeInputComponent->BindKey(
		EKeys::Escape,
		IE_Pressed,
		this,
		&UAIREInventoryUIWorldSubsystem::HandleEscapeInput);
	EscapeBinding.bConsumeInput = true;
	PlayerController->PushInputComponent(EscapeInputComponent);
	InputPlayerController = PlayerController;
}

void UAIREInventoryUIWorldSubsystem::UnregisterEscapeInput()
{
	if (InputPlayerController.IsValid() && IsValid(EscapeInputComponent))
	{
		InputPlayerController->PopInputComponent(EscapeInputComponent);
	}
	if (IsValid(EscapeInputComponent))
	{
		EscapeInputComponent->DestroyComponent();
		EscapeInputComponent = nullptr;
	}
	InputPlayerController.Reset();
}

void UAIREInventoryUIWorldSubsystem::ApplyInventoryInput(
	APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
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
}

void UAIREInventoryUIWorldSubsystem::RestoreGameInput()
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

void UAIREInventoryUIWorldSubsystem::HandleEscapeInput()
{
	CloseInventoryUI();
}

void UAIREInventoryUIWorldSubsystem::HandleTrackedActorDestroyed(
	AActor* DestroyedActor)
{
	(void)DestroyedActor;
	CloseInventoryUI();
}

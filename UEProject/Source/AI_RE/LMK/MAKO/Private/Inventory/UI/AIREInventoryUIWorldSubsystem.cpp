#include "Inventory/UI/AIREInventoryUIWorldSubsystem.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AIRESharedStorageActor.h"
#include "AI_RECharacter.h"
#include "AI_REPlayerController.h"
#include "AI_REPlayerCombatComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Core/AIRECompanionCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Inventory/UI/AIRECompanionInventoryPanelWidget.h"
#include "Inventory/UI/AIREStorageInventoryPanelWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIREInventoryUI, Log, All);

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
	AAI_REPlayerController* PlayerController = IsValid(World)
		? Cast<AAI_REPlayerController>(World->GetFirstPlayerController())
		: nullptr;
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
	UClass* PanelClass = IsValid(PlayerController)
		? PlayerController->GetStorageInventoryPanelClass().Get()
		: nullptr;
	if (!IsValid(PanelClass))
	{
		UE_LOG(
			LogAIREInventoryUI,
			Warning,
			TEXT("Storage inventory panel class is not configured on the PlayerController."));
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
	AAI_REPlayerController* PlayerController = IsValid(World)
		? Cast<AAI_REPlayerController>(World->GetFirstPlayerController())
		: nullptr;
	const AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(Interactor);
	UAIRECompanionInventoryComponent* MakoInventory =
		Companion->GetInventoryComponent();
	UAI_REPlayerInventoryComponent* PlayerInventory =
		IsValid(PlayerCharacter)
			? PlayerCharacter->GetInventoryComponent().Get()
			: nullptr;
	UAI_REPlayerCombatComponent* PlayerCombat =
		IsValid(PlayerCharacter)
			? PlayerCharacter->GetCombatComponent().Get()
			: nullptr;
	UAIREGameplayInventorySubsystem* GameplayInventory =
		IsValid(World) && IsValid(World->GetGameInstance())
			? World->GetGameInstance()
				->GetSubsystem<UAIREGameplayInventorySubsystem>()
			: nullptr;
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(MakoInventory)
		|| !IsValid(PlayerInventory)
		|| !IsValid(PlayerCombat)
		|| !IsValid(GameplayInventory))
	{
		return;
	}

	CloseInventoryUI();
	UClass* PanelClass = IsValid(PlayerController)
		? PlayerController->GetCompanionInventoryPanelClass().Get()
		: nullptr;
	if (!IsValid(PanelClass))
	{
		UE_LOG(
			LogAIREInventoryUI,
			Warning,
			TEXT("Companion inventory panel class is not configured on the PlayerController."));
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
	CompanionPanel->InitializePanel(
		GameplayInventory,
		MakoInventory,
		PlayerInventory,
		PlayerCombat);
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
}

bool UAIREInventoryUIWorldSubsystem::IsInventoryUIOpen() const
{
	return IsValid(StoragePanel) || IsValid(CompanionPanel);
}

void UAIREInventoryUIWorldSubsystem::ApplyInventoryInput(
	APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	InputPlayerController = PlayerController;

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
	InputPlayerController.Reset();
}

void UAIREInventoryUIWorldSubsystem::HandleTrackedActorDestroyed(
	AActor* DestroyedActor)
{
	(void)DestroyedActor;
	CloseInventoryUI();
}

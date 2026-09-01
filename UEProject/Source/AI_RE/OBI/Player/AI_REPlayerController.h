// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AI_REPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UWidget;
class UAIREAggroSwapComponent;
class UAI_REMainUI;
class UAIREChatHUDWidget;
class UAIREChatLogWidget;
class UAIRECompanionPolicyPanelWidget;
class UAIRECompanionStatusWidget;
class UAIRECompanionInventoryPanelWidget;
class UAIREStorageInventoryPanelWidget;
class UAIREBossHUDWidget;
class UAIREGameOverWidget;
class UAIREQuitConfirmationWidget;
class UAIRETargetLockMarkerWidget;
class UAI_RETargetScannerComponent;
class AAIRECompanionCharacter;
class AAIREBossEnemy;
class AActor;
class APawn;

enum class EAIRELocalUIInputMode : uint8
{
	Gameplay,
	ChatInput,
	ChatLog,
	PolicySelection,
	QuitConfirmation,
	GameOver
};

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AAI_REPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAI_REPlayerController();

	UFUNCTION(BlueprintPure, Category = "AIRE|Combat|Aggro Swap")
	UAIREAggroSwapComponent* GetAggroSwapComponent() const;

	TSubclassOf<UAIREStorageInventoryPanelWidget>
		GetStorageInventoryPanelClass() const;
	TSubclassOf<UAIRECompanionInventoryPanelWidget>
		GetCompanionInventoryPanelClass() const;

	void HandlePlayerDeath();

	UFUNCTION(BlueprintCallable, Category = "AIRE|UI|Game Over")
	void ReturnToVillageAfterDeath();

	UFUNCTION(BlueprintCallable, Category = "AIRE|UI|Travel")
	void ConfirmExitGame();

	UFUNCTION(BlueprintCallable, Category = "AIRE|UI|Quit")
	void CancelExitConfirmation();

	bool CanProcessGameplayActionInput() const;
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Assign IA_AIREAggroSwap and map it to Q in an existing input mapping context. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Combat")
	TObjectPtr<UInputAction> AggroSwapAction;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	void HandleAggroSwapInput();
	void HandleEscapeInput();
	void HandleChatEnterInput();
	void HandleChatLogInput();
	void HandlePolicyInputStarted();
	void HandlePolicyInputCompleted();
	void HandlePolicyInputCanceled();
	void HandleChatUIStateChanged();
	void CreateLocalHUD();
	void TryShowDeathReturnGuidance();
	void RefreshPlayerHUD();
	void FindAndBindCompanion();
	void BindCompanion(AAIRECompanionCharacter* Companion);
	void UnbindCompanion();
	void FindAndBindBoss();
	void BindBoss(AAIREBossEnemy* Boss);
	void UnbindBoss();
	void HandleActorSpawned(AActor* SpawnedActor);
	void ShutdownLocalHUD();
	void OpenExitConfirmation();
	void BindPlayerTargetScanner(APawn* PlayerPawn);
	void UnbindPlayerTargetScanner();
	void ApplyLocalUIInputMode(
		EAIRELocalUIInputMode NewMode,
		UWidget* FocusTarget = nullptr);
	bool IsCharacterModalUIOpen() const;

	UFUNCTION()
	void HandlePlayerCombatStateChanged(bool bIsCombat, AActor* CombatTarget);

	UFUNCTION()
	void HandleCompanionDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleBossDestroyed(AActor* DestroyedActor);

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Input")
	TObjectPtr<UInputMappingContext> UserInterfaceMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Input")
	TObjectPtr<UInputAction> ChatEnterAction;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Input")
	TObjectPtr<UInputAction> ChatLogAction;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Input")
	TObjectPtr<UInputAction> CompanionPolicyAction;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Input")
	TObjectPtr<UInputAction> EscapeAction;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAI_REMainUI> MainHUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIRECompanionStatusWidget> CompanionStatusWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIREBossHUDWidget> BossHUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIREChatHUDWidget> ChatHUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIREChatLogWidget> ChatLogClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIRECompanionPolicyPanelWidget> CompanionPolicyWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIREStorageInventoryPanelWidget> StorageInventoryPanelClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIRECompanionInventoryPanelWidget> CompanionInventoryPanelClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIREGameOverWidget> GameOverWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIREQuitConfirmationWidget> QuitConfirmationWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|UI|Widgets")
	TSubclassOf<UAIRETargetLockMarkerWidget> TargetLockMarkerWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UAI_REMainUI> MainHUD;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionStatusWidget> CompanionStatusWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAIREBossHUDWidget> BossHUD;

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatHUDWidget> ChatHUD;

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatLogWidget> ChatLog;

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionPolicyPanelWidget> CompanionPolicyWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAIREGameOverWidget> GameOverWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAIREQuitConfirmationWidget> QuitConfirmationWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAIRETargetLockMarkerWidget> TargetLockMarkerWidget;

	TWeakObjectPtr<AAIRECompanionCharacter> BoundCompanion;
	TWeakObjectPtr<AAIREBossEnemy> BoundBoss;
	TWeakObjectPtr<UAI_RETargetScannerComponent> BoundTargetScanner;
	FDelegateHandle ActorSpawnedDelegateHandle;
	EAIRELocalUIInputMode LocalUIInputMode =
		EAIRELocalUIInputMode::Gameplay;
	bool bOwnsUIInputSuppression = false;
	bool bPreviousShowMouseCursor = false;
	bool bWasGamePausedBeforeQuitConfirmation = false;
	FTimerHandle DeathReturnGuidanceTimer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIREAggroSwapComponent> AggroSwapComponent;

};

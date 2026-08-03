#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIRECompanionPolicyHUDWorldSubsystem.generated.h"

class APlayerController;
class UAIRECompanionPolicyPanelWidget;
class UInputComponent;

UCLASS()
class AI_RE_API UAIRECompanionPolicyHUDWorldSubsystem
	: public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void CreatePolicyHUD();
	void RegisterPolicyInput(APlayerController* PlayerController);
	void UnregisterPolicyInput();
	void HandlePolicyInputPressed();
	void HandlePolicyInputReleased();
	void SetPolicyPanelOpen(bool bOpen);
	void RestoreGameInputMode();

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionPolicyPanelWidget> PolicyPanelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> PolicyInputComponent;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> InputPlayerController;

	bool bOwnsInputSuppression = false;
	bool bPreviousShowMouseCursor = false;
};

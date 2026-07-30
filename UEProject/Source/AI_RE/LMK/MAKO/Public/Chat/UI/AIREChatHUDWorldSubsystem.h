#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIREChatHUDWorldSubsystem.generated.h"

class UAIREChatHUDWidget;
class UAIREChatLogWidget;
class APlayerController;
class UInputComponent;

UCLASS()
class AI_RE_API UAIREChatHUDWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void CreateChatHUD();
	void CreateChatLog(APlayerController* PlayerController);
	void RegisterChatInput(APlayerController* PlayerController);
	void UnregisterChatInput();
	void HandleEnterInput();
	void HandleLogInput();

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatHUDWidget> ChatHUD;

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatLogWidget> ChatLog;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> ChatInputComponent;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> InputPlayerController;
};

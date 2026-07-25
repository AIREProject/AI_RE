#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIREChatHUDWorldSubsystem.generated.h"

class UAIREChatHUDWidget;

UCLASS()
class AI_RE_API UAIREChatHUDWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void CreateChatHUD();

	UPROPERTY(Transient)
	TObjectPtr<UAIREChatHUDWidget> ChatHUD;
};

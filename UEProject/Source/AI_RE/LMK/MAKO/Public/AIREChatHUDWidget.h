#pragma once

#include "CoreMinimal.h"
#include "AIREChatTypes.h"
#include "Blueprint/UserWidget.h"
#include "AIREChatHUDWidget.generated.h"

class UAIRECompanionChatComponent;
class UAIREResponseStackWidget;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREChatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	void InitializeChatRuntime(UAIRECompanionChatComponent* InChatComponent);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	bool SubmitPlayerMessage(const FString& UserMessage);

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Chat")
	TArray<FString> GetVisibleResponseTexts() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Chat", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxVisibleResponses = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AIRE|Companion|Chat", meta = (ClampMin = "0.1"))
	float ResponseDisplaySeconds = 8.0f;

private:
	struct FVisibleResponse
	{
		int64 Id = 0;
		FString Text;
		FTimerHandle ExpirationHandle;
	};

	UFUNCTION()
	void HandleRuntimeChatResponse(const FAIREChatResult& Result);

	UFUNCTION()
	void HandleRuntimeChatFailure(const FAIREChatError& Error);

	void RemoveResponse(int64 ResponseId);
	void RemoveOldestResponse();
	void RefreshVisibleResponseTexts();
	void UnbindChatComponent();
	void ClearResponseTimers();

	UPROPERTY(Transient)
	TObjectPtr<UAIRECompanionChatComponent> RuntimeChatComponent;

	UPROPERTY(Transient)
	TArray<FString> VisibleResponseTexts;

	UPROPERTY(Transient)
	TObjectPtr<UAIREResponseStackWidget> RuntimeResponseStack;

	TArray<FVisibleResponse> VisibleResponses;
	int64 NextResponseId = 1;
};

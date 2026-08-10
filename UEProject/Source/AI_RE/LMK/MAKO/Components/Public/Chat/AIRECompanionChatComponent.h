#pragma once

#include "CoreMinimal.h"
#include "Chat/Auth/AIREGameClientTokenProvider.h"
#include "Chat/Contracts/AIREChatTypes.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "AIRECompanionChatComponent.generated.h"

class IWebSocket;

UCLASS(ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionChatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionChatComponent();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Chat")
	bool ConfigureInGameContext(const FAIREInGameChatContext& InContext);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Chat")
	bool SendPlayerMessage(const FString& UserMessage);

	UFUNCTION(
		BlueprintCallable,
		Category = "AIRE|Chat",
		meta = (DeprecatedFunction, DeprecationMessage = "Chat retries are not supported. Send a new message instead."))
	bool RetryLastRequest();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Chat")
	void CancelActiveRequest();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Chat")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Chat|Testing")
	void SetFakeScenario(EAIREChatFakeScenario InScenario);

	UFUNCTION(
		BlueprintCallable,
		Category = "AIRE|Chat|Credentials",
		meta = (DeprecatedFunction, DeprecationMessage = "AIRE_GAME uses no stored client credential."))
	bool ClearStoredGameClientCredential();

	UFUNCTION(BlueprintPure, Category = "AIRE|Chat")
	EAIREChatConnectionState GetConnectionState() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Chat")
	EAIREChatRequestState GetRequestState() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Chat")
	bool HasInGameContext() const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Chat")
	FAIREChatConnectionStateChanged OnConnectionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Chat")
	FAIREChatRequestStateChanged OnRequestStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Chat")
	FAIREChatResponseReceived OnResponseReceived;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Chat")
	FAIREChatRequestFailed OnRequestFailed;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "AIRE|Chat|Credentials",
		meta = (DeprecatedProperty, DeprecationMessage = "AIRE_GAME uses no token provider."))
	TScriptInterface<IAIREGameClientTokenProvider> TokenProvider;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool ValidateContext(const FAIREInGameChatContext& Context, FString& OutError) const;
	bool BeginActiveRequest();
	void BeginSelectedTransport(const FString& Token);
	void ConnectWebSocket(const FString& Token);
	void HandleWebSocketConnected();
	void HandleWebSocketConnectionError(const FString& Error);
	void HandleWebSocketClosed(
		int32 StatusCode,
		const FString& Reason,
		bool bWasClean);
	void HandleWebSocketMessage(const FString& Message);
	void SendActiveWebSocketFrame();
	void SendActiveHttpRequest(const FString& Token);
	void HandleHttpChatComplete(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		uint64 RequestGeneration);
	void BeginFakeRequest();
	void CompleteFakeRequest(uint64 RequestGeneration);
	void HandleParsedResponse(const FAIREChatResult& Result);
	void HandleParsedError(const FAIREChatError& Error);
	void HandleRequestFailure(
		const FString& Code,
		const FString& Message,
		bool bRetryable);
	void HandleConnectionTimeout();
	void HandleResponseTimeout();
	void SetConnectionState(EAIREChatConnectionState NewState);
	void SetRequestState(EAIREChatRequestState NewState);
	void StartConnectionTimeout();
	void StartResponseTimeout();
	void ClearConnectionTimeout();
	void ClearResponseTimeout();
	void ResetActiveRequest();
	void ShutdownRequests();

	UPROPERTY(Transient)
	FAIREInGameChatContext ChatContext;

	UPROPERTY(Transient)
	bool bHasChatContext = false;

	UPROPERTY(Transient)
	EAIREChatConnectionState ConnectionState = EAIREChatConnectionState::Disconnected;

	UPROPERTY(Transient)
	EAIREChatRequestState RequestState = EAIREChatRequestState::Idle;

	UPROPERTY(Transient)
	EAIREChatFakeScenario FakeScenario = EAIREChatFakeScenario::Disabled;

	TSharedPtr<IWebSocket> WebSocket;
	FHttpRequestPtr HttpChatRequest;
	FTimerHandle ConnectionTimeoutHandle;
	FTimerHandle ResponseTimeoutHandle;
	FTimerHandle FakeResponseHandle;
	FString SessionId;
	FString ActiveRequestId;
	FString ActiveMessageId;
	FString ActiveWebSocketFrame;
	FString ActiveHttpBody;
	uint64 Generation = 0;
	bool bIsEndingPlay = false;
	bool bIsCancellingRequest = false;
};

#pragma once

#include "CoreMinimal.h"
#include "AIREGameClientTokenProvider.h"
#include "AIREChatTypes.h"
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

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	bool ConfigureInGameContext(const FAIREInGameChatContext& InContext);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	bool SendPlayerMessage(const FString& UserMessage);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	bool RetryLastRequest();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	void CancelActiveRequest();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat|Testing")
	void SetFakeScenario(EAIREChatFakeScenario InScenario);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Chat|Credentials")
	bool ClearStoredGameClientCredential();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Chat")
	EAIREChatConnectionState GetConnectionState() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Chat")
	EAIREChatRequestState GetRequestState() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Chat")
	bool HasInGameContext() const;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Chat")
	FAIREChatConnectionStateChanged OnConnectionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Chat")
	FAIREChatRequestStateChanged OnRequestStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Chat")
	FAIREChatResponseReceived OnResponseReceived;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Chat")
	FAIREChatRequestFailed OnRequestFailed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Chat|Credentials")
	TScriptInterface<IAIREGameClientTokenProvider> TokenProvider;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool ValidateContext(const FAIREInGameChatContext& Context, FString& OutError) const;
	bool BeginActiveRequest();
	void AcquireCredentialAndBeginRequest();
	void RegisterGameClient();
	void HandleRegistrationComplete(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		uint64 RequestGeneration);
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
		bool bRetryable,
		bool bPreserveRequest);
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
	bool ResolveToken(FString& OutToken) const;
	FString GetCredentialTarget() const;
	FString LoadOrCreateRegistrationRequestId();
	FString GetCompanionId() const;

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
	FHttpRequestPtr RegistrationRequest;
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
};

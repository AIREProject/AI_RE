#include "AIRECompanionChatComponent.h"

#include "AIREChatJsonAdapter.h"
#include "AIREChatSettings.h"
#include "AIRECompanionCharacter.h"
#include "AIREGameClientCredentialStore.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "IWebSocket.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "WebSocketsModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionChat, Log, All);

namespace
{
	constexpr int32 NormalClosureCode = 1000;
	constexpr int32 PolicyViolationCode = 1008;
	constexpr uint64 MaxWebSocketMessageBytes = 262144;
	constexpr TCHAR BootstrapTokenEnvironmentVariable[] = TEXT("AIRE_GAME_BOOTSTRAP_TOKEN");
	constexpr TCHAR DeviceTokenEnvironmentVariable[] = TEXT("AIRE_GAME_DEVICE_TOKEN");

	FString NewStableId(const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT("%s-%s"),
			Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	}

	FString JoinUrl(const FString& BaseUrl, const FString& Path)
	{
		FString Result = BaseUrl;
		Result.RemoveFromEnd(TEXT("/"));
		if (!Path.StartsWith(TEXT("/")))
		{
			Result += TEXT("/");
		}
		Result += Path;
		return Result;
	}

	FString ToWebSocketUrl(const FString& HttpUrl)
	{
		if (HttpUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
		{
			return TEXT("wss://") + HttpUrl.RightChop(8);
		}
		if (HttpUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase))
		{
			return TEXT("ws://") + HttpUrl.RightChop(7);
		}
		return HttpUrl;
	}

	bool SerializeObject(const TSharedRef<FJsonObject>& Object, FString& OutJson)
	{
		OutJson.Reset();
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object, Writer);
	}

	bool DeserializeObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	FString GetIdentityMetadataPath()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("AI_RE"),
			TEXT("GameClientIdentity.json"));
	}

	FString BuildFakeSuccessFrame(const FString& RequestId)
	{
		const TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
		Metadata->SetStringField(TEXT("provider"), TEXT("fake"));
		Metadata->SetStringField(TEXT("model_version"), TEXT("fake-v1"));
		Metadata->SetStringField(TEXT("prompt_version"), TEXT("fake-v1"));

		const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetNumberField(TEXT("schema_version"), 1);
		Payload->SetStringField(TEXT("request_id"), RequestId);
		Payload->SetStringField(TEXT("response_id"), NewStableId(TEXT("response")));
		Payload->SetStringField(TEXT("interaction_mode"), TEXT("InGame"));
		Payload->SetStringField(TEXT("display_text"), TEXT("Fake Companion response."));
		Payload->SetArrayField(TEXT("command_candidates"), TArray<TSharedPtr<FJsonValue>>());
		Payload->SetArrayField(TEXT("memory_candidates"), TArray<TSharedPtr<FJsonValue>>());
		Payload->SetObjectField(TEXT("ai_metadata"), Metadata);

		const TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
		Frame->SetStringField(TEXT("type"), TEXT("chat_response"));
		Frame->SetObjectField(TEXT("payload"), Payload);
		FString Json;
		SerializeObject(Frame, Json);
		return Json;
	}

	FString BuildFakeErrorFrame(const FString& RequestId)
	{
		const TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("code"), TEXT("AIServiceUnavailable"));
		Error->SetStringField(TEXT("message"), TEXT("Fake AI service is unavailable."));
		Error->SetBoolField(TEXT("retryable"), true);
		Error->SetObjectField(TEXT("details"), MakeShared<FJsonObject>());

		const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetNumberField(TEXT("schema_version"), 1);
		Payload->SetStringField(TEXT("request_id"), RequestId);
		Payload->SetObjectField(TEXT("error"), Error);

		const TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
		Frame->SetStringField(TEXT("type"), TEXT("error"));
		Frame->SetObjectField(TEXT("payload"), Payload);
		FString Json;
		SerializeObject(Frame, Json);
		return Json;
	}
}

UAIRECompanionChatComponent::UAIRECompanionChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIRECompanionChatComponent::ConfigureInGameContext(const FAIREInGameChatContext& InContext)
{
	FString Error;
	if (!ValidateContext(InContext, Error))
	{
		return false;
	}

	ChatContext = InContext;
	bHasChatContext = true;
	return true;
}

bool UAIRECompanionChatComponent::SendPlayerMessage(const FString& UserMessage)
{
	if (bIsEndingPlay
		|| RequestState == EAIREChatRequestState::Sending
		|| RequestState == EAIREChatRequestState::RetryableFailed)
	{
		return false;
	}

	FString ContextError;
	if (!bHasChatContext || !ValidateContext(ChatContext, ContextError))
	{
		HandleRequestFailure(
			TEXT("MissingContext"),
			TEXT("A valid InGame Chat context is required."),
			false,
			false);
		return false;
	}

	++Generation;
	ActiveRequestId = NewStableId(TEXT("request"));
	ActiveMessageId = NewStableId(TEXT("message"));
	FString SerializationError;
	if (!FAIREChatJsonAdapter::BuildInGameRequest(
		ChatContext,
		GetCompanionId(),
		SessionId,
		ActiveRequestId,
		ActiveMessageId,
		UserMessage,
		ActiveHttpBody,
		ActiveWebSocketFrame,
		SerializationError))
	{
		HandleRequestFailure(
			TEXT("InvalidRequest"),
			SerializationError,
			false,
			false);
		return false;
	}

	SetRequestState(EAIREChatRequestState::Sending);
	return BeginActiveRequest();
}

bool UAIRECompanionChatComponent::RetryLastRequest()
{
	if (bIsEndingPlay
		|| RequestState != EAIREChatRequestState::RetryableFailed
		|| ActiveRequestId.IsEmpty()
		|| ActiveWebSocketFrame.IsEmpty()
		|| ActiveHttpBody.IsEmpty())
	{
		return false;
	}

	++Generation;
	SetRequestState(EAIREChatRequestState::Sending);
	return BeginActiveRequest();
}

void UAIRECompanionChatComponent::CancelActiveRequest()
{
	if (RequestState != EAIREChatRequestState::Sending
		&& RequestState != EAIREChatRequestState::RetryableFailed)
	{
		return;
	}

	++Generation;
	ClearConnectionTimeout();
	ClearResponseTimeout();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FakeResponseHandle);
	}
	if (RegistrationRequest.IsValid())
	{
		RegistrationRequest->CancelRequest();
		RegistrationRequest.Reset();
	}
	if (HttpChatRequest.IsValid())
	{
		HttpChatRequest->CancelRequest();
		HttpChatRequest.Reset();
	}
	if (ConnectionState == EAIREChatConnectionState::Connecting)
	{
		SetRequestState(EAIREChatRequestState::Cancelled);
		Disconnect();
	}

	ResetActiveRequest();
	SetRequestState(EAIREChatRequestState::Cancelled);
}

void UAIRECompanionChatComponent::Disconnect()
{
	ClearConnectionTimeout();
	ClearResponseTimeout();

	const TSharedPtr<IWebSocket> SocketToClose = WebSocket;
	WebSocket.Reset();
	if (SocketToClose.IsValid())
	{
		SocketToClose->OnConnected().Clear();
		SocketToClose->OnConnectionError().Clear();
		SocketToClose->OnClosed().Clear();
		SocketToClose->OnMessage().Clear();
		if (SocketToClose->IsConnected())
		{
			SocketToClose->Close(NormalClosureCode, TEXT("Client disconnect"));
		}
	}

	SetConnectionState(EAIREChatConnectionState::Disconnected);
	if (!bIsEndingPlay && RequestState == EAIREChatRequestState::Sending)
	{
		HandleRequestFailure(
			TEXT("ConnectionClosed"),
			TEXT("Chat connection was closed before a response was confirmed."),
			true,
			true);
	}
}

void UAIRECompanionChatComponent::SetFakeScenario(const EAIREChatFakeScenario InScenario)
{
	if (RequestState == EAIREChatRequestState::Sending)
	{
		return;
	}
	FakeScenario = InScenario;
}

bool UAIRECompanionChatComponent::ClearStoredGameClientCredential()
{
	return FAIREGameClientCredentialStore::Remove(GetCredentialTarget());
}

EAIREChatConnectionState UAIRECompanionChatComponent::GetConnectionState() const
{
	return ConnectionState;
}

EAIREChatRequestState UAIRECompanionChatComponent::GetRequestState() const
{
	return RequestState;
}

bool UAIRECompanionChatComponent::HasInGameContext() const
{
	return bHasChatContext;
}

void UAIRECompanionChatComponent::BeginPlay()
{
	Super::BeginPlay();
	SessionId = NewStableId(TEXT("session"));
}

void UAIRECompanionChatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bIsEndingPlay = true;
	ShutdownRequests();
	Super::EndPlay(EndPlayReason);
}

bool UAIRECompanionChatComponent::ValidateContext(
	const FAIREInGameChatContext& Context,
	FString& OutError) const
{
	if (!FAIREChatJsonAdapter::IsStableId(Context.SaveSlotId)
		|| Context.Day < 0
		|| !FMath::IsFinite(Context.Hour)
		|| Context.Hour < 0.0f
		|| Context.Hour >= 24.0f)
	{
		OutError = TEXT("InGame Chat context is invalid.");
		return false;
	}
	OutError.Reset();
	return true;
}

bool UAIRECompanionChatComponent::BeginActiveRequest()
{
	if (FakeScenario != EAIREChatFakeScenario::Disabled)
	{
		BeginFakeRequest();
		return true;
	}

	AcquireCredentialAndBeginRequest();
	return true;
}

void UAIRECompanionChatComponent::AcquireCredentialAndBeginRequest()
{
	FString Token;
	if (ResolveToken(Token))
	{
		BeginSelectedTransport(Token);
		return;
	}

	if (TokenProvider.GetObject() != nullptr)
	{
		HandleRequestFailure(
			TEXT("MissingCredential"),
			TEXT("The configured GameClient Token Provider returned no valid credential."),
			false,
			true);
		return;
	}

	RegisterGameClient();
}

void UAIRECompanionChatComponent::RegisterGameClient()
{
	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	const FString BootstrapToken = FPlatformMisc::GetEnvironmentVariable(BootstrapTokenEnvironmentVariable);
	if (!IsValid(Settings) || BootstrapToken.IsEmpty())
	{
		HandleRequestFailure(
			TEXT("MissingBootstrapCredential"),
			TEXT("GameClient registration requires a runtime bootstrap credential."),
			false,
			true);
		return;
	}

	const FString RegistrationRequestId = LoadOrCreateRegistrationRequestId();
	if (!FAIREChatJsonAdapter::IsStableId(RegistrationRequestId))
	{
		HandleRequestFailure(
			TEXT("CredentialMetadataUnavailable"),
			TEXT("GameClient registration metadata is unavailable."),
			false,
			true);
		return;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("schema_version"), 1);
	Body->SetStringField(TEXT("request_id"), RegistrationRequestId);
	FString SerializedBody;
	if (!SerializeObject(Body, SerializedBody))
	{
		HandleRequestFailure(
			TEXT("RegistrationSerializationFailed"),
			TEXT("Could not serialize GameClient registration."),
			false,
			true);
		return;
	}

	SetConnectionState(EAIREChatConnectionState::Connecting);
	UE_LOG(LogAIRECompanionChat, Log, TEXT("Starting GameClient registration."));
	const uint64 RequestGeneration = Generation;
	RegistrationRequest = FHttpModule::Get().CreateRequest();
	RegistrationRequest->SetURL(JoinUrl(Settings->BackendBaseUrl, Settings->RegisterGamePath));
	RegistrationRequest->SetVerb(TEXT("POST"));
	RegistrationRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + BootstrapToken);
	RegistrationRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	RegistrationRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	RegistrationRequest->SetHeader(TEXT("X-Request-ID"), RegistrationRequestId);
	RegistrationRequest->SetContentAsString(SerializedBody);
	RegistrationRequest->SetTimeout(Settings->ConnectionTimeoutSeconds);
	RegistrationRequest->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, RequestGeneration](
			FHttpRequestPtr Request,
			FHttpResponsePtr Response,
			const bool bWasSuccessful)
		{
			HandleRegistrationComplete(
				Request,
				Response,
				bWasSuccessful,
				RequestGeneration);
		});
	if (!RegistrationRequest->ProcessRequest())
	{
		RegistrationRequest.Reset();
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		HandleRequestFailure(
			TEXT("RegistrationUnavailable"),
			TEXT("Could not start GameClient registration."),
			true,
			true);
	}
}

void UAIRECompanionChatComponent::HandleRegistrationComplete(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	const uint64 RequestGeneration)
{
	if (bIsEndingPlay || RequestGeneration != Generation || Request != RegistrationRequest)
	{
		return;
	}
	RegistrationRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		UE_LOG(
			LogAIRECompanionChat,
			Warning,
			TEXT("GameClient registration failed. HttpStatus=%d"),
			ResponseCode);
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		const bool bTimedOut = Request.IsValid()
			&& Request->GetFailureReason() == EHttpFailureReason::TimedOut;
		HandleRequestFailure(
			bTimedOut ? TEXT("RegistrationTimeout") : TEXT("RegistrationFailed"),
			bTimedOut
				? TEXT("GameClient registration timed out.")
				: TEXT("GameClient registration failed."),
			bTimedOut,
			true);
		return;
	}

	TSharedPtr<FJsonObject> Body;
	FString Token;
	if (!DeserializeObject(Response->GetContentAsString(), Body)
		|| !Body->TryGetStringField(TEXT("device_token"), Token)
		|| Token.Len() < 32
		|| Token.Len() > 512
		|| !FAIREGameClientCredentialStore::Save(GetCredentialTarget(), Token))
	{
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		HandleRequestFailure(
			TEXT("CredentialStorageFailed"),
			TEXT("The GameClient credential could not be validated or stored."),
			false,
			true);
		return;
	}

	UE_LOG(LogAIRECompanionChat, Log, TEXT("GameClient registration succeeded."));
	BeginSelectedTransport(Token);
}

void UAIRECompanionChatComponent::BeginSelectedTransport(const FString& Token)
{
	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	if (!IsValid(Settings))
	{
		HandleRequestFailure(
			TEXT("MissingSettings"),
			TEXT("Chat settings are unavailable."),
			false,
			true);
		return;
	}

	if (Settings->TransportMode == EAIREChatTransportMode::Http)
	{
		SendActiveHttpRequest(Token);
		return;
	}

	ConnectWebSocket(Token);
}

void UAIRECompanionChatComponent::ConnectWebSocket(const FString& Token)
{
	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		SetConnectionState(EAIREChatConnectionState::Connected);
		SendActiveWebSocketFrame();
		return;
	}

	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	if (!IsValid(Settings))
	{
		HandleRequestFailure(TEXT("MissingSettings"), TEXT("Chat settings are unavailable."), false, true);
		return;
	}

	SetConnectionState(EAIREChatConnectionState::Connecting);
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), TEXT("Bearer ") + Token);
	const FString Url = ToWebSocketUrl(JoinUrl(Settings->BackendBaseUrl, Settings->GameWebSocketPath));
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url, FString(), Headers);
	WebSocket->SetTextMessageMemoryLimit(MaxWebSocketMessageBytes);
	const TWeakPtr<IWebSocket> CreatedSocket = WebSocket;
	WebSocket->OnConnected().AddWeakLambda(this, [this, CreatedSocket]()
	{
		if (WebSocket == CreatedSocket.Pin())
		{
			HandleWebSocketConnected();
		}
	});
	WebSocket->OnConnectionError().AddWeakLambda(this, [this, CreatedSocket](const FString& Error)
	{
		if (WebSocket == CreatedSocket.Pin())
		{
			HandleWebSocketConnectionError(Error);
		}
	});
	WebSocket->OnClosed().AddWeakLambda(
		this,
		[this, CreatedSocket](const int32 StatusCode, const FString& Reason, const bool bWasClean)
		{
			if (WebSocket == CreatedSocket.Pin())
			{
				HandleWebSocketClosed(StatusCode, Reason, bWasClean);
			}
		});
	WebSocket->OnMessage().AddWeakLambda(this, [this, CreatedSocket](const FString& Message)
	{
		if (WebSocket == CreatedSocket.Pin())
		{
			HandleWebSocketMessage(Message);
		}
	});
	StartConnectionTimeout();
	WebSocket->Connect();
}

void UAIRECompanionChatComponent::HandleWebSocketConnected()
{
	if (bIsEndingPlay || !WebSocket.IsValid())
	{
		return;
	}
	ClearConnectionTimeout();
	SetConnectionState(EAIREChatConnectionState::Connected);
	UE_LOG(LogAIRECompanionChat, Log, TEXT("Connected to the GameClient Chat WebSocket."));
	if (RequestState == EAIREChatRequestState::Sending)
	{
		SendActiveWebSocketFrame();
	}
}

void UAIRECompanionChatComponent::HandleWebSocketConnectionError(const FString& Error)
{
	(void)Error;
	if (bIsEndingPlay)
	{
		return;
	}
	ClearConnectionTimeout();
	WebSocket.Reset();
	SetConnectionState(EAIREChatConnectionState::Disconnected);
	UE_LOG(LogAIRECompanionChat, Warning, TEXT("GameClient Chat WebSocket connection failed."));
	if (RequestState == EAIREChatRequestState::Sending)
	{
		HandleRequestFailure(
			TEXT("ConnectionFailed"),
			TEXT("Could not connect to the Chat service."),
			true,
			true);
	}
}

void UAIRECompanionChatComponent::HandleWebSocketClosed(
	const int32 StatusCode,
	const FString& Reason,
	const bool bWasClean)
{
	(void)Reason;
	(void)bWasClean;
	if (bIsEndingPlay)
	{
		return;
	}

	ClearConnectionTimeout();
	ClearResponseTimeout();
	WebSocket.Reset();
	SetConnectionState(EAIREChatConnectionState::Disconnected);
	UE_LOG(
		LogAIRECompanionChat,
		Warning,
		TEXT("GameClient Chat WebSocket closed. StatusCode=%d"),
		StatusCode);
	if (StatusCode == PolicyViolationCode && TokenProvider.GetObject() != nullptr)
	{
		IAIREGameClientTokenProvider::Execute_HandleGameClientTokenRejected(TokenProvider.GetObject());
	}
	if (RequestState == EAIREChatRequestState::Sending)
	{
		HandleRequestFailure(
			StatusCode == PolicyViolationCode ? TEXT("CredentialRejected") : TEXT("ConnectionClosed"),
			StatusCode == PolicyViolationCode
				? TEXT("The GameClient credential was rejected.")
				: TEXT("Chat connection closed before a response was confirmed."),
			StatusCode != PolicyViolationCode,
			true);
	}
}

void UAIRECompanionChatComponent::HandleWebSocketMessage(const FString& Message)
{
	if (bIsEndingPlay || RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}

	const FAIREParsedChatFrame Frame =
		FAIREChatJsonAdapter::ParseWebSocketFrame(Message, ActiveRequestId);
	switch (Frame.Kind)
	{
	case EAIREParsedChatFrameKind::Ignored:
		return;
	case EAIREParsedChatFrameKind::Response:
		HandleParsedResponse(Frame.Result);
		return;
	case EAIREParsedChatFrameKind::Error:
		HandleParsedError(Frame.Error);
		return;
	case EAIREParsedChatFrameKind::Invalid:
	default:
		HandleRequestFailure(
			Frame.Error.Code,
			Frame.Error.Message,
			false,
			false);
	}
}

void UAIRECompanionChatComponent::SendActiveWebSocketFrame()
{
	if (!WebSocket.IsValid()
		|| !WebSocket->IsConnected()
		|| RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}
	WebSocket->Send(ActiveWebSocketFrame);
	StartResponseTimeout();
}

void UAIRECompanionChatComponent::SendActiveHttpRequest(const FString& Token)
{
	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	if (!IsValid(Settings))
	{
		HandleRequestFailure(TEXT("MissingSettings"), TEXT("Chat settings are unavailable."), false, true);
		return;
	}

	SetConnectionState(EAIREChatConnectionState::Connecting);
	const uint64 RequestGeneration = Generation;
	HttpChatRequest = FHttpModule::Get().CreateRequest();
	HttpChatRequest->SetURL(JoinUrl(Settings->BackendBaseUrl, Settings->HttpChatPath));
	HttpChatRequest->SetVerb(TEXT("POST"));
	HttpChatRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
	HttpChatRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpChatRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	HttpChatRequest->SetHeader(TEXT("X-Request-ID"), ActiveRequestId);
	HttpChatRequest->SetContentAsString(ActiveHttpBody);
	HttpChatRequest->SetTimeout(Settings->ResponseTimeoutSeconds);
	HttpChatRequest->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, RequestGeneration](
			FHttpRequestPtr Request,
			FHttpResponsePtr Response,
			const bool bWasSuccessful)
		{
			HandleHttpChatComplete(Request, Response, bWasSuccessful, RequestGeneration);
		});
	if (!HttpChatRequest->ProcessRequest())
	{
		HttpChatRequest.Reset();
		HandleRequestFailure(
			TEXT("ConnectionFailed"),
			TEXT("Could not start the HTTP Chat request."),
			true,
			true);
	}
}

void UAIRECompanionChatComponent::HandleHttpChatComplete(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	const uint64 RequestGeneration)
{
	if (bIsEndingPlay || RequestGeneration != Generation || Request != HttpChatRequest)
	{
		return;
	}
	HttpChatRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		const bool bTimedOut = Request.IsValid()
			&& Request->GetFailureReason() == EHttpFailureReason::TimedOut;
		HandleRequestFailure(
			bTimedOut ? TEXT("RequestTimeout") : TEXT("ConnectionFailed"),
			bTimedOut
				? TEXT("The HTTP Chat request timed out.")
				: TEXT("The HTTP Chat request failed."),
			true,
			true);
		return;
	}

	const bool bIsErrorResponse = !EHttpResponseCodes::IsOk(Response->GetResponseCode());
	SetConnectionState(EAIREChatConnectionState::Connected);
	const FAIREParsedChatFrame Frame = FAIREChatJsonAdapter::ParseHttpBody(
		Response->GetContentAsString(),
		ActiveRequestId,
		bIsErrorResponse);
	if (Frame.Kind == EAIREParsedChatFrameKind::Response)
	{
		HandleParsedResponse(Frame.Result);
	}
	else if (Frame.Kind == EAIREParsedChatFrameKind::Error)
	{
		HandleParsedError(Frame.Error);
	}
	else
	{
		HandleRequestFailure(
			Frame.Error.Code,
			Frame.Error.Message,
			false,
			false);
	}
}

void UAIRECompanionChatComponent::BeginFakeRequest()
{
	if (FakeScenario == EAIREChatFakeScenario::ConnectionFailure)
	{
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		HandleRequestFailure(
			TEXT("ConnectionFailed"),
			TEXT("Fake Chat connection failed."),
			true,
			true);
		return;
	}
	if (FakeScenario == EAIREChatFakeScenario::CredentialRejected)
	{
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		if (TokenProvider.GetObject() != nullptr)
		{
			IAIREGameClientTokenProvider::Execute_HandleGameClientTokenRejected(TokenProvider.GetObject());
		}
		HandleRequestFailure(
			TEXT("CredentialRejected"),
			TEXT("Fake GameClient credential was rejected."),
			false,
			true);
		return;
	}

	SetConnectionState(EAIREChatConnectionState::Connected);
	StartResponseTimeout();
	if (FakeScenario == EAIREChatFakeScenario::Timeout)
	{
		return;
	}

	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	const float Delay = FakeScenario == EAIREChatFakeScenario::DelayedSuccess
		? FMath::Min(2.0f, IsValid(Settings) ? Settings->ResponseTimeoutSeconds * 0.5f : 2.0f)
		: 0.05f;
	const uint64 RequestGeneration = Generation;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FakeResponseHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, RequestGeneration]()
			{
				CompleteFakeRequest(RequestGeneration);
			}),
			Delay,
			false);
	}
}

void UAIRECompanionChatComponent::CompleteFakeRequest(const uint64 RequestGeneration)
{
	if (bIsEndingPlay
		|| RequestGeneration != Generation
		|| RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}

	FString FrameJson;
	switch (FakeScenario)
	{
	case EAIREChatFakeScenario::Error:
		FrameJson = BuildFakeErrorFrame(ActiveRequestId);
		break;
	case EAIREChatFakeScenario::MalformedResponse:
		FrameJson = TEXT("{\"type\":\"chat_response\",\"payload\":");
		break;
	case EAIREChatFakeScenario::Success:
	case EAIREChatFakeScenario::DelayedSuccess:
	default:
		FrameJson = BuildFakeSuccessFrame(ActiveRequestId);
		break;
	}
	HandleWebSocketMessage(FrameJson);
}

void UAIRECompanionChatComponent::HandleParsedResponse(const FAIREChatResult& Result)
{
	ClearResponseTimeout();
	ResetActiveRequest();
	SetRequestState(EAIREChatRequestState::Idle);
	UE_LOG(LogAIRECompanionChat, Log, TEXT("Chat response received."));
	OnResponseReceived.Broadcast(Result);
}

void UAIRECompanionChatComponent::HandleParsedError(const FAIREChatError& Error)
{
	ClearResponseTimeout();
	SetRequestState(
		Error.bRetryable
			? EAIREChatRequestState::RetryableFailed
			: EAIREChatRequestState::Failed);
	OnRequestFailed.Broadcast(Error);
	if (!Error.bRetryable)
	{
		ResetActiveRequest();
	}
}

void UAIRECompanionChatComponent::HandleRequestFailure(
	const FString& Code,
	const FString& Message,
	const bool bRetryable,
	const bool bPreserveRequest)
{
	UE_LOG(
		LogAIRECompanionChat,
		Warning,
		TEXT("Chat request failed. Code=%s Retryable=%s"),
		*Code,
		bRetryable ? TEXT("true") : TEXT("false"));
	ClearConnectionTimeout();
	ClearResponseTimeout();
	FAIREChatError Error;
	Error.RequestId = ActiveRequestId;
	Error.Code = Code;
	Error.Message = Message;
	Error.bRetryable = bRetryable;
	SetRequestState(
		bRetryable
			? EAIREChatRequestState::RetryableFailed
			: EAIREChatRequestState::Failed);
	OnRequestFailed.Broadcast(Error);
	if (!bPreserveRequest || !bRetryable)
	{
		ResetActiveRequest();
	}
}

void UAIRECompanionChatComponent::HandleConnectionTimeout()
{
	if (bIsEndingPlay || ConnectionState != EAIREChatConnectionState::Connecting)
	{
		return;
	}

	const TSharedPtr<IWebSocket> TimedOutSocket = WebSocket;
	WebSocket.Reset();
	if (TimedOutSocket.IsValid())
	{
		TimedOutSocket->OnConnected().Clear();
		TimedOutSocket->OnConnectionError().Clear();
		TimedOutSocket->OnClosed().Clear();
		TimedOutSocket->OnMessage().Clear();
		TimedOutSocket->Close(NormalClosureCode, TEXT("Connection timeout"));
	}
	SetConnectionState(EAIREChatConnectionState::Disconnected);
	HandleRequestFailure(
		TEXT("ConnectionTimeout"),
		TEXT("Chat connection timed out."),
		true,
		true);
}

void UAIRECompanionChatComponent::HandleResponseTimeout()
{
	if (bIsEndingPlay || RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}
	HandleRequestFailure(
		TEXT("RequestTimeout"),
		TEXT("Chat response timed out."),
		true,
		true);
}

void UAIRECompanionChatComponent::SetConnectionState(const EAIREChatConnectionState NewState)
{
	if (ConnectionState == NewState)
	{
		return;
	}
	ConnectionState = NewState;
	OnConnectionStateChanged.Broadcast(ConnectionState);
}

void UAIRECompanionChatComponent::SetRequestState(const EAIREChatRequestState NewState)
{
	if (RequestState == NewState)
	{
		return;
	}
	RequestState = NewState;
	OnRequestStateChanged.Broadcast(RequestState);
}

void UAIRECompanionChatComponent::StartConnectionTimeout()
{
	ClearConnectionTimeout();
	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	if (UWorld* World = GetWorld(); IsValid(Settings) && IsValid(World))
	{
		World->GetTimerManager().SetTimer(
			ConnectionTimeoutHandle,
			this,
			&UAIRECompanionChatComponent::HandleConnectionTimeout,
			Settings->ConnectionTimeoutSeconds,
			false);
	}
}

void UAIRECompanionChatComponent::StartResponseTimeout()
{
	ClearResponseTimeout();
	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	if (UWorld* World = GetWorld(); IsValid(Settings) && IsValid(World))
	{
		World->GetTimerManager().SetTimer(
			ResponseTimeoutHandle,
			this,
			&UAIRECompanionChatComponent::HandleResponseTimeout,
			Settings->ResponseTimeoutSeconds,
			false);
	}
}

void UAIRECompanionChatComponent::ClearConnectionTimeout()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConnectionTimeoutHandle);
	}
}

void UAIRECompanionChatComponent::ClearResponseTimeout()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResponseTimeoutHandle);
	}
}

void UAIRECompanionChatComponent::ResetActiveRequest()
{
	ActiveRequestId.Reset();
	ActiveMessageId.Reset();
	ActiveWebSocketFrame.Reset();
	ActiveHttpBody.Reset();
}

void UAIRECompanionChatComponent::ShutdownRequests()
{
	++Generation;
	ClearConnectionTimeout();
	ClearResponseTimeout();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FakeResponseHandle);
	}
	if (RegistrationRequest.IsValid())
	{
		RegistrationRequest->OnProcessRequestComplete().Unbind();
		RegistrationRequest->CancelRequest();
		RegistrationRequest.Reset();
	}
	if (HttpChatRequest.IsValid())
	{
		HttpChatRequest->OnProcessRequestComplete().Unbind();
		HttpChatRequest->CancelRequest();
		HttpChatRequest.Reset();
	}
	Disconnect();
	ResetActiveRequest();
}

bool UAIRECompanionChatComponent::ResolveToken(FString& OutToken) const
{
	OutToken.Reset();
	bool bShouldPersistRuntimeToken = false;
	if (TokenProvider.GetObject() != nullptr)
	{
		if (!IAIREGameClientTokenProvider::Execute_GetGameClientToken(
			TokenProvider.GetObject(),
			OutToken))
		{
			OutToken.Reset();
			return false;
		}
	}
	else
	{
		OutToken = FPlatformMisc::GetEnvironmentVariable(DeviceTokenEnvironmentVariable);
		bShouldPersistRuntimeToken = !OutToken.IsEmpty();
		if (!bShouldPersistRuntimeToken
			&& !FAIREGameClientCredentialStore::Load(GetCredentialTarget(), OutToken))
		{
			return false;
		}
	}

	if (OutToken.Len() < 32 || OutToken.Len() > 512)
	{
		OutToken.Reset();
		return false;
	}

	if (bShouldPersistRuntimeToken
		&& !FAIREGameClientCredentialStore::Save(GetCredentialTarget(), OutToken))
	{
		UE_LOG(
			LogAIRECompanionChat,
			Warning,
			TEXT("Could not persist the runtime GameClient credential."));
	}
	return true;
}

FString UAIRECompanionChatComponent::GetCredentialTarget() const
{
	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	const FString Authority = IsValid(Settings) ? Settings->BackendBaseUrl : TEXT("unconfigured");
	return TEXT("AI_RE.GameClient.") + FMD5::HashAnsiString(*Authority);
}

FString UAIRECompanionChatComponent::LoadOrCreateRegistrationRequestId()
{
	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	if (!IsValid(Settings))
	{
		return FString();
	}

	const FString MetadataPath = GetIdentityMetadataPath();
	FString ExistingJson;
	TSharedPtr<FJsonObject> Existing;
	if (FFileHelper::LoadFileToString(ExistingJson, *MetadataPath)
		&& DeserializeObject(ExistingJson, Existing))
	{
		FString BackendBaseUrl;
		FString RequestId;
		if (Existing->TryGetStringField(TEXT("backend_base_url"), BackendBaseUrl)
			&& BackendBaseUrl == Settings->BackendBaseUrl
			&& Existing->TryGetStringField(TEXT("registration_request_id"), RequestId)
			&& FAIREChatJsonAdapter::IsStableId(RequestId))
		{
			return RequestId;
		}
	}

	const FString RequestId = NewStableId(TEXT("register-game"));
	const TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
	Metadata->SetNumberField(TEXT("schema_version"), 1);
	Metadata->SetStringField(TEXT("backend_base_url"), Settings->BackendBaseUrl);
	Metadata->SetStringField(TEXT("registration_request_id"), RequestId);
	FString SerializedMetadata;
	if (!SerializeObject(Metadata, SerializedMetadata))
	{
		return FString();
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(MetadataPath), true);
	return FFileHelper::SaveStringToFile(SerializedMetadata, *MetadataPath)
		? RequestId
		: FString();
}

FString UAIRECompanionChatComponent::GetCompanionId() const
{
	const AAIRECompanionCharacter* Companion = Cast<AAIRECompanionCharacter>(GetOwner());
	return IsValid(Companion) ? Companion->GetCompanionId() : TEXT("MAKO");
}

#include "Chat/AIRECompanionChatComponent.h"

#include "AIREWorldTimeSubsystem.h"
#include "Chat/Context/AIREWorldContextBuilder.h"
#include "Chat/Transport/AIREChatJsonAdapter.h"
#include "Chat/Contracts/AIREChatSettings.h"
#include "Chat/Presentation/AIREChatPresentationGuard.h"
#include "Command/AIRECompanionCommandGatewayComponent.h"
#include "Core/AIRECompanionCharacter.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "IWebSocket.h"
#include "Misc/Guid.h"
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
	constexpr int32 MaxHttpResponseBytes = 262144;
	constexpr TCHAR FixedGameClientToken[] = TEXT("AIRE_GAME");
	constexpr TCHAR CanonicalSaveSlotId[] = TEXT("demo-slot-1");
	constexpr TCHAR CanonicalCompanionId[] = TEXT("mako");

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

	EAIREGameWorldPeriod GetGameWorldPeriod(const float Hour)
	{
		const int32 WholeHour = FMath::FloorToInt(Hour);
		if (WholeHour >= 5 && WholeHour < 8)
		{
			return EAIREGameWorldPeriod::Dawn;
		}
		if (WholeHour >= 8 && WholeHour < 12)
		{
			return EAIREGameWorldPeriod::Morning;
		}
		if (WholeHour >= 12 && WholeHour < 18)
		{
			return EAIREGameWorldPeriod::Afternoon;
		}
		if (WholeHour >= 18 && WholeHour < 22)
		{
			return EAIREGameWorldPeriod::Evening;
		}
		return EAIREGameWorldPeriod::Night;
	}

	bool SerializeObject(const TSharedRef<FJsonObject>& Object, FString& OutJson)
	{
		OutJson.Reset();
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object, Writer);
	}

	FString BuildFakeSuccessBody(
		const FString& RequestId,
		const FString& MessageId,
		const FString& SessionId,
		const FString& SaveSlotId,
		const FString& CompanionId)
	{
		const TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
		Metadata->SetStringField(TEXT("provider"), TEXT("fake"));
		Metadata->SetStringField(TEXT("model_version"), TEXT("fake-v1"));
		Metadata->SetStringField(TEXT("prompt_version"), TEXT("fake-v1"));

		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("request_id"), RequestId);
		Body->SetStringField(TEXT("message_id"), MessageId);
		Body->SetStringField(TEXT("session_id"), SessionId);
		Body->SetStringField(TEXT("save_slot_id"), SaveSlotId);
		Body->SetStringField(TEXT("companion_id"), CompanionId);
		Body->SetStringField(TEXT("response_id"), NewStableId(TEXT("response")));
		Body->SetStringField(TEXT("display_text"), TEXT("Fake Companion response."));
		Body->SetArrayField(TEXT("command_candidates"), TArray<TSharedPtr<FJsonValue>>());
		Body->SetObjectField(TEXT("ai_metadata"), Metadata);
		FString Json;
		SerializeObject(Body, Json);
		return Json;
	}

	FString BuildFakeErrorBody(
		const FString& RequestId,
		const FString& Code,
		const FString& Message,
		const bool bRetryable)
	{
		const TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("code"), Code);
		Error->SetStringField(TEXT("message"), Message);
		Error->SetBoolField(TEXT("retryable"), bRetryable);
		Error->SetObjectField(TEXT("details"), MakeShared<FJsonObject>());

		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("request_id"), RequestId);
		Body->SetObjectField(TEXT("error"), Error);
		FString Json;
		SerializeObject(Body, Json);
		return Json;
	}
}

UAIRECompanionChatComponent::UAIRECompanionChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIRECompanionChatComponent::ConfigureInGameContext(const FAIREInGameChatContext& InContext)
{
	FAIREInGameChatContext NormalizedContext = InContext;
	NormalizedContext.SaveSlotId = CanonicalSaveSlotId;
	FString Error;
	if (!ValidateContext(NormalizedContext, Error))
	{
		return false;
	}

	ChatContext = MoveTemp(NormalizedContext);
	bHasChatContext = true;
	return true;
}

bool UAIRECompanionChatComponent::SendPlayerMessage(const FString& UserMessage)
{
	if (bIsEndingPlay
		|| RequestState == EAIREChatRequestState::Sending)
	{
		return false;
	}

	FString ContextError;
	if (!bHasChatContext || !ValidateContext(ChatContext, ContextError))
	{
		HandleRequestFailure(
			TEXT("MissingContext"),
			TEXT("A valid InGame Chat context is required."),
			false);
		return false;
	}

	++Generation;
	ActiveRequestId = NewStableId(TEXT("request"));
	ActiveMessageId = NewStableId(TEXT("message"));
	ActiveUserMessage = UserMessage.TrimStartAndEnd();

	FAIREInGameChatContext EffectiveContext = ChatContext;
	if (UWorld* World = GetWorld())
	{
		if (const UAIREWorldTimeSubsystem* WorldTime =
			World->GetSubsystem<UAIREWorldTimeSubsystem>())
		{
			EffectiveContext.Day = WorldTime->GetCurrentDay();
			EffectiveContext.Hour = WorldTime->GetCurrentTimeOfDay();
			EffectiveContext.Period = GetGameWorldPeriod(EffectiveContext.Hour);
		}
	}

	const FAIREWorldContextV1 WorldContext = FAIREWorldContextBuilder::Build(
		Cast<AAIRECompanionCharacter>(GetOwner()),
		EffectiveContext.LocationId);
	const AAIRECompanionCharacter* Character =
		Cast<AAIRECompanionCharacter>(GetOwner());
	const UAIRECompanionCommandGatewayComponent* Gateway =
		IsValid(Character) ? Character->GetCommandGatewayComponent() : nullptr;
	const bool bCraftItemRuntimeAvailable = IsValid(Gateway)
		&& Gateway->CanAdvertiseCraftItem(WorldContext);
	FString SerializationError;
	if (!FAIREChatJsonAdapter::BuildInGameRequest(
		EffectiveContext,
		WorldContext,
		CanonicalCompanionId,
		SessionId,
		ActiveRequestId,
		ActiveMessageId,
		UserMessage,
		bCraftItemRuntimeAvailable,
		ActiveHttpBody,
		ActiveWebSocketFrame,
		SerializationError))
	{
		HandleRequestFailure(
			TEXT("InvalidRequest"),
			SerializationError,
			false);
		return false;
	}

	const uint64 RequestGeneration = Generation;
	SetRequestState(EAIREChatRequestState::Sending);
	if (bIsEndingPlay
		|| RequestGeneration != Generation
		|| RequestState != EAIREChatRequestState::Sending
		|| ActiveRequestId.IsEmpty()
		|| ActiveHttpBody.IsEmpty())
	{
		return false;
	}
	if (!BeginActiveRequest())
	{
		return false;
	}
	return RequestGeneration == Generation
		&& RequestState == EAIREChatRequestState::Sending;
}

bool UAIRECompanionChatComponent::RetryLastRequest()
{
	return false;
}

void UAIRECompanionChatComponent::CancelActiveRequest()
{
	if (bIsCancellingRequest || RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}

	bIsCancellingRequest = true;
	++Generation;
	ClearConnectionTimeout();
	ClearResponseTimeout();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FakeResponseHandle);
	}
	if (HttpChatRequest.IsValid())
	{
		HttpChatRequest->OnProcessRequestComplete().Unbind();
		HttpChatRequest->CancelRequest();
		HttpChatRequest.Reset();
	}

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
			SocketToClose->Close(NormalClosureCode, TEXT("Request cancelled"));
		}
	}
	SetConnectionState(EAIREChatConnectionState::Disconnected);
	ResetActiveRequest();
	bIsCancellingRequest = false;
	SetRequestState(EAIREChatRequestState::Cancelled);
}

void UAIRECompanionChatComponent::Disconnect()
{
	if (bIsCancellingRequest)
	{
		return;
	}

	if (RequestState == EAIREChatRequestState::Sending && HttpChatRequest.IsValid())
	{
		CancelActiveRequest();
		return;
	}

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
	return false;
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
		|| !FAIREChatJsonAdapter::IsStableId(Context.LocationId)
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

	BeginSelectedTransport(FixedGameClientToken);
	return true;
}

void UAIRECompanionChatComponent::BeginSelectedTransport(const FString& Token)
{
	SendActiveHttpRequest(Token);
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
		HandleRequestFailure(TEXT("MissingSettings"), TEXT("Chat settings are unavailable."), false);
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
	if (RequestState == EAIREChatRequestState::Sending)
	{
		HandleRequestFailure(
			StatusCode == PolicyViolationCode ? TEXT("CredentialRejected") : TEXT("ConnectionClosed"),
			StatusCode == PolicyViolationCode
				? TEXT("The GameClient credential was rejected.")
				: TEXT("Chat connection closed before a response was confirmed."),
			StatusCode != PolicyViolationCode);
	}
}

void UAIRECompanionChatComponent::HandleWebSocketMessage(const FString& Message)
{
	if (bIsEndingPlay || RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}

	const FAIREChatResponseCorrelation ExpectedCorrelation{
		ActiveRequestId,
		ActiveMessageId,
		SessionId,
		ChatContext.SaveSlotId,
		CanonicalCompanionId};
	const FAIREParsedChatFrame Frame =
		FAIREChatJsonAdapter::ParseWebSocketFrame(Message, ExpectedCorrelation);
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
		HandleRequestFailure(TEXT("MissingSettings"), TEXT("Chat settings are unavailable."), false);
		return;
	}

	const uint64 RequestGeneration = Generation;
	SetConnectionState(EAIREChatConnectionState::Connecting);
	if (bIsEndingPlay
		|| RequestGeneration != Generation
		|| RequestState != EAIREChatRequestState::Sending
		|| ActiveRequestId.IsEmpty()
		|| ActiveHttpBody.IsEmpty())
	{
		return;
	}
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
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		if (RequestGeneration != Generation
			|| RequestState != EAIREChatRequestState::Sending)
		{
			return;
		}
		HandleRequestFailure(
			TEXT("ConnectionFailed"),
			TEXT("Could not start the HTTP Chat request."),
			true);
	}
}

void UAIRECompanionChatComponent::HandleHttpChatComplete(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	const uint64 RequestGeneration)
{
	if (bIsEndingPlay
		|| RequestGeneration != Generation
		|| Request != HttpChatRequest
		|| RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}
	HttpChatRequest.Reset();

	if (!bWasSuccessful || !Response.IsValid())
	{
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		if (RequestGeneration != Generation
			|| RequestState != EAIREChatRequestState::Sending)
		{
			return;
		}
		const bool bTimedOut = Request.IsValid()
			&& Request->GetFailureReason() == EHttpFailureReason::TimedOut;
		HandleRequestFailure(
			bTimedOut ? TEXT("RequestTimeout") : TEXT("ConnectionFailed"),
			bTimedOut
				? TEXT("The HTTP Chat request timed out.")
				: TEXT("The HTTP Chat request failed."),
			true);
		return;
	}

	const bool bIsErrorResponse = !EHttpResponseCodes::IsOk(Response->GetResponseCode());
	SetConnectionState(EAIREChatConnectionState::Connected);
	if (bIsEndingPlay
		|| RequestGeneration != Generation
		|| RequestState != EAIREChatRequestState::Sending
		|| ActiveRequestId.IsEmpty())
	{
		return;
	}
	if (Response->GetContent().Num() > MaxHttpResponseBytes)
	{
		HandleRequestFailure(
			TEXT("ResponseTooLarge"),
			TEXT("HTTP Chat response exceeds the supported size limit."),
			false);
		return;
	}
	const FAIREChatResponseCorrelation ExpectedCorrelation{
		ActiveRequestId,
		ActiveMessageId,
		SessionId,
		ChatContext.SaveSlotId,
		CanonicalCompanionId};
	const FAIREParsedChatFrame Frame = FAIREChatJsonAdapter::ParseHttpBody(
		Response->GetContentAsString(),
		ExpectedCorrelation,
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
			false);
	}
}

void UAIRECompanionChatComponent::BeginFakeRequest()
{
	if (FakeScenario == EAIREChatFakeScenario::ConnectionFailure)
	{
		const uint64 RequestGeneration = Generation;
		SetConnectionState(EAIREChatConnectionState::Disconnected);
		if (RequestGeneration != Generation
			|| RequestState != EAIREChatRequestState::Sending)
		{
			return;
		}
		HandleRequestFailure(
			TEXT("ConnectionFailed"),
			TEXT("Fake Chat connection failed."),
			true);
		return;
	}
	const uint64 RequestGeneration = Generation;
	SetConnectionState(EAIREChatConnectionState::Connected);
	if (bIsEndingPlay
		|| RequestGeneration != Generation
		|| RequestState != EAIREChatRequestState::Sending
		|| ActiveRequestId.IsEmpty())
	{
		return;
	}
	StartResponseTimeout();
	if (FakeScenario == EAIREChatFakeScenario::Timeout)
	{
		return;
	}

	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	const float Delay = FakeScenario == EAIREChatFakeScenario::DelayedSuccess
		? FMath::Min(2.0f, IsValid(Settings) ? Settings->ResponseTimeoutSeconds * 0.5f : 2.0f)
		: 0.05f;
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

	FString BodyJson;
	bool bIsErrorResponse = false;
	switch (FakeScenario)
	{
	case EAIREChatFakeScenario::CredentialRejected:
		bIsErrorResponse = true;
		BodyJson = BuildFakeErrorBody(
			ActiveRequestId,
			TEXT("UnauthorizedDevice"),
			TEXT("Fake GameClient credential was rejected."),
			false);
		break;
	case EAIREChatFakeScenario::Forbidden:
		bIsErrorResponse = true;
		BodyJson = BuildFakeErrorBody(
			ActiveRequestId,
			TEXT("DeviceRoleNotAllowed"),
			TEXT("Fake GameClient role is not allowed."),
			false);
		break;
	case EAIREChatFakeScenario::Error:
		bIsErrorResponse = true;
		BodyJson = BuildFakeErrorBody(
			ActiveRequestId,
			TEXT("AIServiceUnavailable"),
			TEXT("Fake AI service is unavailable."),
			true);
		break;
	case EAIREChatFakeScenario::MalformedResponse:
		BodyJson = TEXT("{\"request_id\":");
		break;
	case EAIREChatFakeScenario::Success:
	case EAIREChatFakeScenario::DelayedSuccess:
	default:
		BodyJson = BuildFakeSuccessBody(
			ActiveRequestId,
			ActiveMessageId,
			SessionId,
			ChatContext.SaveSlotId,
			CanonicalCompanionId);
		break;
	}

	const FAIREChatResponseCorrelation ExpectedCorrelation{
		ActiveRequestId,
		ActiveMessageId,
		SessionId,
		ChatContext.SaveSlotId,
		CanonicalCompanionId};
	const FAIREParsedChatFrame Frame = FAIREChatJsonAdapter::ParseHttpBody(
		BodyJson,
		ExpectedCorrelation,
		bIsErrorResponse);
	switch (Frame.Kind)
	{
	case EAIREParsedChatFrameKind::Response:
		HandleParsedResponse(Frame.Result);
		return;
	case EAIREParsedChatFrameKind::Error:
		HandleParsedError(Frame.Error);
		return;
	case EAIREParsedChatFrameKind::Ignored:
	case EAIREParsedChatFrameKind::Invalid:
	default:
		HandleRequestFailure(Frame.Error.Code, Frame.Error.Message, false);
	}
}

void UAIRECompanionChatComponent::HandleParsedResponse(const FAIREChatResult& Result)
{
	ClearResponseTimeout();
	FAIREChatResult GuardedResult = Result;
	GuardedResult.RawDisplayText = Result.DisplayText;
	GuardedResult.SubmittedUserMessage = ActiveUserMessage;
	bool bEchoReplaced = false;
	GuardedResult.DisplayText = FAIREChatPresentationGuard::GuardDisplayText(
		GuardedResult.DisplayText,
		ActiveUserMessage,
		bEchoReplaced);
	if (bEchoReplaced)
	{
		UE_LOG(
			LogAIRECompanionChat,
			Warning,
			TEXT("Unsafe companion display text was replaced. RequestId=%s"),
			*GuardedResult.RequestId);
	}
	ResetActiveRequest();
	SetRequestState(EAIREChatRequestState::Idle);
	UE_LOG(LogAIRECompanionChat, Log, TEXT("Chat response received."));
	OnResponseReceived.Broadcast(GuardedResult);
}

void UAIRECompanionChatComponent::HandleParsedError(const FAIREChatError& Error)
{
	ClearResponseTimeout();
	FAIREChatError CorrelatedError = Error;
	CorrelatedError.SubmittedUserMessage = ActiveUserMessage;
	ResetActiveRequest();
	SetRequestState(EAIREChatRequestState::Failed);
	OnRequestFailed.Broadcast(CorrelatedError);
}

void UAIRECompanionChatComponent::HandleRequestFailure(
	const FString& Code,
	const FString& Message,
	const bool bRetryable)
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
	Error.SubmittedUserMessage = ActiveUserMessage;
	Error.bRetryable = bRetryable;
	ResetActiveRequest();
	SetRequestState(EAIREChatRequestState::Failed);
	OnRequestFailed.Broadcast(Error);
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
	const uint64 RequestGeneration = Generation;
	SetConnectionState(EAIREChatConnectionState::Disconnected);
	if (RequestGeneration != Generation
		|| RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}
	HandleRequestFailure(
		TEXT("ConnectionTimeout"),
		TEXT("Chat connection timed out."),
		true);
}

void UAIRECompanionChatComponent::HandleResponseTimeout()
{
	if (bIsEndingPlay || RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}

	const uint64 RequestGeneration = ++Generation;
	if (HttpChatRequest.IsValid())
	{
		HttpChatRequest->OnProcessRequestComplete().Unbind();
		HttpChatRequest->CancelRequest();
		HttpChatRequest.Reset();
	}
	SetConnectionState(EAIREChatConnectionState::Disconnected);
	if (RequestGeneration != Generation
		|| RequestState != EAIREChatRequestState::Sending)
	{
		return;
	}
	HandleRequestFailure(
		TEXT("RequestTimeout"),
		TEXT("Chat response timed out."),
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
	ActiveUserMessage.Reset();
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
	if (HttpChatRequest.IsValid())
	{
		HttpChatRequest->OnProcessRequestComplete().Unbind();
		HttpChatRequest->CancelRequest();
		HttpChatRequest.Reset();
	}
	Disconnect();
	ResetActiveRequest();
}

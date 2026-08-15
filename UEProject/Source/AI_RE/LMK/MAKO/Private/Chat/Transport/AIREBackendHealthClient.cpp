#include "Chat/Transport/AIREBackendHealthClient.h"

#include "Chat/Contracts/AIREChatSettings.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr int32 MaxHealthResponseBytes = 262144;

	FAIREBackendHealthResult MakeHealthResult(
		const EAIREBackendHealthStatus Status,
		const TOptional<int32>& HttpStatusCode = TOptional<int32>(),
		const TCHAR* ErrorCode = nullptr)
	{
		FAIREBackendHealthResult Result;
		Result.Status = Status;
		Result.HttpStatusCode = HttpStatusCode;
		if (ErrorCode != nullptr)
		{
			Result.ErrorCode = FString(ErrorCode);
		}
		return Result;
	}
}

TSharedPtr<FAIREBackendHealthClient> FAIREBackendHealthClient::Create(UObject* Owner)
{
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	TSharedPtr<FAIREBackendHealthClient> Client = MakeShareable(
		new FAIREBackendHealthClient(Owner));
	return Client;
}

FAIREBackendHealthClient::FAIREBackendHealthClient(UObject* Owner)
	: Owner(Owner)
{
}

FAIREBackendHealthClient::~FAIREBackendHealthClient()
{
	Shutdown();
}

bool FAIREBackendHealthClient::CheckHealth()
{
	if (bShuttingDown || bRequestActive || !Owner.IsValid())
	{
		return false;
	}

	const UAIREChatSettings* Settings = GetDefault<UAIREChatSettings>();
	if (!IsValid(Settings))
	{
		return false;
	}

	const FString HealthUrl = BuildHealthUrl(Settings->BackendBaseUrl);
	if (HealthUrl.IsEmpty())
	{
		return false;
	}

	FHttpRequestPtr Request = FHttpModule::Get().CreateRequest();
	if (!Request.IsValid())
	{
		return false;
	}

	const uint64 RequestGeneration = ++Generation;
	bRequestActive = true;
	ActiveRequest = Request;
	Request->SetURL(HealthUrl);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetTimeout(Settings->ConnectionTimeoutSeconds);

	const TWeakPtr<FAIREBackendHealthClient> WeakClient = AsShared();
	Request->OnProcessRequestComplete().BindLambda(
		[WeakClient, RequestGeneration](
			FHttpRequestPtr CompletedRequest,
			FHttpResponsePtr Response,
			const bool bWasSuccessful)
		{
			const TSharedPtr<FAIREBackendHealthClient> Client = WeakClient.Pin();
			if (Client.IsValid())
			{
				Client->HandleRequestComplete(
					CompletedRequest,
					Response,
					bWasSuccessful,
					RequestGeneration);
			}
		});

	if (!Request->ProcessRequest())
	{
		++Generation;
		ReleaseActiveRequest(true);
		return false;
	}

	return true;
}

void FAIREBackendHealthClient::Cancel()
{
	if (bShuttingDown || !bRequestActive)
	{
		return;
	}

	++Generation;
	ReleaseActiveRequest(true);
	if (!Owner.IsValid())
	{
		return;
	}

	OnCompleted.Broadcast(MakeHealthResult(
		EAIREBackendHealthStatus::Cancelled,
		TOptional<int32>(),
		TEXT("Cancelled")));
}

FString FAIREBackendHealthClient::BuildHealthUrl(const FString& BackendBaseUrl)
{
	FString BaseUrl = BackendBaseUrl.TrimStartAndEnd();
	while (BaseUrl.EndsWith(TEXT("/")))
	{
		BaseUrl.LeftChopInline(1, EAllowShrinking::No);
	}
	if (BaseUrl.IsEmpty())
	{
		return FString();
	}
	return BaseUrl + TEXT("/health");
}

FAIREBackendHealthResult FAIREBackendHealthClient::ClassifyPayload(
	const bool bWasSuccessful,
	const bool bTimedOut,
	const TOptional<int32>& HttpStatusCode,
	const FString& Body,
	const int32 BodyByteCount)
{
	if (!bWasSuccessful || !HttpStatusCode.IsSet())
	{
		return MakeHealthResult(
			bTimedOut
				? EAIREBackendHealthStatus::Timeout
				: EAIREBackendHealthStatus::Unreachable,
			TOptional<int32>(),
			bTimedOut ? TEXT("Timeout") : TEXT("TransportError"));
	}

	const int32 ResponseCode = HttpStatusCode.GetValue();
	if (!EHttpResponseCodes::IsOk(ResponseCode))
	{
		return MakeHealthResult(
			EAIREBackendHealthStatus::Unreachable,
			ResponseCode,
			TEXT("HttpStatusError"));
	}

	if (BodyByteCount > MaxHealthResponseBytes)
	{
		return MakeHealthResult(
			EAIREBackendHealthStatus::InvalidResponse,
			ResponseCode,
			TEXT("ResponseTooLarge"));
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return MakeHealthResult(
			EAIREBackendHealthStatus::InvalidResponse,
			ResponseCode,
			TEXT("MalformedResponse"));
	}

	FString Service;
	FString Status;
	FString Provider;
	const bool bValidEnvelope = JsonObject->HasTypedField<EJson::String>(TEXT("service"))
		&& JsonObject->TryGetStringField(TEXT("service"), Service)
		&& Service == TEXT("mako-companion")
		&& JsonObject->HasTypedField<EJson::String>(TEXT("status"))
		&& JsonObject->TryGetStringField(TEXT("status"), Status)
		&& Status == TEXT("ok")
		&& JsonObject->HasTypedField<EJson::String>(TEXT("llm_provider"))
		&& JsonObject->TryGetStringField(TEXT("llm_provider"), Provider)
		&& !Provider.TrimStartAndEnd().IsEmpty();
	if (!bValidEnvelope)
	{
		return MakeHealthResult(
			EAIREBackendHealthStatus::InvalidResponse,
			ResponseCode,
			TEXT("MalformedResponse"));
	}

	return MakeHealthResult(EAIREBackendHealthStatus::Reachable, ResponseCode);
}

void FAIREBackendHealthClient::HandleRequestComplete(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	const uint64 RequestGeneration)
{
	if (bShuttingDown
		|| !bRequestActive
		|| RequestGeneration != Generation
		|| Request != ActiveRequest)
	{
		return;
	}

	if (!Owner.IsValid())
	{
		ReleaseActiveRequest(false);
		return;
	}

	const bool bTimedOut = Request.IsValid()
		&& Request->GetFailureReason() == EHttpFailureReason::TimedOut;
	TOptional<int32> HttpStatusCode;
	FString Body;
	int32 BodyByteCount = 0;
	if (Response.IsValid())
	{
		HttpStatusCode = Response->GetResponseCode();
		BodyByteCount = Response->GetContent().Num();
		if (BodyByteCount <= MaxHealthResponseBytes)
		{
			Body = Response->GetContentAsString();
		}
	}

	CompleteRequest(
		RequestGeneration,
		ClassifyPayload(
			bWasSuccessful,
			bTimedOut,
			HttpStatusCode,
			Body,
			BodyByteCount));
}

void FAIREBackendHealthClient::CompleteRequest(
	const uint64 RequestGeneration,
	const FAIREBackendHealthResult& Result)
{
	if (bShuttingDown
		|| !bRequestActive
		|| RequestGeneration != Generation)
	{
		return;
	}

	ReleaseActiveRequest(false);
	if (Owner.IsValid())
	{
		OnCompleted.Broadcast(Result);
	}
}

void FAIREBackendHealthClient::ReleaseActiveRequest(const bool bCancelRequest)
{
	bRequestActive = false;
	if (!ActiveRequest.IsValid())
	{
		return;
	}

	const FHttpRequestPtr Request = ActiveRequest;
	ActiveRequest.Reset();
	Request->OnProcessRequestComplete().Unbind();
	if (bCancelRequest)
	{
		Request->CancelRequest();
	}
}

void FAIREBackendHealthClient::Shutdown()
{
	if (bShuttingDown)
	{
		return;
	}

	bShuttingDown = true;
	++Generation;
	ReleaseActiveRequest(true);
	OnCompleted.Clear();
}

#if WITH_DEV_AUTOMATION_TESTS

FString FAIREBackendHealthClient::BuildHealthUrlForTesting(
	const FString& BackendBaseUrl)
{
	return BuildHealthUrl(BackendBaseUrl);
}

FAIREBackendHealthResult FAIREBackendHealthClient::ClassifyForTesting(
	const bool bWasSuccessful,
	const bool bTimedOut,
	const TOptional<int32>& HttpStatusCode,
	const FString& Body)
{
	FTCHARToUTF8 Utf8Body(*Body);
	return ClassifyPayload(
		bWasSuccessful,
		bTimedOut,
		HttpStatusCode,
		Body,
		Utf8Body.Length());
}

uint64 FAIREBackendHealthClient::BeginRequestForTesting()
{
	if (bShuttingDown || bRequestActive || !Owner.IsValid())
	{
		return 0;
	}

	bRequestActive = true;
	return ++Generation;
}

void FAIREBackendHealthClient::CompleteRequestForTesting(
	const uint64 RequestGeneration,
	const FAIREBackendHealthResult& Result)
{
	CompleteRequest(RequestGeneration, Result);
}

#endif

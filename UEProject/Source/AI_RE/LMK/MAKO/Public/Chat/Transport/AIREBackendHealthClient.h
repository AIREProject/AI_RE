#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"

enum class EAIREBackendHealthStatus : uint8
{
	Reachable,
	Unreachable,
	Timeout,
	InvalidResponse,
	Cancelled
};

struct AI_RE_API FAIREBackendHealthResult
{
	EAIREBackendHealthStatus Status = EAIREBackendHealthStatus::Unreachable;
	TOptional<int32> HttpStatusCode;
	TOptional<FString> ErrorCode;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FAIREBackendHealthCompleted,
	const FAIREBackendHealthResult&);

class AI_RE_API FAIREBackendHealthClient final
	: public TSharedFromThis<FAIREBackendHealthClient>
{
public:
	static TSharedPtr<FAIREBackendHealthClient> Create(UObject* Owner);

	~FAIREBackendHealthClient();

	bool CheckHealth();
	void Cancel();

	FAIREBackendHealthCompleted OnCompleted;

private:
	explicit FAIREBackendHealthClient(UObject* Owner);
	FAIREBackendHealthClient(const FAIREBackendHealthClient&) = delete;
	FAIREBackendHealthClient& operator=(const FAIREBackendHealthClient&) = delete;

	static FString BuildHealthUrl(const FString& BackendBaseUrl);
	static FAIREBackendHealthResult ClassifyPayload(
		bool bWasSuccessful,
		bool bTimedOut,
		const TOptional<int32>& HttpStatusCode,
		const FString& Body,
		int32 BodyByteCount);

	void HandleRequestComplete(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		uint64 RequestGeneration);
	void CompleteRequest(
		uint64 RequestGeneration,
		const FAIREBackendHealthResult& Result);
	void ReleaseActiveRequest(bool bCancelRequest);
	void Shutdown();

#if WITH_DEV_AUTOMATION_TESTS
	friend class FAIREBackendHealthClientAutomationTest;

	static FString BuildHealthUrlForTesting(const FString& BackendBaseUrl);
	static FAIREBackendHealthResult ClassifyForTesting(
		bool bWasSuccessful,
		bool bTimedOut,
		const TOptional<int32>& HttpStatusCode,
		const FString& Body);
	uint64 BeginRequestForTesting();
	void CompleteRequestForTesting(
		uint64 RequestGeneration,
		const FAIREBackendHealthResult& Result);
#endif

	TWeakObjectPtr<UObject> Owner;
	FHttpRequestPtr ActiveRequest;
	uint64 Generation = 0;
	bool bRequestActive = false;
	bool bShuttingDown = false;
};

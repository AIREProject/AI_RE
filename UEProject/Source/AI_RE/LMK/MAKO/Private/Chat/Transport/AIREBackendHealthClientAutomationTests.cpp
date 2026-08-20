#if WITH_DEV_AUTOMATION_TESTS

#include "Chat/Transport/AIREBackendHealthClient.h"

#include "Chat/Contracts/AIREChatSettings.h"
#include "Misc/AutomationTest.h"

namespace
{
	FString MakeHealthBody(const FString& Provider)
	{
		return FString::Printf(
			TEXT("{\"service\":\"mako-companion\",\"status\":\"ok\",\"llm_provider\":\"%s\"}"),
			*Provider);
	}

	FAIREBackendHealthResult MakeTestResult(const EAIREBackendHealthStatus Status)
	{
		FAIREBackendHealthResult Result;
		Result.Status = Status;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREBackendHealthClientAutomationTest,
	"AIRE.Companion.BackendHealth.Client",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREBackendHealthClientAutomationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(
		TEXT("Health path joins a base URL without a trailing slash"),
		FAIREBackendHealthClient::BuildHealthUrlForTesting(
			TEXT("https://traip.mtvs2026.work")),
		FString(TEXT("https://traip.mtvs2026.work/health")));
	TestEqual(
		TEXT("Health path removes every trailing slash before joining"),
		FAIREBackendHealthClient::BuildHealthUrlForTesting(
			TEXT("  https://traip.mtvs2026.work///  ")),
		FString(TEXT("https://traip.mtvs2026.work/health")));
	TestTrue(
		TEXT("An empty base URL cannot produce a Health URL"),
		FAIREBackendHealthClient::BuildHealthUrlForTesting(TEXT(" / ")).IsEmpty());

	const TCHAR* const Providers[] =
	{
		TEXT("mock"),
		TEXT("local"),
		TEXT("openai")
	};
	FAIREBackendHealthResult Result;
	for (const TCHAR* Provider : Providers)
	{
		const FAIREBackendHealthResult ProviderResult =
			FAIREBackendHealthClient::ClassifyForTesting(
				true,
				false,
				200,
				MakeHealthBody(Provider));
		TestTrue(
			TEXT("Every non-empty provider remains connectivity-only Reachable"),
			ProviderResult.Status == EAIREBackendHealthStatus::Reachable);
		TestTrue(
			TEXT("Reachable result retains the HTTP status"),
			ProviderResult.HttpStatusCode.IsSet()
				&& ProviderResult.HttpStatusCode.GetValue() == 200);
		TestFalse(
			TEXT("Reachable result exposes no provider or error metadata"),
			ProviderResult.ErrorCode.IsSet());
	}
	Result = FAIREBackendHealthClient::ClassifyForTesting(
		true,
		false,
		200,
		TEXT("{\"service\":\"mako-companion\",\"status\":\"ok\",\"llm_provider\":\"mock\",\"future_field\":true}"));
	TestTrue(
		TEXT("Unknown optional Health fields remain compatible"),
		Result.Status == EAIREBackendHealthStatus::Reachable);

	Result = FAIREBackendHealthClient::ClassifyForTesting(
			true,
			false,
			503,
			MakeHealthBody(TEXT("mock")));
	TestTrue(
		TEXT("A non-2xx response is Unreachable"),
		Result.Status == EAIREBackendHealthStatus::Unreachable);
	TestTrue(
		TEXT("A non-2xx result retains its HTTP status"),
		Result.HttpStatusCode.IsSet()
			&& Result.HttpStatusCode.GetValue() == 503);
	TestTrue(
		TEXT("A non-2xx result uses client-owned error metadata"),
		Result.ErrorCode.IsSet()
			&& Result.ErrorCode.GetValue() == TEXT("HttpStatusError"));

	Result = FAIREBackendHealthClient::ClassifyForTesting(
		false,
		false,
		TOptional<int32>(),
		FString());
	TestTrue(
		TEXT("A transport failure is Unreachable"),
		Result.Status == EAIREBackendHealthStatus::Unreachable);
	TestFalse(
		TEXT("A transport failure has no HTTP status"),
		Result.HttpStatusCode.IsSet());

	const FString InvalidBodies[] =
	{
		TEXT("{\"service\":"),
		TEXT("[]"),
		TEXT("{\"service\":\"other\",\"status\":\"ok\",\"llm_provider\":\"mock\"}"),
		TEXT("{\"service\":\"mako-companion\",\"status\":\"ready\",\"llm_provider\":\"mock\"}"),
		TEXT("{\"service\":\"mako-companion\",\"status\":\"ok\",\"llm_provider\":\"   \"}"),
		TEXT("{\"service\":\"mako-companion\",\"status\":\"ok\",\"llm_provider\":1}"),
		TEXT("{\"service\":\"mako-companion\",\"status\":\"ok\",\"llm_provider\":true}")
	};
	for (const FString& Body : InvalidBodies)
	{
		Result = FAIREBackendHealthClient::ClassifyForTesting(
			true,
			false,
			200,
			Body);
		TestTrue(
			*FString::Printf(
				TEXT("Malformed JSON or envelope is InvalidResponse: %s"),
				*Body),
			Result.Status == EAIREBackendHealthStatus::InvalidResponse);
	}

	Result = FAIREBackendHealthClient::ClassifyForTesting(
		true,
		false,
		200,
		FString::ChrN(262145, TEXT('x')));
	TestTrue(
		TEXT("A response above 256 KiB is InvalidResponse"),
		Result.Status == EAIREBackendHealthStatus::InvalidResponse);
	TestTrue(
		TEXT("An oversized response has a bounded client error"),
		Result.ErrorCode.IsSet()
			&& Result.ErrorCode.GetValue() == TEXT("ResponseTooLarge"));

	Result = FAIREBackendHealthClient::ClassifyForTesting(
		false,
		true,
		TOptional<int32>(),
		FString());
	TestTrue(
		TEXT("A timed-out transport is Timeout"),
		Result.Status == EAIREBackendHealthStatus::Timeout);

	UAIREChatSettings* Owner = NewObject<UAIREChatSettings>(GetTransientPackage());
	TSharedPtr<FAIREBackendHealthClient> Client =
		FAIREBackendHealthClient::Create(Owner);
	TestTrue(TEXT("A valid owner creates a Health client"), Client.IsValid());
	if (!Client.IsValid())
	{
		return false;
	}

	int32 CompletionCount = 0;
	EAIREBackendHealthStatus LastStatus = EAIREBackendHealthStatus::Reachable;
	Client->OnCompleted.AddLambda(
		[&CompletionCount, &LastStatus](const FAIREBackendHealthResult& Completion)
		{
			++CompletionCount;
			LastStatus = Completion.Status;
		});

	const uint64 TimeoutGeneration = Client->BeginRequestForTesting();
	Client->CompleteRequestForTesting(
		TimeoutGeneration,
		MakeTestResult(EAIREBackendHealthStatus::Timeout));
	Client->CompleteRequestForTesting(
		TimeoutGeneration,
		MakeTestResult(EAIREBackendHealthStatus::Reachable));
	TestEqual(TEXT("Timeout completes at most once"), CompletionCount, 1);
	TestTrue(
		TEXT("Timeout keeps its terminal status"),
		LastStatus == EAIREBackendHealthStatus::Timeout);

	const uint64 CancelledGeneration = Client->BeginRequestForTesting();
	Client->Cancel();
	TestEqual(TEXT("Explicit cancellation completes once"), CompletionCount, 2);
	TestTrue(
		TEXT("Explicit cancellation reports Cancelled"),
		LastStatus == EAIREBackendHealthStatus::Cancelled);
	Client->CompleteRequestForTesting(
		CancelledGeneration,
		MakeTestResult(EAIREBackendHealthStatus::Reachable));
	TestEqual(
		TEXT("A late completion after cancellation is ignored"),
		CompletionCount,
		2);

	const uint64 CurrentGeneration = Client->BeginRequestForTesting();
	Client->CompleteRequestForTesting(
		CancelledGeneration,
		MakeTestResult(EAIREBackendHealthStatus::Reachable));
	TestEqual(
		TEXT("A previous generation cannot complete the current request"),
		CompletionCount,
		2);
	Client->CompleteRequestForTesting(
		CurrentGeneration,
		MakeTestResult(EAIREBackendHealthStatus::Reachable));
	TestEqual(
		TEXT("The current generation completes exactly once"),
		CompletionCount,
		3);

	UAIREChatSettings* DestroyedOwner =
		NewObject<UAIREChatSettings>(GetTransientPackage());
	TSharedPtr<FAIREBackendHealthClient> DestroyedOwnerClient =
		FAIREBackendHealthClient::Create(DestroyedOwner);
	int32 DestroyedOwnerCompletionCount = 0;
	DestroyedOwnerClient->OnCompleted.AddLambda(
		[&DestroyedOwnerCompletionCount](const FAIREBackendHealthResult&)
		{
			++DestroyedOwnerCompletionCount;
		});
	const uint64 DestroyedOwnerGeneration =
		DestroyedOwnerClient->BeginRequestForTesting();
	DestroyedOwner->MarkAsGarbage();
	DestroyedOwnerClient->CompleteRequestForTesting(
		DestroyedOwnerGeneration,
		MakeTestResult(EAIREBackendHealthStatus::Reachable));
	TestEqual(
		TEXT("Owner destruction makes completion a no-op"),
		DestroyedOwnerCompletionCount,
		0);

	UAIREChatSettings* DestructorOwner =
		NewObject<UAIREChatSettings>(GetTransientPackage());
	TSharedPtr<FAIREBackendHealthClient> DestructorClient =
		FAIREBackendHealthClient::Create(DestructorOwner);
	int32 DestructorCompletionCount = 0;
	DestructorClient->OnCompleted.AddLambda(
		[&DestructorCompletionCount](const FAIREBackendHealthResult&)
		{
			++DestructorCompletionCount;
		});
	DestructorClient->BeginRequestForTesting();
	DestructorClient.Reset();
	TestEqual(
		TEXT("Client destruction cancels silently"),
		DestructorCompletionCount,
		0);

	return true;
}

#endif

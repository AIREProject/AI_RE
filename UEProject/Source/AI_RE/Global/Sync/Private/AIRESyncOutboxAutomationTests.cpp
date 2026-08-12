#if WITH_DEV_AUTOMATION_TESTS

#include "AIRESyncOutboxSaveGame.h"
#include "AIRESyncOutboxSubsystem.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

class FAIRESyncOutboxTestAccess
{
public:
	static void Prepare(UAIRESyncOutboxSubsystem& Subsystem)
	{
		Subsystem.bReady = true;
		Subsystem.bShuttingDown = false;
		Subsystem.bSaveInFlight = true;
		Subsystem.bDirty = false;
		Subsystem.NextSequence = 1;
		Subsystem.LatestGeneration = 1;
		Subsystem.HighestIssuedGeneration = 1;
	}

	static FAIRESyncOutboxScope CanonicalScope()
	{
		return UAIRESyncOutboxSubsystem::MakeCanonicalScope();
	}

	static FString Hash(const TArray<uint8>& Body)
	{
		return UAIRESyncOutboxSubsystem::ComputeBodyHash(Body);
	}

	static bool Validate(
		const UAIRESyncOutboxSubsystem& Subsystem,
		const FAIRESyncOutboxSaveEnvelope& Envelope,
		FAIRESyncOutboxSaveEnvelope& OutNormalized,
		bool& bOutNeedsCompact)
	{
		return Subsystem.ValidateEnvelope(
			Envelope,
			OutNormalized,
			bOutNeedsCompact);
	}

	static bool Fits(
		const UAIRESyncOutboxSubsystem& Subsystem,
		const FAIRESyncOutboxSaveEnvelope& Envelope)
	{
		return Subsystem.DoesEnvelopeFit(Envelope);
	}

	static FAIRESyncOutboxSaveEnvelope PrepareCompactionEnvelope(
		UAIRESyncOutboxSubsystem& Subsystem,
		const FAIRESyncOutboxEntry& AckedEntry)
	{
		Subsystem.Entries = {AckedEntry};
		Subsystem.Entries[0].State = EAIRESyncOutboxEntryState::Acked;
		Subsystem.ScheduleAckedCompaction();
		return Subsystem.BuildEnvelope(8);
	}

	static void FinishCompaction(UAIRESyncOutboxSubsystem& Subsystem)
	{
		Subsystem.FinalizeAckedCompaction();
	}

	static void FinalizeLoadedSlots(
		UAIRESyncOutboxSubsystem& Subsystem,
		const FAIRESyncOutboxSaveEnvelope& Primary,
		const FAIRESyncOutboxSaveEnvelope& Previous,
		FAIRESyncOutboxPersistenceResult& OutLoadResult)
	{
		const FDelegateHandle ResultHandle =
			Subsystem.PersistenceCompletedDelegate.AddLambda(
				[&OutLoadResult](
					const FAIRESyncOutboxPersistenceResult& Result)
				{
					OutLoadResult = Result;
				});
		Subsystem.Entries.Reset();
		Subsystem.bReady = false;
		Subsystem.bShuttingDown = false;
		Subsystem.bSaveInFlight = true;
		Subsystem.PersistenceEpoch = 41;
		Subsystem.LoadSlots.SetNum(2);
		Subsystem.LoadSlots[0].SlotName = AIRESyncOutbox::PrimarySlotName;
		Subsystem.LoadSlots[0].bCompleted = true;
		Subsystem.LoadSlots[0].bExists = true;
		Subsystem.LoadSlots[0].LoadedEnvelope = Primary;
		Subsystem.LoadSlots[1].SlotName = AIRESyncOutbox::PreviousSlotName;
		Subsystem.LoadSlots[1].bCompleted = true;
		Subsystem.LoadSlots[1].bExists = true;
		Subsystem.LoadSlots[1].LoadedEnvelope = Previous;
		Subsystem.FinalizeLoad(41);
		Subsystem.PersistenceCompletedDelegate.Remove(ResultHandle);
	}

	static void PrepareActiveAttempt(
		UAIRESyncOutboxSubsystem& Subsystem,
		const FAIRESyncOutboxEntry& Entry,
		const TSharedPtr<IAIRESyncOutboxTransport>& Transport)
	{
		Subsystem.Entries = {Entry};
		Subsystem.Entries[0].State = EAIRESyncOutboxEntryState::InFlight;
		Subsystem.Transport = Transport;
		Subsystem.ActiveOperationId = Entry.OperationId;
		Subsystem.ActiveAttemptToken = FGuid::NewGuid();
		Subsystem.bStartTransportAfterSave = true;
		Subsystem.BeginActiveTransport();
		Subsystem.bSaveInFlight = true;
	}

	static void PrepareDispatchablePending(
		UAIRESyncOutboxSubsystem& Subsystem,
		const FAIRESyncOutboxEntry& Entry)
	{
		Subsystem.Entries = {Entry};
		Subsystem.Entries[0].State = EAIRESyncOutboxEntryState::Pending;
		Subsystem.Transport.Reset();
		Subsystem.ActiveOperationId.Invalidate();
		Subsystem.ActiveAttemptToken.Invalidate();
		Subsystem.bSaveInFlight = false;
		Subsystem.bDirty = false;
	}

	static bool Timeout(UAIRESyncOutboxSubsystem& Subsystem)
	{
		if (Subsystem.AttemptTimeoutTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(
				Subsystem.AttemptTimeoutTickerHandle);
			Subsystem.AttemptTimeoutTickerHandle.Reset();
		}
		return Subsystem.HandleAttemptTimeout(0.0f);
	}
};

namespace
{
	TArray<uint8> Utf8Body(const FString& Text)
	{
		FTCHARToUTF8 Converted(*Text);
		TArray<uint8> Bytes;
		Bytes.Append(
			reinterpret_cast<const uint8*>(Converted.Get()),
			Converted.Length());
		return Bytes;
	}

	FAIRESyncOutboxEnqueueRequest MakeRequest(
		const EAIRESyncOutboxOperationKind Kind,
		const FString& BodyText,
		const FString& CoalescingKey = FString())
	{
		FAIRESyncOutboxEnqueueRequest Request;
		Request.SchemaVersion = 1;
		Request.Kind = Kind;
		Request.OperationId = FGuid::NewGuid();
		Request.Scope = FAIRESyncOutboxTestAccess::CanonicalScope();
		Request.CoalescingKey = CoalescingKey;
		Request.Body = Utf8Body(BodyText);
		return Request;
	}

	FAIRESyncOutboxEntry MakeEntry(
		const EAIRESyncOutboxEntryState State,
		const int64 Sequence)
	{
		const FAIRESyncOutboxEnqueueRequest Request = MakeRequest(
			EAIRESyncOutboxOperationKind::Snapshot,
			TEXT("{\"revision\":1}"),
			TEXT("inventory"));
		FAIRESyncOutboxEntry Entry;
		Entry.SchemaVersion = Request.SchemaVersion;
		Entry.Kind = Request.Kind;
		Entry.OperationId = Request.OperationId;
		Entry.BodyHash = FAIRESyncOutboxTestAccess::Hash(Request.Body);
		Entry.Scope = Request.Scope;
		Entry.CoalescingKey = Request.CoalescingKey;
		Entry.Body = Request.Body;
		Entry.Sequence = Sequence;
		Entry.State = State;
		Entry.CreatedAtUtc = FDateTime::UtcNow();
		return Entry;
	}

	class FFakeSyncOutboxTransport final : public IAIRESyncOutboxTransport
	{
	public:
		virtual bool StartAttempt(
			const FAIRESyncOutboxTransportAttempt& Attempt,
			FAIRESyncOutboxTransportCallback Completion) override
		{
			LastAttempt = Attempt;
			Callback = MoveTemp(Completion);
			StartCount++;
			return bAcceptAttempts;
		}

		virtual void CancelAttempt(const FGuid& AttemptToken) override
		{
			LastCancelledToken = AttemptToken;
			CancelCount++;
		}

		void Complete(const FAIRESyncOutboxTransportResult& Result)
		{
			if (Callback)
			{
				Callback(Result);
			}
		}

		FAIRESyncOutboxTransportAttempt LastAttempt;
		FAIRESyncOutboxTransportCallback Callback;
		FGuid LastCancelledToken;
		int32 StartCount = 0;
		int32 CancelCount = 0;
		bool bAcceptAttempts = true;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRESyncOutboxEnqueueAndCoalescingTest,
	"AIRE.Sync.Outbox.EnqueueAndCoalescing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIRESyncOutboxEnqueueAndCoalescingTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIRESyncOutboxSubsystem> Subsystem(
		NewObject<UAIRESyncOutboxSubsystem>(GameInstance.Get()));
	FAIRESyncOutboxTestAccess::Prepare(*Subsystem);
	TestEqual(
		TEXT("SHA-256 empty known-answer vector"),
		FAIRESyncOutboxTestAccess::Hash(TArray<uint8>()),
		FString(TEXT(
			"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
	TestEqual(
		TEXT("SHA-256 abc known-answer vector"),
		FAIRESyncOutboxTestAccess::Hash(Utf8Body(TEXT("abc"))),
		FString(TEXT(
			"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
	TestEqual(
		TEXT("SHA-256 two-block padding known-answer vector"),
		FAIRESyncOutboxTestAccess::Hash(Utf8Body(TEXT(
			"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))),
		FString(TEXT(
			"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")));

	FAIRESyncOutboxEnqueueRequest First = MakeRequest(
		EAIRESyncOutboxOperationKind::Snapshot,
		TEXT("{\"revision\":1}"),
		TEXT("inventory"));
	const FAIRESyncOutboxEnqueueResult FirstResult =
		Subsystem->Enqueue(First);
	TestEqual(
		TEXT("First snapshot is accepted"),
		FirstResult.Code,
		EAIRESyncOutboxEnqueueCode::AcceptedPendingPersistence);

	const FAIRESyncOutboxEnqueueResult DuplicateResult =
		Subsystem->Enqueue(First);
	TestEqual(
		TEXT("Same immutable operation is a duplicate"),
		DuplicateResult.Code,
		EAIRESyncOutboxEnqueueCode::Duplicate);
	First.Body = Utf8Body(TEXT("{\"revision\":2}"));
	const FAIRESyncOutboxEnqueueResult ConflictResult =
		Subsystem->Enqueue(First);
	TestEqual(
		TEXT("Same operation id with another body conflicts"),
		ConflictResult.Code,
		EAIRESyncOutboxEnqueueCode::Conflict);

	const FAIRESyncOutboxEnqueueRequest Replacement = MakeRequest(
		EAIRESyncOutboxOperationKind::Snapshot,
		TEXT("{\"revision\":3}"),
		TEXT("inventory"));
	const FAIRESyncOutboxEnqueueResult CoalescedResult =
		Subsystem->Enqueue(Replacement);
	TestEqual(
		TEXT("Pending snapshot is coalesced"),
		CoalescedResult.Code,
		EAIRESyncOutboxEnqueueCode::CoalescedPendingPersistence);
	TestEqual(TEXT("Only replacement remains"), Subsystem->GetEntryCount(), 1);
	TestEqual(
		TEXT("Replacement identity is authoritative"),
		Subsystem->GetEntries()[0].OperationId,
		Replacement.OperationId);

	const FAIRESyncOutboxEnqueueRequest Event = MakeRequest(
		EAIRESyncOutboxOperationKind::Event,
		TEXT("{\"event\":1}"));
	Subsystem->Enqueue(Event);
	const FAIRESyncOutboxEnqueueRequest AfterEvent = MakeRequest(
		EAIRESyncOutboxOperationKind::Snapshot,
		TEXT("{\"revision\":4}"),
		TEXT("inventory"));
	const FAIRESyncOutboxEnqueueResult AfterEventResult =
		Subsystem->Enqueue(AfterEvent);
	TestEqual(
		TEXT("Event is a coalescing barrier"),
		AfterEventResult.Code,
		EAIRESyncOutboxEnqueueCode::AcceptedPendingPersistence);
	TestEqual(TEXT("Event order is preserved"), Subsystem->GetEntryCount(), 3);
	TestEqual(
		TEXT("Pending cancel removes only the requested event"),
		Subsystem->CancelOperation(Event.OperationId),
		EAIRESyncOutboxCancelCode::RemovedPending);
	TestEqual(TEXT("Pending cancel updates queue count"), Subsystem->GetEntryCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRESyncOutboxPersistenceValidationTest,
	"AIRE.Sync.Outbox.PersistenceValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIRESyncOutboxPersistenceValidationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIRESyncOutboxSubsystem> Subsystem(
		NewObject<UAIRESyncOutboxSubsystem>(GameInstance.Get()));
	FAIRESyncOutboxTestAccess::Prepare(*Subsystem);

	FAIRESyncOutboxSaveEnvelope Envelope;
	Envelope.FormatVersion = AIRESyncOutbox::SaveFormatVersion;
	Envelope.Generation = 7;
	Envelope.Scope = FAIRESyncOutboxTestAccess::CanonicalScope();
	Envelope.Entries.Add(MakeEntry(EAIRESyncOutboxEntryState::InFlight, 1));
	Envelope.Entries.Add(MakeEntry(EAIRESyncOutboxEntryState::Acked, 2));

	TStrongObjectPtr<UAIRESyncOutboxSaveGame> SaveGame(
		NewObject<UAIRESyncOutboxSaveGame>());
	SaveGame->Envelope = Envelope;
	TArray<uint8> SaveBytes;
	TestTrue(
		TEXT("Outbox envelope serializes to memory"),
		UGameplayStatics::SaveGameToMemory(SaveGame.Get(), SaveBytes));
	TStrongObjectPtr<UAIRESyncOutboxSaveGame> LoadedSave(
		Cast<UAIRESyncOutboxSaveGame>(
			UGameplayStatics::LoadGameFromMemory(SaveBytes)));
	TestNotNull(TEXT("Outbox envelope deserializes"), LoadedSave.Get());
	if (!LoadedSave.Get())
	{
		return false;
	}
	TestTrue(
		TEXT("Canonical scope survives SaveGame serialization"),
		LoadedSave->Envelope.Scope == Envelope.Scope);

	FAIRESyncOutboxSaveEnvelope Normalized;
	bool bNeedsCompact = false;
	TestTrue(
		TEXT("Valid envelope is accepted"),
		LoadedSave.Get() != nullptr
			&& FAIRESyncOutboxTestAccess::Validate(
				*Subsystem,
				LoadedSave->Envelope,
				Normalized,
				bNeedsCompact));
	TestTrue(TEXT("Recovery requires a new generation"), bNeedsCompact);
	TestEqual(
		TEXT("Acked identity remains until durable compact"),
		Normalized.Entries.Num(),
		2);
	if (!Normalized.Entries.IsEmpty())
	{
		TestEqual(
			TEXT("InFlight entry recovers to Pending"),
			Normalized.Entries[0].State,
			EAIRESyncOutboxEntryState::Pending);
	}
	if (Normalized.Entries.Num() > 1)
	{
		TestEqual(
			TEXT("Acked entry remains non-replayable"),
			Normalized.Entries[1].State,
			EAIRESyncOutboxEntryState::Acked);
	}

	const FString ValidHash = Envelope.Entries[0].BodyHash;
	Envelope.Entries[0].BodyHash =
		TEXT("0000000000000000000000000000000000000000000000000000000000000000");
	TestFalse(
		TEXT("Mismatched body hash rejects the whole envelope"),
		FAIRESyncOutboxTestAccess::Validate(
			*Subsystem,
			Envelope,
			Normalized,
			bNeedsCompact));
	Envelope.Entries[0].BodyHash = ValidHash;
	Envelope.FormatVersion = AIRESyncOutbox::SaveFormatVersion + 1;
	TestFalse(
		TEXT("Unsupported format rejects the whole envelope"),
		FAIRESyncOutboxTestAccess::Validate(
			*Subsystem,
			Envelope,
			Normalized,
			bNeedsCompact));

	const FAIRESyncOutboxSaveEnvelope CompactEnvelope =
		FAIRESyncOutboxTestAccess::PrepareCompactionEnvelope(
			*Subsystem,
			Envelope.Entries[1]);
	TestEqual(
		TEXT("Compact candidate excludes durable Acked entry"),
		CompactEnvelope.Entries.Num(),
		0);
	TestEqual(
		TEXT("Acked identity remains in memory during compact write"),
		Subsystem->GetEntryCount(),
		1);
	FAIRESyncOutboxTestAccess::FinishCompaction(*Subsystem);
	TestEqual(
		TEXT("Acked identity is removed after compact success"),
		Subsystem->GetEntryCount(),
		0);

	FAIRESyncOutboxSaveEnvelope Previous = LoadedSave->Envelope;
	Previous.Generation = 6;
	FAIRESyncOutboxSaveEnvelope CorruptPrimary = LoadedSave->Envelope;
	CorruptPrimary.Generation = 7;
	CorruptPrimary.Entries[0].BodyHash =
		TEXT("0000000000000000000000000000000000000000000000000000000000000000");
	TStrongObjectPtr<UAIRESyncOutboxSubsystem> FallbackSubsystem(
		NewObject<UAIRESyncOutboxSubsystem>(GameInstance.Get()));
	FAIRESyncOutboxPersistenceResult FallbackLoadResult;
	FAIRESyncOutboxTestAccess::FinalizeLoadedSlots(
		*FallbackSubsystem,
		CorruptPrimary,
		Previous,
		FallbackLoadResult);
	TestTrue(TEXT("Previous generation is used as fallback"), FallbackSubsystem->IsReady());
	TestEqual(
		TEXT("Fallback completion result is explicit"),
		FallbackLoadResult.Code,
		EAIRESyncOutboxPersistenceCode::SucceededWithFallback);
	TestEqual(
		TEXT("Fallback completion selects previous generation"),
		FallbackLoadResult.Generation,
		Previous.Generation);
	TestTrue(
		TEXT("Fallback completion flag is preserved"),
		FallbackLoadResult.bUsedFallback);

	TStrongObjectPtr<UAIRESyncOutboxSubsystem> SafeEmptySubsystem(
		NewObject<UAIRESyncOutboxSubsystem>(GameInstance.Get()));
	FAIRESyncOutboxSaveEnvelope CorruptPrevious = Previous;
	CorruptPrevious.FormatVersion = AIRESyncOutbox::SaveFormatVersion + 1;
	FAIRESyncOutboxPersistenceResult SafeEmptyLoadResult;
	FAIRESyncOutboxTestAccess::FinalizeLoadedSlots(
		*SafeEmptySubsystem,
		CorruptPrimary,
		CorruptPrevious,
		SafeEmptyLoadResult);
	TestTrue(TEXT("Invalid slots still finish startup safely"), SafeEmptySubsystem->IsReady());
	TestEqual(TEXT("Invalid slots produce an empty queue"), SafeEmptySubsystem->GetEntryCount(), 0);
	TestEqual(
		TEXT("Invalid slots fall back to explicit safe-empty state"),
		SafeEmptyLoadResult.Code,
		EAIRESyncOutboxPersistenceCode::SafeEmptyNoValidSave);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRESyncOutboxBoundsTest,
	"AIRE.Sync.Outbox.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIRESyncOutboxBoundsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIRESyncOutboxSubsystem> Subsystem(
		NewObject<UAIRESyncOutboxSubsystem>(GameInstance.Get()));
	FAIRESyncOutboxTestAccess::Prepare(*Subsystem);

	FAIRESyncOutboxEnqueueRequest Oversized = MakeRequest(
		EAIRESyncOutboxOperationKind::Event,
		TEXT("{}"));
	Oversized.Body.Init(static_cast<uint8>('a'), AIRESyncOutbox::MaxBodyBytes + 1);
	TestEqual(
		TEXT("Oversized body is rejected"),
		Subsystem->Enqueue(Oversized).Code,
		EAIRESyncOutboxEnqueueCode::PayloadTooLarge);

	for (int32 Index = 0; Index < AIRESyncOutbox::MaxEntries; ++Index)
	{
		const FAIRESyncOutboxEnqueueResult Result = Subsystem->Enqueue(
			MakeRequest(
				EAIRESyncOutboxOperationKind::Event,
				FString::Printf(TEXT("{\"event\":%d}"), Index)));
		if (Result.Code !=
			EAIRESyncOutboxEnqueueCode::AcceptedPendingPersistence)
		{
			AddError(FString::Printf(
				TEXT("Entry %d was unexpectedly rejected"),
				Index));
			break;
		}
	}
	const int32 CountBeforeOverflow = Subsystem->GetEntryCount();
	TestEqual(
		TEXT("Entry count overflow is rejected"),
		Subsystem->Enqueue(MakeRequest(
			EAIRESyncOutboxOperationKind::Event,
			TEXT("{\"overflow\":true}"))).Code,
		EAIRESyncOutboxEnqueueCode::QueueFull);
	TestEqual(
		TEXT("Rejected enqueue does not mutate the queue"),
		Subsystem->GetEntryCount(),
		CountBeforeOverflow);

	FAIRESyncOutboxSaveEnvelope OversizedEnvelope;
	OversizedEnvelope.FormatVersion = AIRESyncOutbox::SaveFormatVersion;
	OversizedEnvelope.Generation = 1;
	OversizedEnvelope.Scope = FAIRESyncOutboxTestAccess::CanonicalScope();
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FAIRESyncOutboxEntry LargeEntry =
			MakeEntry(EAIRESyncOutboxEntryState::Pending, Index + 1);
		LargeEntry.OperationId = FGuid::NewGuid();
		LargeEntry.Body.Init(
			static_cast<uint8>('a'),
			AIRESyncOutbox::MaxBodyBytes);
		LargeEntry.BodyHash = FAIRESyncOutboxTestAccess::Hash(LargeEntry.Body);
		OversizedEnvelope.Entries.Add(MoveTemp(LargeEntry));
	}
	TestFalse(
		TEXT("Serialized envelope limit includes body and metadata"),
		FAIRESyncOutboxTestAccess::Fits(*Subsystem, OversizedEnvelope));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRESyncOutboxTransportLifecycleTest,
	"AIRE.Sync.Outbox.TransportLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIRESyncOutboxTransportLifecycleTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UAIRESyncOutboxSubsystem> Subsystem(
		NewObject<UAIRESyncOutboxSubsystem>(GameInstance.Get()));
	FAIRESyncOutboxTestAccess::Prepare(*Subsystem);
	const FAIRESyncOutboxEntry Entry =
		MakeEntry(EAIRESyncOutboxEntryState::InFlight, 1);
	FAIRESyncOutboxTestAccess::PrepareDispatchablePending(*Subsystem, Entry);
	TestEqual(
		TEXT("Missing transport leaves Pending operation untouched"),
		Subsystem->DispatchNext(),
		EAIRESyncOutboxDispatchCode::NoTransport);
	TestEqual(
		TEXT("Unavailable transport does not mutate state"),
		Subsystem->GetEntries()[0].State,
		EAIRESyncOutboxEntryState::Pending);
	const TSharedPtr<FFakeSyncOutboxTransport> FakeTransport =
		MakeShared<FFakeSyncOutboxTransport>();
	FAIRESyncOutboxTestAccess::PrepareActiveAttempt(
		*Subsystem,
		Entry,
		FakeTransport);
	TestEqual(TEXT("One attempt starts"), FakeTransport->StartCount, 1);

	FAIRESyncOutboxTransportResult WrongAck;
	WrongAck.Code = EAIRESyncOutboxTransportResultCode::Acked;
	WrongAck.AttemptToken = FakeTransport->LastAttempt.AttemptToken;
	WrongAck.OperationId = Entry.OperationId;
	WrongAck.BodyHash =
		TEXT("0000000000000000000000000000000000000000000000000000000000000000");
	FakeTransport->Complete(WrongAck);
	TestEqual(
		TEXT("Wrong hash cannot acknowledge the operation"),
		Subsystem->GetEntries()[0].State,
		EAIRESyncOutboxEntryState::InFlight);

	FAIRESyncOutboxTransportResult Ack = WrongAck;
	Ack.BodyHash = Entry.BodyHash;
	FakeTransport->Complete(Ack);
	TestEqual(
		TEXT("Matching ack enters durable Acked state"),
		Subsystem->GetEntries()[0].State,
		EAIRESyncOutboxEntryState::Acked);
	FakeTransport->Complete(Ack);
	TestEqual(
		TEXT("Late duplicate ack is ignored"),
		Subsystem->GetEntries()[0].State,
		EAIRESyncOutboxEntryState::Acked);

	TStrongObjectPtr<UAIRESyncOutboxSubsystem> TimeoutSubsystem(
		NewObject<UAIRESyncOutboxSubsystem>(GameInstance.Get()));
	FAIRESyncOutboxTestAccess::Prepare(*TimeoutSubsystem);
	const TSharedPtr<FFakeSyncOutboxTransport> TimeoutTransport =
		MakeShared<FFakeSyncOutboxTransport>();
	FAIRESyncOutboxTestAccess::PrepareActiveAttempt(
		*TimeoutSubsystem,
		Entry,
		TimeoutTransport);
	TestFalse(
		TEXT("Timeout ticker is one-shot"),
		FAIRESyncOutboxTestAccess::Timeout(*TimeoutSubsystem));
	TestEqual(
		TEXT("Timeout returns operation to Pending"),
		TimeoutSubsystem->GetEntries()[0].State,
		EAIRESyncOutboxEntryState::Pending);
	TestEqual(TEXT("Timeout cancels transport"), TimeoutTransport->CancelCount, 1);
	return true;
}

#endif

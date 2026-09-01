/*
 * Sync Outbox 구현 원칙
 * - Queue 변경과 InFlight 전환을 먼저 저장해 프로세스 종료 후에도 재전송 근거를 남긴다.
 * - 동시에 하나의 Attempt만 허용하고 Token, OperationId, BodyHash가 모두 일치한 ACK만 수락한다.
 * - Timeout과 실패는 Pending으로 복귀시키며 ACK 항목은 저장 완료 후 Compact한다.
 * - Generation과 Epoch로 오래된 Load/Save/Transport Callback이 최신 상태를 덮지 못하게 한다.
 */
#include "AIRESyncOutboxSubsystem.h"

#include "AIRESyncOutboxSaveGame.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
	constexpr uint32 Sha256RoundConstants[64] =
	{
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
		0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
		0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
		0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
		0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
		0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
		0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
		0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
		0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};

	uint32 RotateRight(const uint32 Value, const uint32 Count)
	{
		return (Value >> Count) | (Value << (32 - Count));
	}

	void TransformSha256Block(const uint8* Block, uint32* State)
	{
		uint32 Words[64];
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const int32 Offset = Index * 4;
			Words[Index] =
				(static_cast<uint32>(Block[Offset]) << 24)
				| (static_cast<uint32>(Block[Offset + 1]) << 16)
				| (static_cast<uint32>(Block[Offset + 2]) << 8)
				| static_cast<uint32>(Block[Offset + 3]);
		}
		for (int32 Index = 16; Index < 64; ++Index)
		{
			const uint32 Sigma0 = RotateRight(Words[Index - 15], 7)
				^ RotateRight(Words[Index - 15], 18)
				^ (Words[Index - 15] >> 3);
			const uint32 Sigma1 = RotateRight(Words[Index - 2], 17)
				^ RotateRight(Words[Index - 2], 19)
				^ (Words[Index - 2] >> 10);
			Words[Index] = Words[Index - 16]
				+ Sigma0
				+ Words[Index - 7]
				+ Sigma1;
		}

		uint32 A = State[0];
		uint32 B = State[1];
		uint32 C = State[2];
		uint32 D = State[3];
		uint32 E = State[4];
		uint32 F = State[5];
		uint32 G = State[6];
		uint32 H = State[7];
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const uint32 Choice = (E & F) ^ (~E & G);
			const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
			const uint32 BigSigma0 =
				RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
			const uint32 BigSigma1 =
				RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
			const uint32 Temp1 = H
				+ BigSigma1
				+ Choice
				+ Sha256RoundConstants[Index]
				+ Words[Index];
			const uint32 Temp2 = BigSigma0 + Majority;
			H = G;
			G = F;
			F = E;
			E = D + Temp1;
			D = C;
			C = B;
			B = A;
			A = Temp1 + Temp2;
		}

		State[0] += A;
		State[1] += B;
		State[2] += C;
		State[3] += D;
		State[4] += E;
		State[5] += F;
		State[6] += G;
		State[7] += H;
	}

	void ComputeSha256(
		const TArray<uint8>& Input,
		uint8 (&OutHash)[32])
	{
		uint32 State[8] =
		{
			0x6a09e667,
			0xbb67ae85,
			0x3c6ef372,
			0xa54ff53a,
			0x510e527f,
			0x9b05688c,
			0x1f83d9ab,
			0x5be0cd19
		};

		const int32 FullBlockCount = Input.Num() / 64;
		for (int32 BlockIndex = 0;
			BlockIndex < FullBlockCount;
			++BlockIndex)
		{
			TransformSha256Block(Input.GetData() + BlockIndex * 64, State);
		}

		uint8 FinalBlocks[128] = {};
		const int32 RemainingBytes = Input.Num() % 64;
		if (RemainingBytes > 0)
		{
			FMemory::Memcpy(
				FinalBlocks,
				Input.GetData() + FullBlockCount * 64,
				RemainingBytes);
		}
		FinalBlocks[RemainingBytes] = 0x80;
		const int32 FinalBlockCount = RemainingBytes < 56 ? 1 : 2;
		const uint64 BitLength = static_cast<uint64>(Input.Num()) * 8;
		const int32 LengthOffset = FinalBlockCount * 64 - 8;
		for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
		{
			FinalBlocks[LengthOffset + ByteIndex] = static_cast<uint8>(
				BitLength >> ((7 - ByteIndex) * 8));
		}
		for (int32 BlockIndex = 0;
			BlockIndex < FinalBlockCount;
			++BlockIndex)
		{
			TransformSha256Block(FinalBlocks + BlockIndex * 64, State);
		}

		for (int32 WordIndex = 0; WordIndex < 8; ++WordIndex)
		{
			for (int32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
			{
				OutHash[WordIndex * 4 + ByteIndex] = static_cast<uint8>(
					State[WordIndex] >> ((3 - ByteIndex) * 8));
			}
		}
	}

	bool IsContinuationByte(const uint8 Byte)
	{
		return (Byte & 0xC0) == 0x80;
	}
}

void UAIRESyncOutboxSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Entries.Reset();
	LoadSlots.Reset();
	PendingCompactionOperationIds.Reset();
	Transport.Reset();
	LastPersistenceResult = FAIRESyncOutboxPersistenceResult();
	ActiveAttemptToken.Invalidate();
	ActiveOperationId.Invalidate();
	LifecycleEpoch++;
	PersistenceEpoch = 0;
	ActiveSaveEpoch = 0;
	LatestGeneration = 0;
	HighestIssuedGeneration = 0;
	NextSequence = 1;
	LatestSlotName.Reset();
	LastIssuedSlotName.Reset();
	bReady = false;
	bShuttingDown = false;
	bDirty = false;
	bSaveInFlight = false;
	bCompactionSaveInFlight = false;
	bStartTransportAfterSave = false;
	BeginLoad();
}

void UAIRESyncOutboxSubsystem::Deinitialize()
{
	bShuttingDown = true;
	++LifecycleEpoch;

	if (AttemptTimeoutTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(AttemptTimeoutTickerHandle);
		AttemptTimeoutTickerHandle.Reset();
	}
	if (ActiveAttemptToken.IsValid() && Transport.IsValid())
	{
		Transport->CancelAttempt(ActiveAttemptToken);
	}
	if (const int32 ActiveIndex = FindEntryIndex(ActiveOperationId);
		Entries.IsValidIndex(ActiveIndex)
			&& Entries[ActiveIndex].State == EAIRESyncOutboxEntryState::InFlight)
	{
		Entries[ActiveIndex].State = EAIRESyncOutboxEntryState::Pending;
		bDirty = true;
	}
	ClearActiveAttempt();

	if (bReady && bDirty && HighestIssuedGeneration < MAX_int64)
	{
		const int64 Generation =
			FMath::Max(LatestGeneration, HighestIssuedGeneration) + 1;
		FAIRESyncOutboxSaveEnvelope Envelope = BuildEnvelope(Generation);
		if (DoesEnvelopeFit(Envelope))
		{
			const FString ShutdownSlotName = bSaveInFlight
				? (LastIssuedSlotName == AIRESyncOutbox::PrimarySlotName
					? FString(AIRESyncOutbox::PreviousSlotName)
					: FString(AIRESyncOutbox::PrimarySlotName))
				: GetNextSlotName();
			UAIRESyncOutboxSaveGame* SaveGame =
				NewObject<UAIRESyncOutboxSaveGame>();
			SaveGame->Envelope = MoveTemp(Envelope);
			UGameplayStatics::AsyncSaveGameToSlot(
				SaveGame,
				ShutdownSlotName,
				AIRESyncOutbox::UserIndex);
		}
	}

	++PersistenceEpoch;
	++ActiveSaveEpoch;
	PersistenceCompletedDelegate.Clear();
	Transport.Reset();
	LoadSlots.Reset();
	Entries.Reset();
	PendingCompactionOperationIds.Reset();
	bReady = false;
	bSaveInFlight = false;
	bCompactionSaveInFlight = false;
	bDirty = false;
	bStartTransportAfterSave = false;
	Super::Deinitialize();
}

bool UAIRESyncOutboxSubsystem::IsReady() const
{
	return bReady && !bShuttingDown;
}

int32 UAIRESyncOutboxSubsystem::GetEntryCount() const
{
	return Entries.Num();
}

TArray<FAIRESyncOutboxEntry> UAIRESyncOutboxSubsystem::GetEntries() const
{
	return Entries;
}

FAIRESyncOutboxPersistenceResult
UAIRESyncOutboxSubsystem::GetLastPersistenceResult() const
{
	return LastPersistenceResult;
}

FAIRESyncOutboxScope UAIRESyncOutboxSubsystem::MakeCanonicalScope()
{
	FAIRESyncOutboxScope Scope;
	Scope.ProfileId = AIRESyncOutbox::CanonicalProfileId;
	Scope.SaveSlotId = AIRESyncOutbox::CanonicalSaveSlotId;
	Scope.CompanionId = AIRESyncOutbox::CanonicalCompanionId;
	return Scope;
}

bool UAIRESyncOutboxSubsystem::IsCanonicalScope(
	const FAIRESyncOutboxScope& Scope)
{
	return Scope == MakeCanonicalScope();
}

bool UAIRESyncOutboxSubsystem::IsValidUtf8(const TArray<uint8>& Bytes)
{
	if (Bytes.IsEmpty())
	{
		return false;
	}

	int32 Index = 0;
	while (Index < Bytes.Num())
	{
		const uint8 First = Bytes[Index++];
		if (First <= 0x7F)
		{
			continue;
		}

		int32 Continuations = 0;
		uint32 CodePoint = 0;
		if (First >= 0xC2 && First <= 0xDF)
		{
			Continuations = 1;
			CodePoint = First & 0x1F;
		}
		else if (First >= 0xE0 && First <= 0xEF)
		{
			Continuations = 2;
			CodePoint = First & 0x0F;
		}
		else if (First >= 0xF0 && First <= 0xF4)
		{
			Continuations = 3;
			CodePoint = First & 0x07;
		}
		else
		{
			return false;
		}

		if (Index + Continuations > Bytes.Num())
		{
			return false;
		}
		for (int32 ContinuationIndex = 0;
			ContinuationIndex < Continuations;
			++ContinuationIndex)
		{
			const uint8 Byte = Bytes[Index++];
			if (!IsContinuationByte(Byte))
			{
				return false;
			}
			CodePoint = (CodePoint << 6) | (Byte & 0x3F);
		}

		if ((Continuations == 2 && CodePoint < 0x800)
			|| (Continuations == 3 && CodePoint < 0x10000)
			|| CodePoint > 0x10FFFF
			|| (CodePoint >= 0xD800 && CodePoint <= 0xDFFF))
		{
			return false;
		}
	}
	return true;
}

FString UAIRESyncOutboxSubsystem::ComputeBodyHash(
	const TArray<uint8>& Body)
{
	uint8 HashBytes[32];
	ComputeSha256(Body, HashBytes);
	return BytesToHex(HashBytes, UE_ARRAY_COUNT(HashBytes)).ToLower();
}

bool UAIRESyncOutboxSubsystem::IsValidBodyHash(const FString& BodyHash)
{
	if (BodyHash.Len() != 64)
	{
		return false;
	}
	for (const TCHAR Character : BodyHash)
	{
		if (!((Character >= TEXT('0') && Character <= TEXT('9'))
			|| (Character >= TEXT('a') && Character <= TEXT('f'))))
		{
			return false;
		}
	}
	return true;
}

bool UAIRESyncOutboxSubsystem::IsValidCoalescingKey(const FString& Key)
{
	if (Key.IsEmpty() || Key.Len() > AIRESyncOutbox::MaxCoalescingKeyLength)
	{
		return false;
	}
	for (const TCHAR Character : Key)
	{
		const bool bAllowed =
			(Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9'))
			|| Character == TEXT('.')
			|| Character == TEXT('-')
			|| Character == TEXT('_')
			|| Character == TEXT(':');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}

FAIRESyncOutboxEnqueueResult UAIRESyncOutboxSubsystem::Enqueue(
	const FAIRESyncOutboxEnqueueRequest& Request)
{
	FAIRESyncOutboxEnqueueResult Result;
	Result.OperationId = Request.OperationId;
	if (bShuttingDown)
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::ShuttingDown;
		return Result;
	}
	if (!bReady)
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::NotReady;
		return Result;
	}
	if (Request.SchemaVersion <= 0)
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::InvalidSchemaVersion;
		return Result;
	}
	if (Request.Kind != EAIRESyncOutboxOperationKind::Snapshot
		&& Request.Kind != EAIRESyncOutboxOperationKind::Event)
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::InvalidOperationKind;
		return Result;
	}
	if (!Request.OperationId.IsValid())
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::InvalidOperationId;
		return Result;
	}
	if (!IsCanonicalScope(Request.Scope))
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::InvalidScope;
		return Result;
	}
	if (Request.Body.IsEmpty() || !IsValidUtf8(Request.Body))
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::InvalidBody;
		return Result;
	}
	if (Request.Body.Num() > AIRESyncOutbox::MaxBodyBytes)
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::PayloadTooLarge;
		return Result;
	}
	if ((Request.Kind == EAIRESyncOutboxOperationKind::Snapshot
			&& !IsValidCoalescingKey(Request.CoalescingKey))
		|| (Request.Kind == EAIRESyncOutboxOperationKind::Event
			&& !Request.CoalescingKey.IsEmpty()))
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::InvalidCoalescingKey;
		return Result;
	}

	const FString BodyHash = ComputeBodyHash(Request.Body);
	if (!IsValidBodyHash(BodyHash))
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::InvalidBody;
		return Result;
	}
	Result.BodyHash = BodyHash;
	if (const int32 ExistingIndex = FindEntryIndex(Request.OperationId);
		Entries.IsValidIndex(ExistingIndex))
	{
		const FAIRESyncOutboxEntry& Existing = Entries[ExistingIndex];
		Result.Sequence = Existing.Sequence;
		Result.ExistingState = Existing.State;
		const bool bSame = Existing.SchemaVersion == Request.SchemaVersion
			&& Existing.Kind == Request.Kind
			&& Existing.Scope == Request.Scope
			&& Existing.CoalescingKey == Request.CoalescingKey
			&& Existing.BodyHash == BodyHash;
		Result.Code = bSame
			? EAIRESyncOutboxEnqueueCode::Duplicate
			: EAIRESyncOutboxEnqueueCode::Conflict;
		return Result;
	}
	if (NextSequence <= 0 || NextSequence == MAX_int64)
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::QueueFull;
		return Result;
	}

	TArray<FAIRESyncOutboxEntry> CandidateEntries = Entries;
	const int32 CoalescingIndex = FindCoalescingCandidate(Request);
	if (CandidateEntries.IsValidIndex(CoalescingIndex))
	{
		Result.SupersededOperationId =
			CandidateEntries[CoalescingIndex].OperationId;
		CandidateEntries.RemoveAt(CoalescingIndex);
	}
	if (CandidateEntries.Num() >= AIRESyncOutbox::MaxEntries)
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::QueueFull;
		return Result;
	}

	FAIRESyncOutboxEntry Entry;
	Entry.SchemaVersion = Request.SchemaVersion;
	Entry.Kind = Request.Kind;
	Entry.OperationId = Request.OperationId;
	Entry.BodyHash = BodyHash;
	Entry.Scope = Request.Scope;
	Entry.CoalescingKey = Request.CoalescingKey;
	Entry.Body = Request.Body;
	Entry.Sequence = NextSequence;
	Entry.State = EAIRESyncOutboxEntryState::Pending;
	Entry.CreatedAtUtc = FDateTime::UtcNow();
	CandidateEntries.Add(MoveTemp(Entry));

	FAIRESyncOutboxSaveEnvelope CandidateEnvelope;
	CandidateEnvelope.FormatVersion = AIRESyncOutbox::SaveFormatVersion;
	CandidateEnvelope.Generation =
		FMath::Max(LatestGeneration, HighestIssuedGeneration) + 1;
	CandidateEnvelope.Scope = MakeCanonicalScope();
	CandidateEnvelope.Entries = CandidateEntries;
	if (!DoesEnvelopeFit(CandidateEnvelope))
	{
		Result.Code = EAIRESyncOutboxEnqueueCode::QueueFull;
		return Result;
	}

	Entries = MoveTemp(CandidateEntries);
	Result.Sequence = NextSequence++;
	Result.ExistingState = EAIRESyncOutboxEntryState::Pending;
	Result.Code = Result.SupersededOperationId.IsValid()
		? EAIRESyncOutboxEnqueueCode::CoalescedPendingPersistence
		: EAIRESyncOutboxEnqueueCode::AcceptedPendingPersistence;
	bDirty = true;
	StartPersistence();
	return Result;
}

FAIRESyncOutboxPersistenceResult
UAIRESyncOutboxSubsystem::RequestPersistence()
{
	return StartPersistence();
}

EAIRESyncOutboxDispatchCode UAIRESyncOutboxSubsystem::DispatchNext()
{
	if (bShuttingDown)
	{
		return EAIRESyncOutboxDispatchCode::ShuttingDown;
	}
	if (!bReady)
	{
		return EAIRESyncOutboxDispatchCode::NotReady;
	}
	if (HasInFlightEntry() || ActiveAttemptToken.IsValid())
	{
		return EAIRESyncOutboxDispatchCode::AlreadyInFlight;
	}
	if (bDirty || bSaveInFlight)
	{
		StartPersistence();
		return EAIRESyncOutboxDispatchCode::PersistencePending;
	}
	if (!Transport.IsValid())
	{
		return EAIRESyncOutboxDispatchCode::NoTransport;
	}

	const int32 PendingIndex = FindNextPendingIndex();
	if (!Entries.IsValidIndex(PendingIndex))
	{
		return EAIRESyncOutboxDispatchCode::QueueEmpty;
	}
	FAIRESyncOutboxEntry& Entry = Entries[PendingIndex];
	Entry.State = EAIRESyncOutboxEntryState::InFlight;
	Entry.AttemptCount++;
	Entry.LastAttemptAtUtc = FDateTime::UtcNow();
	ActiveOperationId = Entry.OperationId;
	ActiveAttemptToken = FGuid::NewGuid();
	bStartTransportAfterSave = true;
	bDirty = true;
	StartPersistence();
	return EAIRESyncOutboxDispatchCode::StartedPersistence;
}

EAIRESyncOutboxCancelCode UAIRESyncOutboxSubsystem::CancelOperation(
	const FGuid& OperationId)
{
	if (bShuttingDown)
	{
		return EAIRESyncOutboxCancelCode::ShuttingDown;
	}
	if (!bReady)
	{
		return EAIRESyncOutboxCancelCode::NotReady;
	}
	const int32 EntryIndex = FindEntryIndex(OperationId);
	if (!Entries.IsValidIndex(EntryIndex))
	{
		return EAIRESyncOutboxCancelCode::NotFound;
	}
	if (Entries[EntryIndex].State == EAIRESyncOutboxEntryState::Pending)
	{
		Entries.RemoveAt(EntryIndex);
		bDirty = true;
		StartPersistence();
		return EAIRESyncOutboxCancelCode::RemovedPending;
	}
	if (Entries[EntryIndex].State == EAIRESyncOutboxEntryState::InFlight)
	{
		ReturnActiveAttemptToPending(true);
		StartPersistence();
		return EAIRESyncOutboxCancelCode::ReturnedInFlightToPending;
	}
	return EAIRESyncOutboxCancelCode::NotFound;
}

void UAIRESyncOutboxSubsystem::FlushBestEffort()
{
	if (!IsReady())
	{
		return;
	}
	if (bDirty || bSaveInFlight)
	{
		StartPersistence();
		return;
	}
	DispatchNext();
}

void UAIRESyncOutboxSubsystem::SetTransport(
	TSharedPtr<IAIRESyncOutboxTransport> InTransport)
{
	if (bShuttingDown)
	{
		return;
	}
	if (Transport.Get() == InTransport.Get())
	{
		return;
	}
	if (ActiveAttemptToken.IsValid())
	{
		ReturnActiveAttemptToPending(!bStartTransportAfterSave);
		StartPersistence();
	}
	Transport = MoveTemp(InTransport);
}

FAIRESyncOutboxPersistenceCompleted&
UAIRESyncOutboxSubsystem::OnPersistenceCompleted()
{
	return PersistenceCompletedDelegate;
}

void UAIRESyncOutboxSubsystem::BeginLoad()
{
	const uint64 LoadEpoch = ++PersistenceEpoch;
	LoadSlots.SetNum(2);
	LoadSlots[0].SlotName = AIRESyncOutbox::PrimarySlotName;
	LoadSlots[1].SlotName = AIRESyncOutbox::PreviousSlotName;
	LastPersistenceResult.Code = EAIRESyncOutboxPersistenceCode::InProgress;
	BeginLoadSlot(0, LoadEpoch);
	BeginLoadSlot(1, LoadEpoch);
}

void UAIRESyncOutboxSubsystem::BeginLoadSlot(
	const int32 SlotIndex,
	const uint64 LoadEpoch)
{
	if (!LoadSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	ISaveGameSystem* SaveSystem =
		IPlatformFeaturesModule::Get().GetSaveGameSystem();
	if (!SaveSystem)
	{
		HandleSlotExistence(SlotIndex, LoadEpoch, false, true);
		return;
	}

	const FString SlotName = LoadSlots[SlotIndex].SlotName;
	const FPlatformUserId PlatformUserId =
		FPlatformMisc::GetPlatformUserForUserIndex(AIRESyncOutbox::UserIndex);
	const TWeakObjectPtr<UAIRESyncOutboxSubsystem> WeakThis(this);
	SaveSystem->DoesSaveGameExistAsync(
		*SlotName,
		PlatformUserId,
		[WeakThis, SlotIndex, LoadEpoch](
			const FString&,
			FPlatformUserId,
			const ISaveGameSystem::ESaveExistsResult ExistsResult)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			const bool bExists =
				ExistsResult == ISaveGameSystem::ESaveExistsResult::OK
				|| ExistsResult == ISaveGameSystem::ESaveExistsResult::Corrupt;
			const bool bIoFailure =
				ExistsResult ==
				ISaveGameSystem::ESaveExistsResult::UnspecifiedError;
			WeakThis->HandleSlotExistence(
				SlotIndex,
				LoadEpoch,
				bExists,
				bIoFailure);
		});
}

void UAIRESyncOutboxSubsystem::HandleSlotExistence(
	const int32 SlotIndex,
	const uint64 LoadEpoch,
	const bool bExists,
	const bool bIoFailure)
{
	if (LoadEpoch != PersistenceEpoch
		|| bShuttingDown
		|| !LoadSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	FLoadSlotState& Slot = LoadSlots[SlotIndex];
	Slot.bExists = bExists;
	Slot.bIoFailure = bIoFailure;
	if (!bExists || bIoFailure)
	{
		Slot.bCompleted = true;
		FinalizeLoad(LoadEpoch);
		return;
	}

	const TWeakObjectPtr<UAIRESyncOutboxSubsystem> WeakThis(this);
	UGameplayStatics::AsyncLoadGameFromSlot(
		Slot.SlotName,
		AIRESyncOutbox::UserIndex,
		FAsyncLoadGameFromSlotDelegate::CreateWeakLambda(
			this,
			[WeakThis, SlotIndex, LoadEpoch](
				const FString&,
				const int32,
				USaveGame* LoadedGame)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleSlotLoaded(
						SlotIndex,
						LoadEpoch,
						LoadedGame);
				}
			}));
}

void UAIRESyncOutboxSubsystem::HandleSlotLoaded(
	const int32 SlotIndex,
	const uint64 LoadEpoch,
	USaveGame* LoadedGame)
{
	if (LoadEpoch != PersistenceEpoch
		|| bShuttingDown
		|| !LoadSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	FLoadSlotState& Slot = LoadSlots[SlotIndex];
	if (const UAIRESyncOutboxSaveGame* OutboxSave =
		Cast<UAIRESyncOutboxSaveGame>(LoadedGame))
	{
		Slot.LoadedEnvelope = OutboxSave->Envelope;
	}
	Slot.bCompleted = true;
	FinalizeLoad(LoadEpoch);
}

void UAIRESyncOutboxSubsystem::FinalizeLoad(const uint64 LoadEpoch)
{
	if (LoadEpoch != PersistenceEpoch
		|| bShuttingDown
		|| LoadSlots.Num() != 2
		|| LoadSlots.ContainsByPredicate(
			[](const FLoadSlotState& Slot)
			{
				return !Slot.bCompleted;
			}))
	{
		return;
	}

	int32 SelectedIndex = INDEX_NONE;
	FAIRESyncOutboxSaveEnvelope SelectedEnvelope;
	bool bSelectedNeedsCompact = false;
	bool bAnyInvalid = false;
	bool bAnyIoFailure = false;
	for (int32 Index = 0; Index < LoadSlots.Num(); ++Index)
	{
		const FLoadSlotState& Slot = LoadSlots[Index];
		bAnyIoFailure |= Slot.bIoFailure;
		if (!Slot.LoadedEnvelope.IsSet())
		{
			bAnyInvalid |= Slot.bExists && !Slot.bIoFailure;
			continue;
		}
		FAIRESyncOutboxSaveEnvelope Normalized;
		bool bNeedsCompact = false;
		if (!ValidateEnvelope(
			Slot.LoadedEnvelope.GetValue(),
			Normalized,
			bNeedsCompact))
		{
			bAnyInvalid = true;
			continue;
		}
		if (SelectedIndex == INDEX_NONE
			|| Normalized.Generation > SelectedEnvelope.Generation)
		{
			SelectedIndex = Index;
			SelectedEnvelope = MoveTemp(Normalized);
			bSelectedNeedsCompact = bNeedsCompact;
		}
		else if (Normalized.Generation == SelectedEnvelope.Generation)
		{
			SelectedIndex = INDEX_NONE;
			bAnyInvalid = true;
		}
	}

	Entries.Reset();
	PendingCompactionOperationIds.Reset();
	if (SelectedIndex != INDEX_NONE)
	{
		Entries = MoveTemp(SelectedEnvelope.Entries);
		LatestGeneration = SelectedEnvelope.Generation;
		HighestIssuedGeneration = LatestGeneration;
		LatestSlotName = LoadSlots[SelectedIndex].SlotName;
		LastIssuedSlotName = LatestSlotName;
		NextSequence = 1;
		for (const FAIRESyncOutboxEntry& Entry : Entries)
		{
			NextSequence = FMath::Max(NextSequence, Entry.Sequence + 1);
			if (Entry.State == EAIRESyncOutboxEntryState::Acked)
			{
				PendingCompactionOperationIds.Add(Entry.OperationId);
			}
		}
		bReady = true;
		bDirty = bSelectedNeedsCompact;
		const bool bUsedFallback = bAnyInvalid || bAnyIoFailure;
		LastPersistenceResult.Code =
			bUsedFallback
				? EAIRESyncOutboxPersistenceCode::SucceededWithFallback
				: EAIRESyncOutboxPersistenceCode::Succeeded;
		LastPersistenceResult.Generation = LatestGeneration;
		LastPersistenceResult.bUsedFallback = bUsedFallback;
		PersistenceCompletedDelegate.Broadcast(LastPersistenceResult);
		if (bDirty)
		{
			StartPersistence();
		}
		return;
	}

	bReady = true;
	NextSequence = 1;
	LastPersistenceResult.Code = bAnyIoFailure
		? EAIRESyncOutboxPersistenceCode::IoFailure
		: EAIRESyncOutboxPersistenceCode::SafeEmptyNoValidSave;
	LastPersistenceResult.Generation = 0;
	LastPersistenceResult.bUsedFallback = false;
	PersistenceCompletedDelegate.Broadcast(LastPersistenceResult);
}

bool UAIRESyncOutboxSubsystem::ValidateEnvelope(
	const FAIRESyncOutboxSaveEnvelope& Envelope,
	FAIRESyncOutboxSaveEnvelope& OutNormalized,
	bool& bOutNeedsCompact) const
{
	bOutNeedsCompact = false;
	if (Envelope.FormatVersion != AIRESyncOutbox::SaveFormatVersion
		|| Envelope.Generation <= 0
		|| Envelope.Generation == MAX_int64
		|| !IsCanonicalScope(Envelope.Scope)
		|| Envelope.Entries.Num() > AIRESyncOutbox::MaxEntries
		|| !DoesEnvelopeFit(Envelope))
	{
		return false;
	}

	TSet<FGuid> OperationIds;
	int64 PreviousSequence = 0;
	OutNormalized = Envelope;
	OutNormalized.Entries.Reset();
	for (FAIRESyncOutboxEntry Entry : Envelope.Entries)
	{
		if (Entry.SchemaVersion <= 0
			|| (Entry.Kind != EAIRESyncOutboxOperationKind::Snapshot
				&& Entry.Kind != EAIRESyncOutboxOperationKind::Event)
			|| !Entry.OperationId.IsValid()
			|| OperationIds.Contains(Entry.OperationId)
			|| Entry.Scope != Envelope.Scope
			|| Entry.Body.IsEmpty()
			|| Entry.Body.Num() > AIRESyncOutbox::MaxBodyBytes
			|| !IsValidUtf8(Entry.Body)
			|| !IsValidBodyHash(Entry.BodyHash)
			|| ComputeBodyHash(Entry.Body) != Entry.BodyHash
			|| Entry.Sequence <= PreviousSequence
			|| Entry.Sequence == MAX_int64
			|| Entry.AttemptCount < 0
			|| Entry.AttemptCount == MAX_int32
			|| Entry.CreatedAtUtc.GetTicks() <= 0
			|| (Entry.AttemptCount > 0
				&& Entry.LastAttemptAtUtc.GetTicks() <= 0)
			|| (Entry.Kind == EAIRESyncOutboxOperationKind::Snapshot
				&& !IsValidCoalescingKey(Entry.CoalescingKey))
			|| (Entry.Kind == EAIRESyncOutboxOperationKind::Event
				&& !Entry.CoalescingKey.IsEmpty()))
		{
			return false;
		}
		OperationIds.Add(Entry.OperationId);
		PreviousSequence = Entry.Sequence;
		switch (Entry.State)
		{
		case EAIRESyncOutboxEntryState::Pending:
			OutNormalized.Entries.Add(MoveTemp(Entry));
			break;
		case EAIRESyncOutboxEntryState::InFlight:
			Entry.State = EAIRESyncOutboxEntryState::Pending;
			OutNormalized.Entries.Add(MoveTemp(Entry));
			bOutNeedsCompact = true;
			break;
		case EAIRESyncOutboxEntryState::Acked:
			OutNormalized.Entries.Add(MoveTemp(Entry));
			bOutNeedsCompact = true;
			break;
		default:
			return false;
		}
	}
	return true;
}

bool UAIRESyncOutboxSubsystem::DoesEnvelopeFit(
	const FAIRESyncOutboxSaveEnvelope& Envelope) const
{
	UAIRESyncOutboxSaveGame* SaveGame =
		NewObject<UAIRESyncOutboxSaveGame>();
	SaveGame->Envelope = Envelope;
	TArray<uint8> SerializedBytes;
	return UGameplayStatics::SaveGameToMemory(SaveGame, SerializedBytes)
		&& SerializedBytes.Num() <= AIRESyncOutbox::MaxPersistedBytes;
}

FAIRESyncOutboxSaveEnvelope UAIRESyncOutboxSubsystem::BuildEnvelope(
	const int64 Generation) const
{
	FAIRESyncOutboxSaveEnvelope Envelope;
	Envelope.FormatVersion = AIRESyncOutbox::SaveFormatVersion;
	Envelope.Generation = Generation;
	Envelope.Scope = MakeCanonicalScope();
	for (const FAIRESyncOutboxEntry& Entry : Entries)
	{
		if (!PendingCompactionOperationIds.Contains(Entry.OperationId))
		{
			Envelope.Entries.Add(Entry);
		}
	}
	return Envelope;
}

FAIRESyncOutboxPersistenceResult
UAIRESyncOutboxSubsystem::StartPersistence()
{
	if (bShuttingDown)
	{
		LastPersistenceResult.Code =
			EAIRESyncOutboxPersistenceCode::ShuttingDown;
		return LastPersistenceResult;
	}
	if (!bReady)
	{
		LastPersistenceResult.Code =
			EAIRESyncOutboxPersistenceCode::InProgress;
		return LastPersistenceResult;
	}
	if (bSaveInFlight)
	{
		LastPersistenceResult.Code =
			EAIRESyncOutboxPersistenceCode::InProgress;
		return LastPersistenceResult;
	}
	if (!bDirty)
	{
		LastPersistenceResult.Code =
			EAIRESyncOutboxPersistenceCode::NoChanges;
		LastPersistenceResult.Generation = LatestGeneration;
		return LastPersistenceResult;
	}

	const int64 GenerationBase =
		FMath::Max(LatestGeneration, HighestIssuedGeneration);
	if (GenerationBase == MAX_int64)
	{
		LastPersistenceResult.Code =
			EAIRESyncOutboxPersistenceCode::InvalidEnvelope;
		return LastPersistenceResult;
	}
	const int64 Generation = GenerationBase + 1;
	FAIRESyncOutboxSaveEnvelope Envelope = BuildEnvelope(Generation);
	if (!DoesEnvelopeFit(Envelope))
	{
		LastPersistenceResult.Code =
			EAIRESyncOutboxPersistenceCode::InvalidEnvelope;
		return LastPersistenceResult;
	}

	UAIRESyncOutboxSaveGame* SaveGame =
		NewObject<UAIRESyncOutboxSaveGame>();
	SaveGame->Envelope = MoveTemp(Envelope);
	const FString TargetSlotName = GetNextSlotName();
	const uint64 SaveEpoch = ++ActiveSaveEpoch;
	HighestIssuedGeneration = Generation;
	LastIssuedSlotName = TargetSlotName;
	bSaveInFlight = true;
	bCompactionSaveInFlight = !PendingCompactionOperationIds.IsEmpty();
	bDirty = false;
	LastPersistenceResult.Code = EAIRESyncOutboxPersistenceCode::InProgress;
	LastPersistenceResult.Generation = Generation;
	LastPersistenceResult.bUsedFallback = false;

	const TWeakObjectPtr<UAIRESyncOutboxSubsystem> WeakThis(this);
	UGameplayStatics::AsyncSaveGameToSlot(
		SaveGame,
		TargetSlotName,
		AIRESyncOutbox::UserIndex,
		FAsyncSaveGameToSlotDelegate::CreateWeakLambda(
			this,
			[WeakThis, SaveEpoch, Generation](
				const FString& SlotName,
				const int32,
				const bool bSucceeded)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleSaveCompleted(
						SlotName,
						SaveEpoch,
						Generation,
						bSucceeded);
				}
			}));
	return LastPersistenceResult;
}

void UAIRESyncOutboxSubsystem::HandleSaveCompleted(
	const FString& SlotName,
	const uint64 SaveEpoch,
	const int64 Generation,
	const bool bSucceeded)
{
	if (bShuttingDown
		|| SaveEpoch != ActiveSaveEpoch
		|| Generation != HighestIssuedGeneration
		|| SlotName != LastIssuedSlotName)
	{
		return;
	}
	bSaveInFlight = false;
	if (!bSucceeded)
	{
		bDirty = true;
		bCompactionSaveInFlight = false;
		LastIssuedSlotName = LatestSlotName;
		if (bStartTransportAfterSave)
		{
			ReturnActiveAttemptToPending(false);
		}
		LastPersistenceResult.Code = EAIRESyncOutboxPersistenceCode::IoFailure;
		LastPersistenceResult.Generation = Generation;
		PersistenceCompletedDelegate.Broadcast(LastPersistenceResult);
		return;
	}

	LatestGeneration = Generation;
	HighestIssuedGeneration = Generation;
	LatestSlotName = SlotName;
	LastIssuedSlotName = SlotName;
	LastPersistenceResult.Code = EAIRESyncOutboxPersistenceCode::Succeeded;
	LastPersistenceResult.Generation = Generation;
	LastPersistenceResult.bUsedFallback = false;
	PersistenceCompletedDelegate.Broadcast(LastPersistenceResult);

	if (bCompactionSaveInFlight)
	{
		FinalizeAckedCompaction();
	}
	else
	{
		ScheduleAckedCompaction();
	}
	bCompactionSaveInFlight = false;
	if (bDirty)
	{
		StartPersistence();
		return;
	}
	if (bStartTransportAfterSave)
	{
		BeginActiveTransport();
	}
}

FString UAIRESyncOutboxSubsystem::GetNextSlotName() const
{
	return LatestSlotName == AIRESyncOutbox::PrimarySlotName
		? FString(AIRESyncOutbox::PreviousSlotName)
		: FString(AIRESyncOutbox::PrimarySlotName);
}

int32 UAIRESyncOutboxSubsystem::FindEntryIndex(
	const FGuid& OperationId) const
{
	return Entries.IndexOfByPredicate(
		[&OperationId](const FAIRESyncOutboxEntry& Entry)
		{
			return Entry.OperationId == OperationId;
		});
}

int32 UAIRESyncOutboxSubsystem::FindCoalescingCandidate(
	const FAIRESyncOutboxEnqueueRequest& Request) const
{
	if (Request.Kind != EAIRESyncOutboxOperationKind::Snapshot)
	{
		return INDEX_NONE;
	}
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		const FAIRESyncOutboxEntry& Entry = Entries[Index];
		if (Entry.Kind == EAIRESyncOutboxOperationKind::Event
			|| Entry.State != EAIRESyncOutboxEntryState::Pending)
		{
			break;
		}
		if (Entry.State == EAIRESyncOutboxEntryState::Pending
			&& Entry.Kind == EAIRESyncOutboxOperationKind::Snapshot
			&& Entry.Scope == Request.Scope
			&& Entry.CoalescingKey == Request.CoalescingKey)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UAIRESyncOutboxSubsystem::FindNextPendingIndex() const
{
	int32 SelectedIndex = INDEX_NONE;
	int64 SelectedSequence = MAX_int64;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].State == EAIRESyncOutboxEntryState::Pending
			&& Entries[Index].Sequence < SelectedSequence)
		{
			SelectedIndex = Index;
			SelectedSequence = Entries[Index].Sequence;
		}
	}
	return SelectedIndex;
}

bool UAIRESyncOutboxSubsystem::HasInFlightEntry() const
{
	return Entries.ContainsByPredicate(
		[](const FAIRESyncOutboxEntry& Entry)
		{
			return Entry.State == EAIRESyncOutboxEntryState::InFlight;
		});
}

void UAIRESyncOutboxSubsystem::BeginActiveTransport()
{
	bStartTransportAfterSave = false;
	const int32 EntryIndex = FindEntryIndex(ActiveOperationId);
	if (!Entries.IsValidIndex(EntryIndex)
		|| Entries[EntryIndex].State != EAIRESyncOutboxEntryState::InFlight
		|| !ActiveAttemptToken.IsValid()
		|| !Transport.IsValid())
	{
		ReturnActiveAttemptToPending(false);
		StartPersistence();
		return;
	}

	FAIRESyncOutboxTransportAttempt Attempt;
	Attempt.AttemptToken = ActiveAttemptToken;
	Attempt.Entry = Entries[EntryIndex];
	const uint64 CallbackLifecycleEpoch = LifecycleEpoch;
	const TWeakObjectPtr<UAIRESyncOutboxSubsystem> WeakThis(this);
	AttemptTimeoutTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UAIRESyncOutboxSubsystem::HandleAttemptTimeout),
		AIRESyncOutbox::AttemptTimeoutSeconds);
	const bool bAccepted = Transport->StartAttempt(
		Attempt,
		[WeakThis, CallbackLifecycleEpoch](
			const FAIRESyncOutboxTransportResult& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleTransportResult(
					CallbackLifecycleEpoch,
					Result);
			}
		});
	if (!bAccepted)
	{
		ReturnActiveAttemptToPending(false);
		StartPersistence();
	}
}

void UAIRESyncOutboxSubsystem::HandleTransportResult(
	const uint64 CallbackLifecycleEpoch,
	const FAIRESyncOutboxTransportResult& Result)
{
	if (!ensure(IsInGameThread())
		|| bShuttingDown
		|| CallbackLifecycleEpoch != LifecycleEpoch
		|| Result.AttemptToken != ActiveAttemptToken
		|| Result.OperationId != ActiveOperationId)
	{
		return;
	}
	const int32 EntryIndex = FindEntryIndex(ActiveOperationId);
	if (!Entries.IsValidIndex(EntryIndex)
		|| Entries[EntryIndex].State != EAIRESyncOutboxEntryState::InFlight)
	{
		return;
	}
	FAIRESyncOutboxEntry& Entry = Entries[EntryIndex];
	if (Result.Code == EAIRESyncOutboxTransportResultCode::Acked)
	{
		if (Result.BodyHash != Entry.BodyHash)
		{
			return;
		}
		Entry.State = EAIRESyncOutboxEntryState::Acked;
		ClearActiveAttempt();
		bDirty = true;
		StartPersistence();
		return;
	}
	ReturnActiveAttemptToPending(false);
	StartPersistence();
}

bool UAIRESyncOutboxSubsystem::HandleAttemptTimeout(const float DeltaTime)
{
	(void)DeltaTime;
	AttemptTimeoutTickerHandle.Reset();
	ReturnActiveAttemptToPending(true);
	StartPersistence();
	return false;
}

void UAIRESyncOutboxSubsystem::ReturnActiveAttemptToPending(
	const bool bCancelTransport)
{
	const FGuid AttemptToken = ActiveAttemptToken;
	const int32 EntryIndex = FindEntryIndex(ActiveOperationId);
	if (Entries.IsValidIndex(EntryIndex)
		&& Entries[EntryIndex].State == EAIRESyncOutboxEntryState::InFlight)
	{
		Entries[EntryIndex].State = EAIRESyncOutboxEntryState::Pending;
		bDirty = true;
	}
	ClearActiveAttempt();
	if (bCancelTransport && AttemptToken.IsValid() && Transport.IsValid())
	{
		Transport->CancelAttempt(AttemptToken);
	}
}

void UAIRESyncOutboxSubsystem::ClearActiveAttempt()
{
	if (AttemptTimeoutTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(AttemptTimeoutTickerHandle);
		AttemptTimeoutTickerHandle.Reset();
	}
	ActiveAttemptToken.Invalidate();
	ActiveOperationId.Invalidate();
	bStartTransportAfterSave = false;
}

void UAIRESyncOutboxSubsystem::ScheduleAckedCompaction()
{
	for (const FAIRESyncOutboxEntry& Entry : Entries)
	{
		if (Entry.State == EAIRESyncOutboxEntryState::Acked)
		{
			PendingCompactionOperationIds.Add(Entry.OperationId);
		}
	}
	if (!PendingCompactionOperationIds.IsEmpty())
	{
		bDirty = true;
	}
}

void UAIRESyncOutboxSubsystem::FinalizeAckedCompaction()
{
	Entries.RemoveAll(
		[this](const FAIRESyncOutboxEntry& Entry)
		{
			return PendingCompactionOperationIds.Contains(Entry.OperationId);
		});
	PendingCompactionOperationIds.Reset();
}

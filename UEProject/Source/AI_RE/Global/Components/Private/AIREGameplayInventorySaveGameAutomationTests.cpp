#include "AIREGameplayInventorySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIREGameplayInventorySaveGame.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

class FAIREGameplayInventoryPersistenceTestAccess
{
public:
	static FAIREInventorySessionScope GetCanonicalScope()
	{
		return UAIREGameplayInventorySubsystem::
			MakeCanonicalPersistenceScope();
	}

	static void PrepareTransientSession(
		UAIREGameplayInventorySubsystem& Inventory)
	{
		Inventory.ResetInventorySession(GetCanonicalScope());
		Inventory.CachedPlayerPersistenceState =
			FAIREInventoryPersistedPlayerState();
		Inventory.CachedPlayerPersistenceState.InventoryCapacity =
			AIREGameplayInventoryPersistence::PlayerInventoryCapacity;
		Inventory.bHasPlayerPersistenceState = true;
		Inventory.bPersistenceLifecycleInitialized = false;
		Inventory.bPersistenceDirty = false;
		Inventory.LastPersistenceSaveResult =
			FAIREInventoryPersistenceResult();
	}

	static bool CapturePlayerState(
		UAIREGameplayInventorySubsystem& Inventory,
		UAI_REPlayerInventoryComponent& PlayerInventory,
		const int64 Revision,
		const FName EquippedWeaponItemId)
	{
		PlayerInventory.MaxSlots =
			AIREGameplayInventoryPersistence::PlayerInventoryCapacity;
		PlayerInventory.Revision = Revision;
		PlayerInventory.EquippedWeaponItemId = EquippedWeaponItemId;
		EAIREInventoryPersistenceResultCode CaptureCode =
			EAIREInventoryPersistenceResultCode::NotStarted;
		const bool bCaptured = Inventory.CapturePlayerPersistenceState(
			PlayerInventory,
			Inventory.CachedPlayerPersistenceState,
			CaptureCode);
		Inventory.bHasPlayerPersistenceState = bCaptured;
		return bCaptured;
	}

	static bool RegisterPlayerForRestore(
		UAIREGameplayInventorySubsystem& Inventory,
		UAI_REPlayerInventoryComponent& PlayerInventory)
	{
		return Inventory.RegisterPlayerInventory(
			&PlayerInventory,
			nullptr);
	}

	static bool BuildEnvelope(
		const UAIREGameplayInventorySubsystem& Inventory,
		const int64 Generation,
		FAIREInventorySaveEnvelope& OutEnvelope,
		EAIREInventoryPersistenceResultCode& OutCode)
	{
		return Inventory.BuildPersistenceEnvelope(
			Generation,
			OutEnvelope,
			OutCode);
	}

	static bool ValidateEnvelope(
		const UAIREGameplayInventorySubsystem& Inventory,
		const FAIREInventorySaveEnvelope& Envelope,
		FAIREInventorySaveEnvelope& OutNormalizedEnvelope,
		EAIREInventoryPersistenceResultCode& OutCode)
	{
		return Inventory.ValidatePersistenceEnvelope(
			Envelope,
			OutNormalizedEnvelope,
			OutCode);
	}

	static bool CommitEnvelope(
		UAIREGameplayInventorySubsystem& Inventory,
		const FAIREInventorySaveEnvelope& Envelope)
	{
		return Inventory.CommitPersistenceEnvelope(Envelope);
	}

	static void SetStableEquipment(
		UAIREGameplayInventorySubsystem& Inventory,
		const FName ItemId)
	{
		UAIREGameplayInventorySubsystem::FAIREContainerState* Mako =
			Inventory.FindContainer(
				UAIREGameplayInventorySubsystem::GetMakoContainerId());
		check(Mako);
		Mako->EquippedItemId = ItemId;
		Mako->PendingItemId = NAME_None;
		Mako->PreviousItemId = NAME_None;
		Mako->EquipmentTransition = EAIREEquipmentTransitionState::Idle;
		Mako->EquipmentMutationId.Invalidate();
		Mako->ReservedSlotIndex = INDEX_NONE;
		++Mako->Revision;
	}

	static void SetEquipmentTransition(
		UAIREGameplayInventorySubsystem& Inventory,
		const EAIREEquipmentTransitionState Transition)
	{
		UAIREGameplayInventorySubsystem::FAIREContainerState* Mako =
			Inventory.FindContainer(
				UAIREGameplayInventorySubsystem::GetMakoContainerId());
		check(Mako);
		Mako->EquipmentTransition = Transition;
		Mako->PendingItemId = FName(TEXT("AIRE.Test.WeaponB"));
		Mako->PreviousItemId = Mako->EquippedItemId;
		Mako->EquipmentMutationId = FGuid::NewGuid();
		Mako->ReservedSlotIndex = 0;
	}

	static void RecordWork(
		UAIREGameplayInventorySubsystem& Inventory,
		const FGuid& WorkId,
		const FAIREInventoryWorkResult& Result)
	{
		Inventory.RecordAppliedWorkResult(WorkId, Result, true);
	}

	static void RecordMutation(
		UAIREGameplayInventorySubsystem& Inventory,
		const FAIREInventoryMutationResult& Result)
	{
		Inventory.RecordAppliedMutation(Result, true);
	}

	static void RecordImports(
		UAIREGameplayInventorySubsystem& Inventory,
		const FString& CandidateId,
		const TArray<FString>& OperationIds)
	{
		Inventory.RecordAppliedImportCandidateId(CandidateId);
		Inventory.RecordAppliedImportOperationIds(OperationIds);
	}

	static void FinalizeLoadFromMemory(
		UAIREGameplayInventorySubsystem& Inventory,
		const TOptional<FAIREInventorySaveEnvelope>& Primary,
		const TOptional<FAIREInventorySaveEnvelope>& Previous)
	{
		PrepareTransientSession(Inventory);
		Inventory.bPersistenceReady = false;
		Inventory.bPersistenceLoadComplete = false;
		Inventory.bMakoInventoryInitialized = false;
		const uint64 LoadEpoch = ++Inventory.PersistenceEpoch;
		Inventory.PersistenceLoadSlots.SetNum(2);
		Inventory.PersistenceLoadSlots[0].SlotName =
			AIREGameplayInventoryPersistence::PrimarySlotName;
		Inventory.PersistenceLoadSlots[0].bCompleted = true;
		Inventory.PersistenceLoadSlots[0].bExists = Primary.IsSet();
		Inventory.PersistenceLoadSlots[0].LoadedEnvelope = Primary;
		Inventory.PersistenceLoadSlots[1].SlotName =
			AIREGameplayInventoryPersistence::PreviousSlotName;
		Inventory.PersistenceLoadSlots[1].bCompleted = true;
		Inventory.PersistenceLoadSlots[1].bExists = Previous.IsSet();
		Inventory.PersistenceLoadSlots[1].LoadedEnvelope = Previous;
		Inventory.FinalizePersistenceLoad(LoadEpoch);
	}

	static uint64 PrimeSaveCompletion(
		UAIREGameplayInventorySubsystem& Inventory,
		const int64 LatestGeneration,
		const FString& LatestSlotName,
		const int64 IssuedGeneration,
		const FString& IssuedSlotName,
		const bool bDirty)
	{
		Inventory.LatestPersistenceGeneration = LatestGeneration;
		Inventory.LatestPersistenceSlotName = LatestSlotName;
		Inventory.HighestIssuedPersistenceGeneration = IssuedGeneration;
		Inventory.LastIssuedPersistenceSlotName = IssuedSlotName;
		Inventory.bPersistenceSaveInFlight = true;
		Inventory.bPersistenceDirty = bDirty;
		return ++Inventory.ActiveSaveEpoch;
	}

	static void CompleteSave(
		UAIREGameplayInventorySubsystem& Inventory,
		const FString& SlotName,
		const uint64 SaveEpoch,
		const FGuid& SessionId,
		const int64 Generation,
		const bool bSucceeded)
	{
		Inventory.HandlePersistenceSaveCompleted(
			SlotName,
			SaveEpoch,
			SessionId,
			Generation,
			bSucceeded);
	}

	static int64 GetLatestGeneration(
		const UAIREGameplayInventorySubsystem& Inventory)
	{
		return Inventory.LatestPersistenceGeneration;
	}

	static FString GetLatestSlotName(
		const UAIREGameplayInventorySubsystem& Inventory)
	{
		return Inventory.LatestPersistenceSlotName;
	}

	static bool IsDirty(const UAIREGameplayInventorySubsystem& Inventory)
	{
		return Inventory.bPersistenceDirty;
	}

	static bool IsSaveInFlight(
		const UAIREGameplayInventorySubsystem& Inventory)
	{
		return Inventory.bPersistenceSaveInFlight;
	}
};

namespace
{
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> MakeInventory()
	{
		TStrongObjectPtr<UGameInstance> GameInstance(
			NewObject<UGameInstance>());
		return TStrongObjectPtr<UAIREGameplayInventorySubsystem>(
			NewObject<UAIREGameplayInventorySubsystem>(GameInstance.Get()));
	}

	FAIREInventoryMutationRequest MakeAddRequest(
		UAIREGameplayInventorySubsystem& Inventory,
		const FName ContainerId,
		const FName ItemId,
		const int32 Count)
	{
		FAIREInventoryContainerSnapshot Snapshot;
		check(Inventory.GetContainerSnapshot(ContainerId, Snapshot));
		FAIREInventoryMutationRequest Request;
		Request.SessionId = Snapshot.SessionId;
		Request.MutationId = FGuid::NewGuid();
		Request.ContainerId = ContainerId;
		Request.ExpectedRevision = Snapshot.Revision;
		Request.ItemId = ItemId;
		Request.Count = Count;
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREGameplayInventorySaveGameRoundTripTest,
	"AIRE.Inventory.SaveGame.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIREGameplayInventorySaveGameRoundTripTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> Source = MakeInventory();
	FAIREGameplayInventoryPersistenceTestAccess::PrepareTransientSession(
		*Source);

	const FAIREInventoryMutationRequest MakoAdd = MakeAddRequest(
		*Source,
		UAIREGameplayInventorySubsystem::GetMakoContainerId(),
		FName(TEXT("AIRE.Test.GenericStack4")),
		3);
	const FAIREInventoryMutationRequest StorageAdd = MakeAddRequest(
		*Source,
		UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
		FName(TEXT("AIRE.Test.Stack2")),
		2);
	TestEqual(
		TEXT("MAKO setup succeeds"),
		Source->TryAddItem(MakoAdd).Code,
		EAIREInventoryMutationCode::Succeeded);
	TestEqual(
		TEXT("Storage setup succeeds"),
		Source->TryAddItem(StorageAdd).Code,
		EAIREInventoryMutationCode::Succeeded);
	FAIREGameplayInventoryPersistenceTestAccess::SetStableEquipment(
		*Source,
		FName(TEXT("AIRE.Test.WeaponA")));
	TStrongObjectPtr<UAI_REPlayerInventoryComponent> SourcePlayer(
		NewObject<UAI_REPlayerInventoryComponent>());
	FInventoryItemStack& PlayerInventoryStack =
		SourcePlayer->Items.AddDefaulted_GetRef();
	PlayerInventoryStack.SlotIndex = 4;
	PlayerInventoryStack.ItemId = FName(TEXT("AIRE.Test.Stack4"));
	PlayerInventoryStack.Count = 3;
	FInventoryItemStack& PlayerQuickSlotStack =
		SourcePlayer->Items.AddDefaulted_GetRef();
	PlayerQuickSlotStack.SlotIndex = 100;
	PlayerQuickSlotStack.ItemId = FName(TEXT("AIRE.Test.Stack2"));
	PlayerQuickSlotStack.Count = 1;
	TestTrue(
		TEXT("Player save fixture validates"),
		FAIREGameplayInventoryPersistenceTestAccess::CapturePlayerState(
			*Source,
			*SourcePlayer,
			7,
			FName(TEXT("AIRE.Test.WeaponA"))));

	const FGuid WorkId = FGuid::NewGuid();
	FAIREInventoryWorkResult WorkResult;
	WorkResult.Code = EAIREInventoryMutationCode::Succeeded;
	WorkResult.Destination =
		EAIREInventoryWorkResultDestination::SharedStorage;
	WorkResult.DeliveredItem.ItemId = FName(TEXT("AIRE.Test.Stack2"));
	WorkResult.DeliveredItem.Count = 1;
	FAIREInventoryContainerSnapshot MakoBefore;
	FAIREInventoryContainerSnapshot StorageBefore;
	Source->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetMakoContainerId(),
		MakoBefore);
	Source->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
		StorageBefore);
	WorkResult.MakoRevision = MakoBefore.Revision;
	WorkResult.StorageRevision = StorageBefore.Revision;
	FAIREGameplayInventoryPersistenceTestAccess::RecordWork(
		*Source,
		WorkId,
		WorkResult);
	FAIREGameplayInventoryPersistenceTestAccess::RecordImports(
		*Source,
		TEXT("candidate.roundtrip"),
		{ TEXT("operation.roundtrip") });

	FAIREInventorySaveEnvelope SourceEnvelope;
	EAIREInventoryPersistenceResultCode BuildCode =
		EAIREInventoryPersistenceResultCode::NotStarted;
	TestTrue(
		TEXT("A valid envelope is built"),
		FAIREGameplayInventoryPersistenceTestAccess::BuildEnvelope(
			*Source,
			1,
			SourceEnvelope,
			BuildCode));
	TestEqual(
		TEXT("Build result is success"),
		BuildCode,
		EAIREInventoryPersistenceResultCode::Succeeded);

	TStrongObjectPtr<UAIREGameplayInventorySaveGame> SaveGame(
		NewObject<UAIREGameplayInventorySaveGame>());
	SaveGame->Envelope = SourceEnvelope;
	TArray<uint8> SaveBytes;
	TestTrue(
		TEXT("SaveGame serializes to memory"),
		UGameplayStatics::SaveGameToMemory(SaveGame.Get(), SaveBytes));
	TStrongObjectPtr<UAIREGameplayInventorySaveGame> LoadedSave(
		Cast<UAIREGameplayInventorySaveGame>(
			UGameplayStatics::LoadGameFromMemory(SaveBytes)));
	TestNotNull(TEXT("SaveGame deserializes from memory"), LoadedSave.Get());
	if (!LoadedSave)
	{
		return false;
	}

	TStrongObjectPtr<UAIREGameplayInventorySubsystem> Restored = MakeInventory();
	FAIREGameplayInventoryPersistenceTestAccess::PrepareTransientSession(
		*Restored);
	const FGuid RestoredRuntimeSession = Restored->GetInventorySessionId();
	FAIREInventorySaveEnvelope Normalized;
	EAIREInventoryPersistenceResultCode ValidationCode =
		EAIREInventoryPersistenceResultCode::NotStarted;
	TestTrue(
		TEXT("Round-trip envelope validates"),
		FAIREGameplayInventoryPersistenceTestAccess::ValidateEnvelope(
			*Restored,
			LoadedSave->Envelope,
			Normalized,
			ValidationCode));
	TestTrue(
		TEXT("Envelope commits atomically"),
		FAIREGameplayInventoryPersistenceTestAccess::CommitEnvelope(
			*Restored,
			Normalized));
	TestTrue(
		TEXT("Runtime session remains new"),
		Restored->GetInventorySessionId() == RestoredRuntimeSession);
	TestTrue(
		TEXT("Saved source session is not restored"),
		Restored->GetInventorySessionId()
			!= SourceEnvelope.SourceSessionId);

	FAIREInventoryContainerSnapshot MakoAfter;
	FAIREInventoryContainerSnapshot StorageAfter;
	Restored->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetMakoContainerId(),
		MakoAfter);
	Restored->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
		StorageAfter);
	TestEqual(TEXT("MAKO revision restores"), MakoAfter.Revision, MakoBefore.Revision);
	TestEqual(TEXT("Storage revision restores"), StorageAfter.Revision, StorageBefore.Revision);
	TestEqual(TEXT("MAKO stack count restores"), MakoAfter.ItemStacks.Num(), 1);
	TestEqual(TEXT("Storage stack count restores"), StorageAfter.ItemStacks.Num(), 1);
	if (!MakoAfter.ItemStacks.IsEmpty())
	{
		TestEqual(
			TEXT("Player-compatible generic item restores in MAKO"),
			MakoAfter.ItemStacks[0].ItemId,
			FName(TEXT("AIRE.Test.GenericStack4")));
		TestEqual(
			TEXT("Generic MAKO item count restores"),
			MakoAfter.ItemStacks[0].Count,
			3);
	}
	TestEqual(
		TEXT("Stable equipment restores"),
		MakoAfter.Equipment.EquippedItemId,
		FName(TEXT("AIRE.Test.WeaponA")));
	TestTrue(TEXT("Pending equipment is not restored"), MakoAfter.Equipment.PendingItemId.IsNone());
	TStrongObjectPtr<UAI_REPlayerInventoryComponent> RestoredPlayer(
		NewObject<UAI_REPlayerInventoryComponent>());
	TestTrue(
		TEXT("Restored player registers"),
		FAIREGameplayInventoryPersistenceTestAccess::RegisterPlayerForRestore(
			*Restored,
			*RestoredPlayer));
	TestEqual(
		TEXT("Player inventory capacity restores"),
		RestoredPlayer->MaxSlots,
		AIREGameplayInventoryPersistence::PlayerInventoryCapacity);
	TestEqual(
		TEXT("Player revision restores"),
		RestoredPlayer->GetInventoryRevision(),
		static_cast<int64>(7));
	TestEqual(
		TEXT("Player equipped weapon restores"),
		RestoredPlayer->GetEquippedWeaponItemId(),
		FName(TEXT("AIRE.Test.WeaponA")));
	TestTrue(
		TEXT("Player physical slot restores"),
		RestoredPlayer->Items.ContainsByPredicate(
			[](const FInventoryItemStack& Stack)
			{
				return Stack.SlotIndex == 4
					&& Stack.ItemId == FName(TEXT("AIRE.Test.Stack4"))
					&& Stack.Count == 3;
			}));
	TestTrue(
		TEXT("Player quick slot 100 restores"),
		RestoredPlayer->Items.ContainsByPredicate(
			[](const FInventoryItemStack& Stack)
			{
				return Stack.SlotIndex == 100
					&& Stack.ItemId == FName(TEXT("AIRE.Test.Stack2"))
					&& Stack.Count == 1;
			}));

	FAIREInventoryMutationRequest ReplayMutation = MakoAdd;
	ReplayMutation.SessionId = Restored->GetInventorySessionId();
	ReplayMutation.ExpectedRevision = MakoAfter.Revision;
	TestEqual(
		TEXT("Mutation replay is already applied after restart"),
		Restored->TryAddItem(ReplayMutation).Code,
		EAIREInventoryMutationCode::AlreadyApplied);
	FAIREMakoWorkRewardRequest ReplayWork;
	ReplayWork.SessionId = Restored->GetInventorySessionId();
	ReplayWork.DeliveryId = WorkId;
	ReplayWork.ExpectedMakoRevision = MakoAfter.Revision;
	ReplayWork.ExpectedStorageRevision = StorageAfter.Revision;
	ReplayWork.Reward = WorkResult.DeliveredItem;
	TestEqual(
		TEXT("Work replay is already applied after restart"),
		Restored->TryStoreMakoWorkReward(ReplayWork).Code,
		EAIREInventoryMutationCode::AlreadyApplied);

	FAIREInventoryStartupImportCandidate ReplayImport;
	ReplayImport.CandidateId = TEXT("candidate.roundtrip");
	ReplayImport.Scope =
		FAIREGameplayInventoryPersistenceTestAccess::GetCanonicalScope();
	ReplayImport.SessionId = Restored->GetInventorySessionId();
	ReplayImport.ContainerId =
		UAIREGameplayInventorySubsystem::GetMakoContainerId();
	ReplayImport.BaseRevision = MakoAfter.Revision;
	FAIREInventoryImportOperation& ReplayOperation =
		ReplayImport.Operations.AddDefaulted_GetRef();
	ReplayOperation.OperationId = TEXT("operation.roundtrip");
	ReplayOperation.ItemId = FName(TEXT("AIRE.Test.Stack4"));
	ReplayOperation.Count = 1;
	TestEqual(
		TEXT("Import replay is already applied after restart"),
		Restored->TryApplyStartupImportCandidate(ReplayImport).Code,
		EAIREInventoryMutationCode::AlreadyApplied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREGameplayInventorySaveGameValidationFallbackTest,
	"AIRE.Inventory.SaveGame.ValidationAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIREGameplayInventorySaveGameValidationFallbackTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> Source = MakeInventory();
	FAIREGameplayInventoryPersistenceTestAccess::PrepareTransientSession(
		*Source);
	const FAIREInventoryMutationRequest AddRequest = MakeAddRequest(
		*Source,
		UAIREGameplayInventorySubsystem::GetMakoContainerId(),
		FName(TEXT("AIRE.Test.Stack4")),
		1);
	Source->TryAddItem(AddRequest);

	FAIREInventorySaveEnvelope ValidEnvelope;
	EAIREInventoryPersistenceResultCode ResultCode =
		EAIREInventoryPersistenceResultCode::NotStarted;
	TestTrue(
		TEXT("Validation fixture builds"),
		FAIREGameplayInventoryPersistenceTestAccess::BuildEnvelope(
			*Source,
			1,
			ValidEnvelope,
			ResultCode));

	FAIREInventorySaveEnvelope Normalized;
	FAIREInventorySaveEnvelope WrongFormat = ValidEnvelope;
	++WrongFormat.FormatVersion;
	TestFalse(
		TEXT("Unsupported format is rejected"),
		FAIREGameplayInventoryPersistenceTestAccess::ValidateEnvelope(
			*Source,
			WrongFormat,
			Normalized,
			ResultCode));
	TestEqual(
		TEXT("Unsupported format has an explicit result"),
		ResultCode,
		EAIREInventoryPersistenceResultCode::UnsupportedFormatVersion);
	FAIREInventoryContainerSnapshot BeforeRejectedCommit;
	Source->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetMakoContainerId(),
		BeforeRejectedCommit);
	TestFalse(
		TEXT("An invalid envelope cannot commit"),
		FAIREGameplayInventoryPersistenceTestAccess::CommitEnvelope(
			*Source,
			WrongFormat));
	FAIREInventoryContainerSnapshot AfterRejectedCommit;
	Source->GetContainerSnapshot(
		UAIREGameplayInventorySubsystem::GetMakoContainerId(),
		AfterRejectedCommit);
	TestEqual(
		TEXT("Rejected commit preserves revision"),
		AfterRejectedCommit.Revision,
		BeforeRejectedCommit.Revision);
	TestEqual(
		TEXT("Rejected commit preserves stacks"),
		AfterRejectedCommit.ItemStacks.Num(),
		BeforeRejectedCommit.ItemStacks.Num());

	FAIREInventorySaveEnvelope WrongScope = ValidEnvelope;
	WrongScope.ProfileId = TEXT("AIRE_OTHER");
	TestFalse(
		TEXT("Wrong scope is rejected"),
		FAIREGameplayInventoryPersistenceTestAccess::ValidateEnvelope(
			*Source,
			WrongScope,
			Normalized,
			ResultCode));
	TestEqual(
		TEXT("Wrong scope has an explicit result"),
		ResultCode,
		EAIREInventoryPersistenceResultCode::ScopeMismatch);

	auto ExpectRejected =
		[this, &Source, &Normalized, &ResultCode](
			const TCHAR* What,
			const FAIREInventorySaveEnvelope& Envelope,
			const EAIREInventoryPersistenceResultCode ExpectedCode)
		{
			const bool bAccepted =
				FAIREGameplayInventoryPersistenceTestAccess::ValidateEnvelope(
					*Source,
					Envelope,
					Normalized,
					ResultCode);
			TestFalse(What, bAccepted);
			TestEqual(What, ResultCode, ExpectedCode);
		};

	FAIREInventorySaveEnvelope WrongContent = ValidEnvelope;
	++WrongContent.ContentVersion;
	ExpectRejected(
		TEXT("Unsupported content version is rejected"),
		WrongContent,
		EAIREInventoryPersistenceResultCode::UnsupportedContentVersion);
	FAIREInventorySaveEnvelope InvalidGeneration = ValidEnvelope;
	InvalidGeneration.Generation = 0;
	ExpectRejected(
		TEXT("Invalid generation is rejected"),
		InvalidGeneration,
		EAIREInventoryPersistenceResultCode::InvalidGeneration);
	FAIREInventorySaveEnvelope InvalidSession = ValidEnvelope;
	InvalidSession.SourceSessionId.Invalidate();
	ExpectRejected(
		TEXT("Invalid source session is rejected"),
		InvalidSession,
		EAIREInventoryPersistenceResultCode::InvalidSession);

	auto FindMako = [](FAIREInventorySaveEnvelope& Envelope)
	{
		return Envelope.Containers.FindByPredicate(
			[](const FAIREInventoryPersistedContainer& Container)
			{
				return Container.ContainerId
					== UAIREGameplayInventorySubsystem::GetMakoContainerId();
			});
	};
	FAIREInventorySaveEnvelope WrongCapacity = ValidEnvelope;
	++FindMako(WrongCapacity)->Capacity;
	ExpectRejected(
		TEXT("Wrong container capacity is rejected"),
		WrongCapacity,
		EAIREInventoryPersistenceResultCode::InvalidContainer);
	FAIREInventorySaveEnvelope NegativeRevision = ValidEnvelope;
	FindMako(NegativeRevision)->Revision = -1;
	ExpectRejected(
		TEXT("Negative container revision is rejected"),
		NegativeRevision,
		EAIREInventoryPersistenceResultCode::InvalidContainer);
	FAIREInventorySaveEnvelope OutOfRangeSlot = ValidEnvelope;
	FindMako(OutOfRangeSlot)->ItemStacks[0].SlotIndex =
		AIREGameplayInventory::MakoItemSlotCapacity;
	ExpectRejected(
		TEXT("Out-of-range slot is rejected"),
		OutOfRangeSlot,
		EAIREInventoryPersistenceResultCode::InvalidPayload);
	FAIREInventorySaveEnvelope InvalidCount = ValidEnvelope;
	FindMako(InvalidCount)->ItemStacks[0].Count = 0;
	ExpectRejected(
		TEXT("Non-positive count is rejected"),
		InvalidCount,
		EAIREInventoryPersistenceResultCode::InvalidPayload);
	FAIREInventorySaveEnvelope OverMaxStack = ValidEnvelope;
	FindMako(OverMaxStack)->ItemStacks[0].Count = 5;
	ExpectRejected(
		TEXT("Count above item max-stack is rejected"),
		OverMaxStack,
		EAIREInventoryPersistenceResultCode::InvalidItem);
	FAIREInventorySaveEnvelope UnknownItem = ValidEnvelope;
	FindMako(UnknownItem)->ItemStacks[0].ItemId =
		FName(TEXT("AIRE.Test.Unknown"));
	ExpectRejected(
		TEXT("Unknown item is rejected"),
		UnknownItem,
		EAIREInventoryPersistenceResultCode::InvalidItem);

	FAIREInventorySaveEnvelope WrongPlayerCapacity = ValidEnvelope;
	WrongPlayerCapacity.Player.InventoryCapacity = 31;
	ExpectRejected(
		TEXT("Wrong player capacity is rejected"),
		WrongPlayerCapacity,
		EAIREInventoryPersistenceResultCode::InvalidPayload);
	FAIREInventorySaveEnvelope InvalidQuickSlot = ValidEnvelope;
	FAIREInventoryPersistedStack& InvalidQuickStack =
		InvalidQuickSlot.Player.ItemStacks.AddDefaulted_GetRef();
	InvalidQuickStack.SlotIndex = 110;
	InvalidQuickStack.ItemId = FName(TEXT("AIRE.Test.Stack2"));
	InvalidQuickStack.Count = 1;
	ExpectRejected(
		TEXT("Player quick slot outside 100-109 is rejected"),
		InvalidQuickSlot,
		EAIREInventoryPersistenceResultCode::InvalidPayload);
	FAIREInventorySaveEnvelope DuplicatePlayerSlot = ValidEnvelope;
	FAIREInventoryPersistedStack PlayerStack;
	PlayerStack.SlotIndex = 100;
	PlayerStack.ItemId = FName(TEXT("AIRE.Test.Stack2"));
	PlayerStack.Count = 1;
	DuplicatePlayerSlot.Player.ItemStacks.Add(PlayerStack);
	DuplicatePlayerSlot.Player.ItemStacks.Add(PlayerStack);
	ExpectRejected(
		TEXT("Duplicate player slot is rejected"),
		DuplicatePlayerSlot,
		EAIREInventoryPersistenceResultCode::InvalidPayload);
	FAIREInventorySaveEnvelope UnknownPlayerEquipment = ValidEnvelope;
	UnknownPlayerEquipment.Player.Equipment.EquippedItemId =
		FName(TEXT("AIRE.Test.Unknown"));
	ExpectRejected(
		TEXT("Unknown player equipment is rejected"),
		UnknownPlayerEquipment,
		EAIREInventoryPersistenceResultCode::InvalidItem);

	FAIREInventorySaveEnvelope DuplicateSlot = ValidEnvelope;
	FAIREInventoryPersistedContainer* Mako =
		DuplicateSlot.Containers.FindByPredicate(
			[](const FAIREInventoryPersistedContainer& Container)
			{
				return Container.ContainerId
					== UAIREGameplayInventorySubsystem::GetMakoContainerId();
			});
	check(Mako && !Mako->ItemStacks.IsEmpty());
	const FAIREInventoryPersistedStack DuplicateStack =
		Mako->ItemStacks[0];
	Mako->ItemStacks.Add(DuplicateStack);
	TestFalse(
		TEXT("Duplicate physical slot is rejected"),
		FAIREGameplayInventoryPersistenceTestAccess::ValidateEnvelope(
			*Source,
			DuplicateSlot,
			Normalized,
			ResultCode));
	TestEqual(
		TEXT("Duplicate slot is a payload failure"),
		ResultCode,
		EAIREInventoryPersistenceResultCode::InvalidPayload);
	FAIREInventorySaveEnvelope DuplicateLedger = ValidEnvelope;
	const FAIREInventoryPersistedMutationEntry DuplicateMutation =
		DuplicateLedger.Mutations[0];
	DuplicateLedger.Mutations.Add(DuplicateMutation);
	ExpectRejected(
		TEXT("Duplicate ledger ID is rejected"),
		DuplicateLedger,
		EAIREInventoryPersistenceResultCode::InvalidLedger);

	FAIREInventorySaveEnvelope OversizedLedger = ValidEnvelope;
	OversizedLedger.Mutations.Reset();
	for (int32 Index = 0;
		Index <= AIREGameplayInventoryPersistence::MaxLedgerEntries;
		++Index)
	{
		FAIREInventoryPersistedMutationEntry& Entry =
			OversizedLedger.Mutations.AddDefaulted_GetRef();
		Entry.MutationId = FGuid::NewGuid();
		Entry.Code = EAIREInventoryMutationCode::Succeeded;
		Entry.SourceRevision = 0;
	}
	TestFalse(
		TEXT("Oversized ledger is rejected"),
		FAIREGameplayInventoryPersistenceTestAccess::ValidateEnvelope(
			*Source,
			OversizedLedger,
			Normalized,
			ResultCode));
	TestEqual(
		TEXT("Oversized ledger has an explicit result"),
		ResultCode,
		EAIREInventoryPersistenceResultCode::InvalidLedger);

	TStrongObjectPtr<UAIREGameplayInventorySubsystem> EvictionTarget =
		MakeInventory();
	FAIREGameplayInventoryPersistenceTestAccess::PrepareTransientSession(
		*EvictionTarget);
	TArray<FGuid> MutationIds;
	for (int32 Index = 0;
		Index <= AIREGameplayInventoryPersistence::MaxLedgerEntries;
		++Index)
	{
		FAIREInventoryMutationResult Result;
		Result.MutationId = FGuid::NewGuid();
		Result.Code = EAIREInventoryMutationCode::Succeeded;
		Result.SourceRevision = 0;
		MutationIds.Add(Result.MutationId);
		FAIREGameplayInventoryPersistenceTestAccess::RecordMutation(
			*EvictionTarget,
			Result);
	}
	FAIREInventorySaveEnvelope EvictedEnvelope;
	TestTrue(
		TEXT("Bounded ledger still builds"),
		FAIREGameplayInventoryPersistenceTestAccess::BuildEnvelope(
			*EvictionTarget,
			1,
			EvictedEnvelope,
			ResultCode));
	TestEqual(
		TEXT("Mutation ledger is capped"),
		EvictedEnvelope.Mutations.Num(),
		AIREGameplayInventoryPersistence::MaxLedgerEntries);
	TestTrue(
		TEXT("FIFO evicts the oldest mutation"),
		!EvictedEnvelope.Mutations.ContainsByPredicate(
			[&MutationIds](const FAIREInventoryPersistedMutationEntry& Entry)
			{
				return Entry.MutationId == MutationIds[0];
			}));
	FAIREInventoryMutationRequest LedgerReplay;
	LedgerReplay.MutationId = MutationIds[1];
	TestEqual(
		TEXT("Ledger replay is already applied"),
		EvictionTarget->TryAddItem(LedgerReplay).Code,
		EAIREInventoryMutationCode::AlreadyApplied);
	FAIREInventoryMutationResult NewestResult;
	NewestResult.MutationId = FGuid::NewGuid();
	NewestResult.Code = EAIREInventoryMutationCode::Succeeded;
	NewestResult.SourceRevision = 0;
	FAIREGameplayInventoryPersistenceTestAccess::RecordMutation(
		*EvictionTarget,
		NewestResult);
	TestTrue(
		TEXT("Post-replay bounded ledger builds"),
		FAIREGameplayInventoryPersistenceTestAccess::BuildEnvelope(
			*EvictionTarget,
			2,
			EvictedEnvelope,
			ResultCode));
	TestTrue(
		TEXT("Replay lookup does not refresh FIFO order"),
		!EvictedEnvelope.Mutations.ContainsByPredicate(
			[&MutationIds](const FAIREInventoryPersistedMutationEntry& Entry)
			{
				return Entry.MutationId == MutationIds[1];
			}));
	TestTrue(
		TEXT("FIFO advances to the next oldest mutation"),
		EvictedEnvelope.Mutations[0].MutationId == MutationIds[2]);

	FAIREInventorySaveEnvelope CorruptLatest = ValidEnvelope;
	CorruptLatest.Generation = 2;
	++CorruptLatest.ContentVersion;
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> FallbackTarget =
		MakeInventory();
	FAIREGameplayInventoryPersistenceTestAccess::FinalizeLoadFromMemory(
		*FallbackTarget,
		TOptional<FAIREInventorySaveEnvelope>(CorruptLatest),
		TOptional<FAIREInventorySaveEnvelope>(ValidEnvelope));
	const FAIREInventoryPersistenceResult FallbackResult =
		FallbackTarget->GetLastPersistenceLoadResult();
	TestEqual(
		TEXT("Previous valid generation is used"),
		FallbackResult.Code,
		EAIREInventoryPersistenceResultCode::SucceededWithFallback);
	TestTrue(TEXT("Fallback is reported"), FallbackResult.bUsedFallback);
	TestEqual(TEXT("Fallback generation is preserved"), FallbackResult.Generation, 1LL);

	FAIREInventorySaveEnvelope HigherPrevious = ValidEnvelope;
	HigherPrevious.Generation = 2;
	TStrongObjectPtr<UAIREGameplayInventorySubsystem> HighestTarget =
		MakeInventory();
	FAIREGameplayInventoryPersistenceTestAccess::FinalizeLoadFromMemory(
		*HighestTarget,
		TOptional<FAIREInventorySaveEnvelope>(ValidEnvelope),
		TOptional<FAIREInventorySaveEnvelope>(HigherPrevious));
	TestEqual(
		TEXT("Highest valid generation wins independent of slot name"),
		HighestTarget->GetLastPersistenceLoadResult().Generation,
		2LL);
	TestEqual(
		TEXT("A valid higher Previous slot is not a fallback"),
		HighestTarget->GetLastPersistenceLoadResult().Code,
		EAIREInventoryPersistenceResultCode::Succeeded);

	TStrongObjectPtr<UAIREGameplayInventorySubsystem> SafeEmptyTarget =
		MakeInventory();
	FAIREInventorySaveEnvelope InvalidPrevious = ValidEnvelope;
	InvalidPrevious.SourceSessionId.Invalidate();
	FAIREGameplayInventoryPersistenceTestAccess::FinalizeLoadFromMemory(
		*SafeEmptyTarget,
		TOptional<FAIREInventorySaveEnvelope>(CorruptLatest),
		TOptional<FAIREInventorySaveEnvelope>(InvalidPrevious));
	TestEqual(
		TEXT("Two invalid generations produce safe empty"),
		SafeEmptyTarget->GetLastPersistenceLoadResult().Code,
		EAIREInventoryPersistenceResultCode::SafeEmptyNoValidSave);
	FAIREInventoryContainerSnapshot SafeEmptyMako;
	TestTrue(
		TEXT("Safe empty state is ready"),
		SafeEmptyTarget->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetMakoContainerId(),
			SafeEmptyMako));
	TestTrue(TEXT("Safe empty does not seed items"), SafeEmptyMako.ItemStacks.IsEmpty());

	FAIREGameplayInventoryPersistenceTestAccess::SetStableEquipment(
		*Source,
		FName(TEXT("AIRE.Test.WeaponA")));
	FAIREGameplayInventoryPersistenceTestAccess::SetEquipmentTransition(
		*Source,
		EAIREEquipmentTransitionState::Equipping);
	FAIREInventorySaveEnvelope DeferredEnvelope;
	TestFalse(
		TEXT("Equipping defers persistence"),
		FAIREGameplayInventoryPersistenceTestAccess::BuildEnvelope(
			*Source,
			2,
			DeferredEnvelope,
			ResultCode));
	TestEqual(
		TEXT("Equipment defer has an explicit result"),
		ResultCode,
		EAIREInventoryPersistenceResultCode::DeferredEquipmentTransition);
	FAIREGameplayInventoryPersistenceTestAccess::SetEquipmentTransition(
		*Source,
		EAIREEquipmentTransitionState::RecoveryFailed);
	TestTrue(
		TEXT("RecoveryFailed saves only stable equipment"),
		FAIREGameplayInventoryPersistenceTestAccess::BuildEnvelope(
			*Source,
			2,
			DeferredEnvelope,
			ResultCode));
	const FAIREInventoryPersistedContainer* StableMako =
		DeferredEnvelope.Containers.FindByPredicate(
			[](const FAIREInventoryPersistedContainer& Container)
			{
				return Container.ContainerId
					== UAIREGameplayInventorySubsystem::GetMakoContainerId();
			});
	TestTrue(
		TEXT("RecoveryFailed keeps only the last stable equipment"),
		StableMako
			&& StableMako->Equipment.EquippedItemId
				== FName(TEXT("AIRE.Test.WeaponA")));

	const FString PrimarySlot =
		AIREGameplayInventoryPersistence::PrimarySlotName;
	const FString PreviousSlot =
		AIREGameplayInventoryPersistence::PreviousSlotName;
	const uint64 FailedSaveEpoch =
		FAIREGameplayInventoryPersistenceTestAccess::PrimeSaveCompletion(
			*Source,
			1,
			PrimarySlot,
			2,
			PreviousSlot,
			false);
	FAIREGameplayInventoryPersistenceTestAccess::CompleteSave(
		*Source,
		PreviousSlot,
		FailedSaveEpoch,
		Source->GetInventorySessionId(),
		2,
		false);
	TestEqual(
		TEXT("Failed write preserves the last valid generation"),
		FAIREGameplayInventoryPersistenceTestAccess::GetLatestGeneration(
			*Source),
		1LL);
	TestEqual(
		TEXT("Failed write preserves the last valid slot"),
		FAIREGameplayInventoryPersistenceTestAccess::GetLatestSlotName(
			*Source),
		PrimarySlot);
	TestTrue(
		TEXT("Failed write remains dirty for a later retry"),
		FAIREGameplayInventoryPersistenceTestAccess::IsDirty(*Source));

	const uint64 CoalescedEpoch =
		FAIREGameplayInventoryPersistenceTestAccess::PrimeSaveCompletion(
			*Source,
			1,
			PrimarySlot,
			2,
			PreviousSlot,
			false);
	TestEqual(
		TEXT("A request during an active save is coalesced"),
		Source->RequestInventorySave().Code,
		EAIREInventoryPersistenceResultCode::Coalesced);
	FAIREGameplayInventoryPersistenceTestAccess::CompleteSave(
		*Source,
		PreviousSlot,
		CoalescedEpoch + 1,
		Source->GetInventorySessionId(),
		2,
		true);
	TestTrue(
		TEXT("A stale completion cannot finish the current save"),
		FAIREGameplayInventoryPersistenceTestAccess::IsSaveInFlight(
			*Source));
	return true;
}

#endif

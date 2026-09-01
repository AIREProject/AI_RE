#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryPersistenceTypes.h"
#include "AIREGameplayInventoryTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIREGameplayInventorySubsystem.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECompanionInventoryComponent;
class UAIRECompanionItemDefinitionDataAsset;
class UAI_REPlayerCombatComponent;
class UAI_REPlayerInventoryComponent;
class UAIREGameplayInventorySaveGame;
class USaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREInventoryContainerChanged,
	FName,
	ContainerId,
	int64,
	Revision);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FAIREInventoryPersistenceReady,
	const FAIREInventoryPersistenceResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FAIREInventoryPersistenceSaveCompleted,
	const FAIREInventoryPersistenceResult&);

/**
 * Player, MAKO, Shared Storage의 아이템 상태와 SaveGame을 소유하는 권위 Subsystem입니다.
 *
 * UI와 개별 Actor는 Snapshot만 소비하며 모든 변경은 SessionId, MutationId,
 * ExpectedRevision 검증을 통과한 뒤 Commit됩니다. 이동·제작·장착처럼 여러 상태를
 * 바꾸는 요청은 부분 성공을 남기지 않고, 같은 Mutation이나 Offline Task가 재전송되면
 * 기록된 결과를 반환해 멱등성을 유지합니다.
 *
 * 비동기 저장은 Generation과 Epoch로 오래된 Callback을 무시하고, Primary/Previous
 * 슬롯과 Mutation Ledger를 함께 보존해 손상 복구 후에도 중복 적용을 막습니다.
 */
UCLASS()
class AI_RE_API UAIREGameplayInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory|Persistence")
	bool IsPersistenceReady() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory|Persistence")
	FAIREInventoryPersistenceResult GetLastPersistenceLoadResult() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory|Persistence")
	FAIREInventoryPersistenceResult GetLastPersistenceSaveResult() const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory|Persistence")
	FAIREInventoryPersistenceResult RequestInventorySave();

	/** Deletes only local gameplay inventory progress. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory|Persistence")
	FAIREInventoryPersistenceResult DeleteGameplayProgress();

	FAIREInventoryPersistenceReady& OnPersistenceReady();
	FAIREInventoryPersistenceSaveCompleted& OnPersistenceSaveCompleted();

	bool RegisterPlayerInventory(
		UAI_REPlayerInventoryComponent* PlayerInventory,
		UAI_REPlayerCombatComponent* PlayerCombat);
	void UnregisterPlayerInventory(
		UAI_REPlayerInventoryComponent* PlayerInventory);
	void NotifyPlayerInventoryChanged(
		UAI_REPlayerInventoryComponent* PlayerInventory);

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory")
	FGuid GetInventorySessionId() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory")
	static FName GetMakoContainerId();

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory")
	static FName GetSharedStorageContainerId();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	bool GetContainerSnapshot(
		FName ContainerId,
		FAIREInventoryContainerSnapshot& OutSnapshot) const;

	/** Returns the validated Player state used by the current SaveGame generation. */
	bool GetPlayerPersistenceSnapshot(
		FAIREInventoryPersistedPlayerState& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	FAIREInventoryMutationResult TryAddItem(
		const FAIREInventoryMutationRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	FAIREInventoryMutationResult TryRemoveItem(
		const FAIREInventoryMutationRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	FAIREInventoryMutationResult TryMoveItem(
		const FAIREInventoryMoveRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	FAIREInventoryMutationResult TryTransferItem(
		const FAIREInventoryTransferRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	FAIREInventoryMutationResult TryTransferPlayerStorage(
		UAI_REPlayerInventoryComponent* PlayerInventory,
		const FAIREPlayerStorageTransferRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	FAIREInventoryMutationResult TryTransferPlayerMako(
		UAI_REPlayerInventoryComponent* PlayerInventory,
		const FAIREPlayerMakoTransferRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	FAIREInventoryMutationResult TryEquipPlayerWeapon(
		UAI_REPlayerInventoryComponent* PlayerInventory,
		UAI_REPlayerCombatComponent* PlayerCombat,
		const FAIREPlayerWeaponEquipRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	FAIREInventoryMutationResult TryUnequipPlayerWeapon(
		UAI_REPlayerInventoryComponent* PlayerInventory,
		UAI_REPlayerCombatComponent* PlayerCombat,
		const FAIREPlayerWeaponUnequipRequest& Request);

	/** Validates Player + SharedStorage craft settlement without mutation. */
	bool CanCompletePlayerCraft(
		const UAI_REPlayerInventoryComponent* PlayerInventory,
		const FAIREPlayerCraftRequest& Request,
		FAIREInventoryMutationResult& OutResult) const;

	FAIREInventoryMutationResult TryCompletePlayerCraft(
		UAI_REPlayerInventoryComponent* PlayerInventory,
		const FAIREPlayerCraftRequest& Request);

	/** Validates a craft completion without changing containers or mutation ledgers. */
	bool CanCompleteMakoCraftWork(
		const FAIREMakoCraftWorkRequest& Request,
		FAIREInventoryWorkResult& OutResult) const;

	FAIREInventoryWorkResult TryCompleteMakoCraftWork(
		const FAIREMakoCraftWorkRequest& Request);

	/**
	 * Stores a reward in MAKO or the shared storage. WorldDrop is a routing
	 * decision only; the harvest actor owns spawn success and duplicate suppression.
	 */
	FAIREInventoryWorkResult TryStoreMakoWorkReward(
		const FAIREMakoWorkRewardRequest& Request);

	FAIREOfflineTaskApplyResult TryApplyOfflineTaskResult(
		const FAIREOfflineTaskApplyRequest& Request);

	FGuid ResetInventorySession(
		const FAIREInventorySessionScope& NewScope =
			FAIREInventorySessionScope());

	bool EnsureMakoInventoryInitialized(
		const UAIRECompanionConfigDataAsset* CompanionConfig);
	const UAIRECompanionItemDefinitionDataAsset*
		FindCompanionItemDefinition(FName ItemId) const;

	FAIREInventoryMutationResult TryApplyStartupImportCandidate(
		const FAIREInventoryStartupImportCandidate& Candidate);

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Inventory")
	FAIREInventoryContainerChanged OnContainerChanged;

private:
	friend class UAIRECompanionInventoryComponent;
	friend class FAIREGameplayInventorySubsystemContractTest;
	friend class FAIREGameplayInventoryMakoCraftWorkTest;
	friend class FAIREGameplayInventoryPersistenceTestAccess;

	struct FAIREContainerState
	{
		FName ContainerId;
		int32 Capacity = 0;
		int64 Revision = 0;
		TArray<FAIREInventoryItemStackSnapshot> ItemStacks;
		FName EquippedItemId;
		FName PendingItemId;
		FName PreviousItemId;
		EAIREEquipmentTransitionState EquipmentTransition =
			EAIREEquipmentTransitionState::Idle;
		FGuid EquipmentMutationId;
		int32 ReservedSlotIndex = INDEX_NONE;
	};

	struct FAIREItemRules
	{
		int32 MaxStackSize = 0;
		bool bIsCompanionItem = false;
		bool bIsWeapon = false;
	};

	struct FAIREPersistenceLoadSlotState
	{
		FString SlotName;
		bool bCompleted = false;
		bool bExists = false;
		bool bIoFailure = false;
		TOptional<FAIREInventorySaveEnvelope> LoadedEnvelope;
		EAIREInventoryPersistenceResultCode ValidationCode =
			EAIREInventoryPersistenceResultCode::NotStarted;
	};

	void CreateEmptyContainers();
	FAIREContainerState* FindContainer(FName ContainerId);
	const FAIREContainerState* FindContainer(FName ContainerId) const;
	bool ResolveItemRules(
		FName ItemId,
		bool bRequireCompanionItem,
		FAIREItemRules& OutRules) const;
	bool IsEquipmentTransitionActive(const FAIREContainerState& State) const;
	bool IsSessionScopeValid(const FAIREInventorySessionScope& Scope) const;
	static FAIREInventorySessionScope MakeCanonicalPersistenceScope();

	FAIREInventoryMutationResult ValidateMutation(
		const FGuid& RequestSessionId,
		const FGuid& MutationId,
		const FAIREContainerState* Container,
		int64 ExpectedRevision) const;
	FAIREInventoryMutationResult MakeResult(
		EAIREInventoryMutationCode Code,
		const FGuid& MutationId,
		int64 SourceRevision = INDEX_NONE,
		int64 DestinationRevision = INDEX_NONE) const;
	bool FindAppliedMutation(
		const FGuid& MutationId,
		FAIREInventoryMutationResult& OutResult) const;
	void RecordAppliedMutation(
		const FAIREInventoryMutationResult& Result,
		bool bPersistent);
	void RemoveAppliedMutation(const FGuid& MutationId);
	void RecordAppliedWorkResult(
		const FGuid& OperationId,
		const FAIREInventoryWorkResult& Result,
		bool bPersistent = true);
	void RecordAppliedImportCandidateId(const FString& CandidateId);
	void RecordAppliedImportOperationIds(const TArray<FString>& OperationIds);
	void RecordAppliedOfflineTaskId(const FString& TaskId);
	void BroadcastContainerChanged(const FAIREContainerState& Container);
	bool AggregateWorkIngredients(
		const TArray<FAIREInventoryItemQuantity>& Ingredients,
		TMap<FName, int32>& OutIngredients) const;
	FAIREInventoryWorkResult MakeWorkResult(
		EAIREInventoryMutationCode Code,
		EAIREInventoryWorkResultDestination Destination,
		const FAIREInventoryItemQuantity& DeliveredItem) const;
	bool FindAppliedWorkResult(
		const FGuid& MutationId,
		FAIREInventoryWorkResult& OutResult) const;

	void BeginPersistenceLoad();
	void BeginLoadForSlot(int32 SlotIndex, uint64 LoadEpoch);
	void HandleSlotExistenceResult(
		int32 SlotIndex,
		uint64 LoadEpoch,
		bool bExists,
		bool bIoFailure);
	void HandleSlotLoaded(
		int32 SlotIndex,
		uint64 LoadEpoch,
		USaveGame* LoadedGame);
	void FinalizePersistenceLoad(uint64 LoadEpoch);
	bool ValidatePersistenceEnvelope(
		const FAIREInventorySaveEnvelope& Envelope,
		FAIREInventorySaveEnvelope& OutNormalizedEnvelope,
		EAIREInventoryPersistenceResultCode& OutCode) const;
	bool CommitPersistenceEnvelope(
		const FAIREInventorySaveEnvelope& Envelope);
	void FinalizeFreshPersistenceStateIfPossible();
	void CompletePersistenceStartup(
		const FAIREInventoryPersistenceResult& Result);
	bool BuildPersistenceEnvelope(
		int64 Generation,
		FAIREInventorySaveEnvelope& OutEnvelope,
		EAIREInventoryPersistenceResultCode& OutCode) const;
	bool CapturePlayerPersistenceState(
		const UAI_REPlayerInventoryComponent& PlayerInventory,
		FAIREInventoryPersistedPlayerState& OutState,
		EAIREInventoryPersistenceResultCode& OutCode) const;
	bool ValidatePlayerPersistenceState(
		const FAIREInventoryPersistedPlayerState& State,
		FAIREInventoryPersistedPlayerState& OutNormalizedState,
		EAIREInventoryPersistenceResultCode& OutCode) const;
	void ApplyOrInitializeRegisteredPlayerState();
	void MarkPersistenceDirty();
	FAIREInventoryPersistenceResult TryStartPersistenceSave();
	void HandlePersistenceSaveCompleted(
		const FString& SlotName,
		uint64 SaveEpoch,
		const FGuid& SaveSessionId,
		int64 Generation,
		bool bSucceeded);
	FString GetNextPersistenceSlotName() const;
	FAIREInventoryPersistenceResult MakePersistenceResult(
		EAIREInventoryPersistenceOperation Operation,
		EAIREInventoryPersistenceResultCode Code,
		int64 Generation = 0,
		bool bUsedFallback = false) const;

	FAIREInventoryMutationResult ReserveMakoEquipmentSwap(
		const FAIREInventoryEquipRequest& Request);
	FAIREInventoryMutationResult CommitMakoEquipmentSwap(
		const FGuid& RequestSessionId,
		const FGuid& MutationId);
	FAIREInventoryMutationResult BeginMakoEquipmentRecovery(
		const FGuid& RequestSessionId,
		const FGuid& MutationId);
	FAIREInventoryMutationResult CompleteMakoEquipmentRecovery(
		const FGuid& RequestSessionId,
		const FGuid& MutationId,
		bool bSucceeded);
	FAIREInventoryMutationResult CompleteMakoEquipmentRuntimeRestore(
		const FGuid& RequestSessionId,
		FName ItemId,
		bool bSucceeded);
	FAIREInventoryMutationResult CancelMakoEquipmentSwap(
		const FGuid& RequestSessionId,
		const FGuid& MutationId);

	FGuid InventorySessionId;
	FAIREInventorySessionScope SessionScope;
	TMap<FName, FAIREContainerState> Containers;
	TMap<FGuid, FAIREInventoryMutationResult> AppliedMutations;
	TArray<FGuid> AppliedMutationOrder;
	TMap<FGuid, FAIREInventoryMutationResult> TransientAppliedMutations;
	TArray<FGuid> TransientAppliedMutationOrder;
	TMap<FGuid, FAIREInventoryWorkResult> AppliedWorkResults;
	TArray<FGuid> AppliedWorkResultOrder;
	TMap<FGuid, FAIREInventoryWorkResult> TransientAppliedWorkResults;
	TArray<FGuid> TransientAppliedWorkResultOrder;
	TSet<FString> AppliedImportCandidateIds;
	TArray<FString> AppliedImportCandidateOrder;
	TSet<FString> AppliedImportOperationIds;
	TArray<FString> AppliedImportOperationOrder;
	TSet<FString> AppliedOfflineTaskIds;
	TArray<FString> AppliedOfflineTaskOrder;
	TArray<FAIREPersistenceLoadSlotState> PersistenceLoadSlots;
	TWeakObjectPtr<const UAIRECompanionConfigDataAsset> PendingCompanionConfig;
	TWeakObjectPtr<UAI_REPlayerInventoryComponent> RegisteredPlayerInventory;
	TWeakObjectPtr<UAI_REPlayerCombatComponent> RegisteredPlayerCombat;
	FAIREInventoryPersistedPlayerState CachedPlayerPersistenceState;
	FAIREInventoryPersistenceReady PersistenceReadyDelegate;
	FAIREInventoryPersistenceSaveCompleted PersistenceSaveCompletedDelegate;
	FAIREInventoryPersistenceResult LastPersistenceLoadResult;
	FAIREInventoryPersistenceResult LastPersistenceSaveResult;
	uint64 PersistenceEpoch = 0;
	uint64 ActiveSaveEpoch = 0;
	uint64 DeletedThroughSaveEpoch = 0;
	int64 LatestPersistenceGeneration = 0;
	int64 HighestIssuedPersistenceGeneration = 0;
	FString LatestPersistenceSlotName;
	FString LastIssuedPersistenceSlotName;
	bool bPersistenceLoadComplete = false;
	bool bPersistenceReady = false;
	bool bPersistenceDirty = false;
	bool bPersistenceSaveInFlight = false;
	bool bSuppressPersistenceDirty = false;
	bool bPersistenceShuttingDown = false;
	bool bPersistenceLifecycleInitialized = false;
	bool bHasPlayerPersistenceState = false;
	bool bApplyingPlayerPersistenceState = false;
	bool bMakoInventoryInitialized = false;
	bool bShouldSeedFreshSharedStorage = false;
};

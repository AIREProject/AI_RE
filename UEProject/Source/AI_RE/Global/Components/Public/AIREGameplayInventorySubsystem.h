#pragma once

#include "CoreMinimal.h"
#include "AIREGameplayInventoryTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIREGameplayInventorySubsystem.generated.h"

class UAIRECompanionConfigDataAsset;
class UAIRECompanionInventoryComponent;
class UAIRECompanionItemDefinitionDataAsset;
class UAI_REPlayerInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREInventoryContainerChanged,
	FName,
	ContainerId,
	int64,
	Revision);

UCLASS()
class AI_RE_API UAIREGameplayInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory")
	FGuid GetInventorySessionId() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory")
	static FName GetMakoContainerId();

	UFUNCTION(BlueprintPure, Category = "AIRE|Inventory")
	static FName GetSharedWarehouseContainerId();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Inventory")
	bool GetContainerSnapshot(
		FName ContainerId,
		FAIREInventoryContainerSnapshot& OutSnapshot) const;

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
	FAIREInventoryMutationResult TryTransferPlayerWarehouse(
		UAI_REPlayerInventoryComponent* PlayerInventory,
		const FAIREPlayerWarehouseTransferRequest& Request);

	/** Validates a craft completion without changing containers or mutation ledgers. */
	bool CanCompleteMakoCraftWork(
		const FAIREMakoCraftWorkRequest& Request,
		FAIREInventoryWorkResult& OutResult) const;

	FAIREInventoryWorkResult TryCompleteMakoCraftWork(
		const FAIREMakoCraftWorkRequest& Request);

	/**
	 * Stores a reward in MAKO or the shared warehouse. WorldDrop is a routing
	 * decision only; the harvest actor owns spawn success and duplicate suppression.
	 */
	FAIREInventoryWorkResult TryStoreMakoWorkReward(
		const FAIREMakoWorkRewardRequest& Request);

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

	void CreateEmptyContainers();
	FAIREContainerState* FindContainer(FName ContainerId);
	const FAIREContainerState* FindContainer(FName ContainerId) const;
	bool ResolveItemRules(
		FName ItemId,
		bool bRequireCompanionItem,
		FAIREItemRules& OutRules) const;
	bool IsEquipmentTransitionActive(const FAIREContainerState& State) const;
	bool IsSessionScopeValid(const FAIREInventorySessionScope& Scope) const;

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
	void RecordAppliedMutation(const FAIREInventoryMutationResult& Result);
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
	TMap<FGuid, FAIREInventoryWorkResult> AppliedWorkResults;
	TSet<FString> AppliedImportCandidateIds;
	TSet<FString> AppliedImportOperationIds;
	bool bMakoInventoryInitialized = false;
};

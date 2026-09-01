#include "Chat/Context/AIREWorldContextBuilder.h"

#include "AIREGameplayInventorySubsystem.h"
#include "AIREGameplayInventoryTypes.h"
#include "AI_REHarvestGameplayTags.h"
#include "Core/AIRECompanionAIController.h"
#include "Core/AIRECompanionCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Threat/AIRECompanionThreatComponent.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "Work/AIRECompanionHarvestableResourceQuery.h"
#include "Work/AIRECompanionWorkbenchQuery.h"
#include "Work/AIRECompanionWorkOrderTypes.h"

namespace
{
	const FName IncludedInventoryItemIds[] =
	{
		TEXT("IronIngot"),
		TEXT("Sword_Iron"),
		TEXT("PlantStem"),
		TEXT("ShoddyBandage"),
		TEXT("Stone"),
		TEXT("WoodHandle"),
	};

	bool IsWorldContextStableId(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > AIREWorldContext::MaxStableIdLength)
		{
			return false;
		}

		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			const bool bIsAsciiAlphanumeric =
				(Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9'));
			const bool bIsAllowed = bIsAsciiAlphanumeric
				|| (Index > 0
					&& (Character == TEXT('.')
						|| Character == TEXT('_')
						|| Character == TEXT(':')
						|| Character == TEXT('-')));
			if (!bIsAllowed)
			{
				return false;
			}
		}
		return true;
	}

	bool IsIncludedInventoryItem(const FName ItemId)
	{
		for (const FName IncludedItemId : IncludedInventoryItemIds)
		{
			if (ItemId == IncludedItemId)
			{
				return true;
			}
		}
		return false;
	}

	bool TryBuildInventory(
		const FAIREInventoryContainerSnapshot& Snapshot,
		const FName ExpectedContainerId,
		const int32 ExpectedCapacity,
		FAIREWorldContextInventory& OutInventory)
	{
		if (!Snapshot.SessionId.IsValid()
			|| Snapshot.ContainerId != ExpectedContainerId
			|| Snapshot.Capacity != ExpectedCapacity
			|| Snapshot.ItemStacks.Num() > ExpectedCapacity)
		{
			return false;
		}

		TMap<FName, int32> Totals;
		TSet<int32> OccupiedSlots;
		int32 TotalItemCount = 0;
		bool bTruncated = false;
		for (const FAIREInventoryItemStackSnapshot& Stack : Snapshot.ItemStacks)
		{
			const FString ItemId = Stack.ItemId.ToString();
			if (Stack.SlotIndex < 0
				|| Stack.SlotIndex >= ExpectedCapacity
				|| OccupiedSlots.Contains(Stack.SlotIndex)
				|| !IsWorldContextStableId(ItemId)
				|| Stack.Count < 1)
			{
				return false;
			}
			OccupiedSlots.Add(Stack.SlotIndex);
			if (Stack.Count > ExpectedCapacity * 99 - TotalItemCount)
			{
				return false;
			}
			TotalItemCount += Stack.Count;

			if (IsIncludedInventoryItem(Stack.ItemId))
			{
				Totals.FindOrAdd(Stack.ItemId) += Stack.Count;
			}
			else
			{
				bTruncated = true;
			}
		}

		OutInventory = FAIREWorldContextInventory();
		OutInventory.ContainerId = ExpectedContainerId.ToString();
		OutInventory.FreeSlots = ExpectedCapacity - OccupiedSlots.Num();
		OutInventory.bTruncated = bTruncated;
		for (const FName IncludedItemId : IncludedInventoryItemIds)
		{
			const int32* Count = Totals.Find(IncludedItemId);
			if (Count && *Count > 0)
			{
				FAIREWorldContextInventoryItemTotal& Item =
					OutInventory.ItemTotals.AddDefaulted_GetRef();
				Item.ItemId = IncludedItemId.ToString();
				Item.Count = *Count;
			}
		}
		return true;
	}

	EAIREWorldContextWorkType ToContextWorkType(
		const EAIRECompanionWorkOrderType Type)
	{
		switch (Type)
		{
		case EAIRECompanionWorkOrderType::Crafting:
			return EAIREWorldContextWorkType::Crafting;
		case EAIRECompanionWorkOrderType::Harvesting:
			return EAIREWorldContextWorkType::Harvesting;
		case EAIRECompanionWorkOrderType::StorageTransfer:
			return EAIREWorldContextWorkType::StorageTransfer;
		case EAIRECompanionWorkOrderType::None:
		default:
			return EAIREWorldContextWorkType::None;
		}
	}

	EAIREWorldContextWorkState ToContextWorkState(
		const EAIRECompanionWorkOrderState State)
	{
		switch (State)
		{
		case EAIRECompanionWorkOrderState::Requested:
			return EAIREWorldContextWorkState::Requested;
		case EAIRECompanionWorkOrderState::Moving:
			return EAIREWorldContextWorkState::Moving;
		case EAIRECompanionWorkOrderState::Working:
			return EAIREWorldContextWorkState::Working;
		case EAIRECompanionWorkOrderState::PausedByCombat:
			return EAIREWorldContextWorkState::PausedByCombat;
		case EAIRECompanionWorkOrderState::None:
		case EAIRECompanionWorkOrderState::Completed:
		case EAIRECompanionWorkOrderState::Cancelled:
		case EAIRECompanionWorkOrderState::Failed:
		default:
			return EAIREWorldContextWorkState::None;
		}
	}
}

FAIREWorldContextV1 FAIREWorldContextBuilder::Build(
	const AAIRECompanionCharacter* Companion,
	const FString& LocationId)
{
	FAIREWorldContextV1 Context;
	Context.LocationId = IsWorldContextStableId(LocationId)
		? LocationId
		: FString();
	if (!IsValid(Companion)
		|| Companion->IsActorBeingDestroyed()
		|| !IsValid(Companion->GetWorld()))
	{
		return Context;
	}

	const AAIRECompanionAIController* Controller =
		Cast<AAIRECompanionAIController>(Companion->GetController());
	const UAIRECompanionThreatComponent* ThreatComponent =
		IsValid(Controller) && !Controller->IsActorBeingDestroyed()
			? Controller->GetThreatComponent()
			: nullptr;
	if (IsValid(ThreatComponent))
	{
		Context.Threat.Count = FMath::Clamp(
			ThreatComponent->GetPerceivedHostileCount(),
			0,
			AIREWorldContext::MaxThreatCount);
		Context.Threat.bPresent = Context.Threat.Count > 0;
	}

	const UAIRECompanionWorkOrderComponent* WorkOrderComponent =
		Companion->GetWorkOrderComponent();
	if (IsValid(WorkOrderComponent) && WorkOrderComponent->HasActiveWorkOrder())
	{
		const FAIRECompanionWorkOrderSnapshot Snapshot =
			WorkOrderComponent->GetWorkOrderSnapshot();
		const AActor* TargetActor = Snapshot.TargetActor.Get();
		if (IsValid(TargetActor) && !TargetActor->IsActorBeingDestroyed())
		{
			Context.CurrentWork.Type = ToContextWorkType(Snapshot.WorkType);
			Context.CurrentWork.State = ToContextWorkState(Snapshot.State);
			if (!Context.CurrentWork.IsSet())
			{
				Context.CurrentWork = FAIREWorldContextCurrentWork();
			}
		}
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	FAIREInventoryContainerSnapshot StorageSnapshot;
	const UAIRECompanionInventoryComponent* InventoryComponent =
		Companion->GetInventoryComponent();
	const bool bHasMakoSnapshot = IsValid(InventoryComponent)
		&& InventoryComponent->GetInventorySnapshot(MakoSnapshot);

	UGameInstance* GameInstance = Companion->GetGameInstance();
	const UAIREGameplayInventorySubsystem* GameplayInventory =
		IsValid(GameInstance)
			? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
			: nullptr;
	const bool bHasStorageSnapshot = IsValid(GameplayInventory)
		&& GameplayInventory->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			StorageSnapshot);
	if (bHasMakoSnapshot
		&& bHasStorageSnapshot
		&& MakoSnapshot.SessionId != StorageSnapshot.SessionId)
	{
		return Context;
	}

	FAIREWorldContextInventory Inventory;
	if (bHasMakoSnapshot
		&& TryBuildInventory(
			MakoSnapshot,
			UAIREGameplayInventorySubsystem::GetMakoContainerId(),
			AIREGameplayInventory::MakoItemSlotCapacity,
			Inventory))
	{
		Context.Inventories.Add(MoveTemp(Inventory));
	}
	if (bHasStorageSnapshot
		&& TryBuildInventory(
			StorageSnapshot,
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			AIREGameplayInventory::SharedStorageSlotCapacity,
			Inventory))
	{
		Context.Inventories.Add(MoveTemp(Inventory));
	}

	FAIRECompanionWorkbenchQuery::GetNearbyCapabilityIds(
		*Companion,
		Context.AvailableWorkstations);
	struct FNearbyResourceDefinition
	{
		const TCHAR* Kind;
		FGameplayTag Tag;
	};
	const FNearbyResourceDefinition ResourceDefinitions[] =
	{
		{TEXT("wood"), AI_REHarvestGameplayTags::Resource_Wood},
		{TEXT("stone"), AI_REHarvestGameplayTags::Resource_Rock},
		{TEXT("iron_ore"), AI_REHarvestGameplayTags::Resource_IronOre},
	};
	for (const FNearbyResourceDefinition& Definition : ResourceDefinitions)
	{
		int32 NearbyCount = 0;
		if (FAIRECompanionHarvestableResourceQuery::GetNearbyResourceCount(
				*Companion,
				Definition.Tag,
				NearbyCount)
			&& NearbyCount > 0)
		{
			FAIREWorldContextNearbyResource& Resource =
				Context.NearbyResources.AddDefaulted_GetRef();
			Resource.Kind = Definition.Kind;
			Resource.Count = NearbyCount;
		}
	}
	return Context;
}

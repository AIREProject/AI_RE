#include "Testing/AIRECompanionTestingBlueprintLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "Core/AIRECompanionCharacter.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionDamageGameplayEffect.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AIREGameplayInventorySubsystem.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionTesting, Log, All);

namespace
{
	AAIRECompanionAIController* FindFirstCompanionController(const UObject* WorldContextObject)
	{
		if (!IsValid(GEngine) || !IsValid(WorldContextObject))
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (!IsValid(World))
		{
			return nullptr;
		}

		for (TActorIterator<AAIRECompanionAIController> Iterator(World); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator))
			{
				return *Iterator;
			}
		}

		return nullptr;
	}

	AAIRECompanionCharacter* FindFirstCompanionCharacter(const UObject* WorldContextObject)
	{
		if (!IsValid(GEngine) || !IsValid(WorldContextObject))
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (!IsValid(World))
		{
			return nullptr;
		}

		for (TActorIterator<AAIRECompanionCharacter> Iterator(World); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator))
			{
				return *Iterator;
			}
		}

		return nullptr;
	}

	UAIREGameplayInventorySubsystem* FindGameplayInventorySubsystem(
		const UObject* WorldContextObject)
	{
		if (!IsValid(GEngine) || !IsValid(WorldContextObject))
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		return IsValid(GameInstance)
			? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
			: nullptr;
	}

	UAI_REPlayerInventoryComponent* FindFirstPlayerInventory(const UObject* WorldContextObject)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, 0);
		return IsValid(PlayerPawn)
			? PlayerPawn->FindComponentByClass<UAI_REPlayerInventoryComponent>()
			: nullptr;
	}

	bool FindFirstMatchingStack(
		const FAIREInventoryContainerSnapshot& Snapshot,
		const FName ItemId,
		FAIREInventoryItemStackSnapshot& OutStack)
	{
		for (const FAIREInventoryItemStackSnapshot& Stack : Snapshot.ItemStacks)
		{
			if (Stack.ItemId == ItemId && Stack.Count > 0)
			{
				OutStack = Stack;
				return true;
			}
		}

		return false;
	}

	void LogInventorySnapshot(const TCHAR* Label, const FAIREInventoryContainerSnapshot& Snapshot)
	{
		UE_LOG(
			LogAIRECompanionTesting,
			Log,
			TEXT("%s Inventory. Container=%s Session=%s Revision=%lld Capacity=%d Equipped=%s Pending=%s Transition=%d Stacks=%d"),
			Label,
			*Snapshot.ContainerId.ToString(),
			*Snapshot.SessionId.ToString(),
			Snapshot.Revision,
			Snapshot.Capacity,
			*Snapshot.Equipment.EquippedItemId.ToString(),
			*Snapshot.Equipment.PendingItemId.ToString(),
			static_cast<int32>(Snapshot.Equipment.TransitionState),
			Snapshot.ItemStacks.Num());

		for (const FAIREInventoryItemStackSnapshot& Stack : Snapshot.ItemStacks)
		{
			UE_LOG(
				LogAIRECompanionTesting,
				Log,
				TEXT("%s Inventory Stack. Slot=%d Item=%s Count=%d"),
				Label,
				Stack.SlotIndex,
				*Stack.ItemId.ToString(),
				Stack.Count);
		}
	}
}

bool UAIRECompanionTestingBlueprintLibrary::SetFirstCompanionTestBehaviorRequest(
	const UObject* WorldContextObject,
	const EAIRECompanionTestBehaviorRequest Request,
	const bool bIsRequested)
{
	AAIRECompanionAIController* CompanionController = FindFirstCompanionController(WorldContextObject);
	if (!IsValid(CompanionController))
	{
		return false;
	}

	CompanionController->SetTestBehaviorRequest(Request, bIsRequested);
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::ClearFirstCompanionTestBehaviorRequests(
	const UObject* WorldContextObject)
{
	AAIRECompanionAIController* CompanionController = FindFirstCompanionController(WorldContextObject);
	if (!IsValid(CompanionController))
	{
		return false;
	}

	CompanionController->ClearTestBehaviorRequests();
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::ApplyDamageToFirstCompanion(
	const UObject* WorldContextObject,
	const float DamageAmount)
{
	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return false;
	}

	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAbilitySystemComponent* AbilitySystem = IsValid(CompanionCharacter)
		? CompanionCharacter->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(AbilitySystem))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(CompanionCharacter);
	FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
		UAIRECompanionDamageGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!EffectSpec.IsValid())
	{
		return false;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(AIRECompanionGameplayTags::DataDamage, -DamageAmount);
	const FActiveGameplayEffectHandle AppliedEffect = AbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	return AppliedEffect.WasSuccessfullyApplied();
}

bool UAIRECompanionTestingBlueprintLibrary::ResetFirstCompanionAttributes(const UObject* WorldContextObject)
{
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	return IsValid(CompanionCharacter) && CompanionCharacter->ResetAttributesToConfiguredDefaults();
}

bool UAIRECompanionTestingBlueprintLibrary::LogFirstCompanionAbilityState(const UObject* WorldContextObject)
{
	const AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	if (!IsValid(CompanionCharacter))
	{
		return false;
	}

	const UAIRECompanionAttributeSet* Attributes = CompanionCharacter->GetCompanionAttributeSet();
	const UAbilitySystemComponent* AbilitySystem = CompanionCharacter->GetAbilitySystemComponent();
	if (!IsValid(Attributes) || !IsValid(AbilitySystem))
	{
		return false;
	}

	UE_LOG(
		LogAIRECompanionTesting,
		Log,
		TEXT("Companion GAS state. Companion=%s Health=%.2f/%.2f Stamina=%.2f/%.2f Dead=%s Disabled=%s"),
		*GetNameSafe(CompanionCharacter),
		Attributes->GetHealth(),
		Attributes->GetMaxHealth(),
		Attributes->GetStamina(),
		Attributes->GetMaxStamina(),
		AbilitySystem->HasMatchingGameplayTag(AIRECompanionGameplayTags::StateDisabledDead) ? TEXT("true") : TEXT("false"),
		AbilitySystem->HasMatchingGameplayTag(AIRECompanionGameplayTags::StateDisabled) ? TEXT("true") : TEXT("false"));
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::SeedFirstCompanionInventoryItem(
	const UObject* WorldContextObject,
	const FName ItemId,
	const int32 Count)
{
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAIRECompanionInventoryComponent* Inventory = IsValid(CompanionCharacter)
		? CompanionCharacter->GetInventoryComponent()
		: nullptr;
	return IsValid(Inventory) && Inventory->TryAddItem(ItemId, Count);
}

bool UAIRECompanionTestingBlueprintLibrary::SeedFirstPlayerInventoryItem(
	const UObject* WorldContextObject,
	const FName ItemId,
	const int32 Count)
{
	UAI_REPlayerInventoryComponent* Inventory = FindFirstPlayerInventory(WorldContextObject);
	if (!IsValid(Inventory))
	{
		return false;
	}

	TArray<FInventoryItemStack> NewItems;
	if (!Inventory->BuildExactAddState(ItemId, Count, NewItems))
	{
		return false;
	}
	Inventory->CommitExactInventoryState(MoveTemp(NewItems));
	Inventory->NotifyExactInventoryMutation();
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::TransferFirstInventoryItem(
	const UObject* WorldContextObject,
	const EAIRECompanionTestingInventoryTransferDirection Direction,
	const FName ItemId,
	const int32 Count)
{
	UAIREGameplayInventorySubsystem* InventorySubsystem = FindGameplayInventorySubsystem(WorldContextObject);
	if (!IsValid(InventorySubsystem) || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	FAIREInventoryContainerSnapshot WarehouseSnapshot;
	if (!InventorySubsystem->GetContainerSnapshot(UAIREGameplayInventorySubsystem::GetMakoContainerId(), MakoSnapshot)
		|| !InventorySubsystem->GetContainerSnapshot(UAIREGameplayInventorySubsystem::GetSharedWarehouseContainerId(), WarehouseSnapshot))
	{
		return false;
	}

	if (Direction == EAIRECompanionTestingInventoryTransferDirection::MakoToWarehouse
		|| Direction == EAIRECompanionTestingInventoryTransferDirection::WarehouseToMako)
	{
		const FAIREInventoryContainerSnapshot& SourceSnapshot =
			Direction == EAIRECompanionTestingInventoryTransferDirection::MakoToWarehouse
			? MakoSnapshot
			: WarehouseSnapshot;
		FAIREInventoryItemStackSnapshot SourceStack;
		if (!FindFirstMatchingStack(SourceSnapshot, ItemId, SourceStack))
		{
			return false;
		}

		FAIREInventoryTransferRequest Request;
		Request.SessionId = InventorySubsystem->GetInventorySessionId();
		Request.MutationId = FGuid::NewGuid();
		Request.SourceContainerId = SourceSnapshot.ContainerId;
		Request.DestinationContainerId =
			Direction == EAIRECompanionTestingInventoryTransferDirection::MakoToWarehouse
			? WarehouseSnapshot.ContainerId
			: MakoSnapshot.ContainerId;
		Request.ExpectedSourceRevision = SourceSnapshot.Revision;
		Request.ExpectedDestinationRevision =
			Direction == EAIRECompanionTestingInventoryTransferDirection::MakoToWarehouse
			? WarehouseSnapshot.Revision
			: MakoSnapshot.Revision;
		Request.SourceSlotIndex = SourceStack.SlotIndex;
		Request.Count = Count;
		return InventorySubsystem->TryTransferItem(Request).WasApplied();
	}

	UAI_REPlayerInventoryComponent* PlayerInventory = FindFirstPlayerInventory(WorldContextObject);
	if (!IsValid(PlayerInventory))
	{
		return false;
	}

	FAIREPlayerWarehouseTransferRequest Request;
	Request.SessionId = InventorySubsystem->GetInventorySessionId();
	Request.MutationId = FGuid::NewGuid();
	Request.ExpectedWarehouseRevision = WarehouseSnapshot.Revision;
	Request.Count = Count;
	if (Direction == EAIRECompanionTestingInventoryTransferDirection::PlayerToWarehouse)
	{
		const FInventoryItemStack* SourceStack = PlayerInventory->Items.FindByPredicate(
			[ItemId, PlayerInventory](const FInventoryItemStack& Stack)
			{
				return Stack.SlotIndex >= 0
					&& Stack.SlotIndex < PlayerInventory->MaxSlots
					&& Stack.ItemId == ItemId
					&& Stack.Count > 0;
			});
		if (!SourceStack)
		{
			return false;
		}

		Request.Direction = EAIREPlayerWarehouseTransferDirection::DepositPlayerToWarehouse;
		Request.SourceSlotIndex = SourceStack->SlotIndex;
	}
	else
	{
		FAIREInventoryItemStackSnapshot SourceStack;
		if (!FindFirstMatchingStack(WarehouseSnapshot, ItemId, SourceStack))
		{
			return false;
		}

		Request.Direction = EAIREPlayerWarehouseTransferDirection::WithdrawWarehouseToPlayer;
		Request.SourceSlotIndex = SourceStack.SlotIndex;
	}

	return InventorySubsystem->TryTransferPlayerWarehouse(PlayerInventory, Request).WasApplied();
}

bool UAIRECompanionTestingBlueprintLibrary::LogFirstCompanionInventoryState(
	const UObject* WorldContextObject)
{
	UAIREGameplayInventorySubsystem* InventorySubsystem = FindGameplayInventorySubsystem(WorldContextObject);
	if (!IsValid(InventorySubsystem))
	{
		return false;
	}

	FAIREInventoryContainerSnapshot MakoSnapshot;
	FAIREInventoryContainerSnapshot WarehouseSnapshot;
	if (!InventorySubsystem->GetContainerSnapshot(UAIREGameplayInventorySubsystem::GetMakoContainerId(), MakoSnapshot)
		|| !InventorySubsystem->GetContainerSnapshot(UAIREGameplayInventorySubsystem::GetSharedWarehouseContainerId(), WarehouseSnapshot))
	{
		return false;
	}

	LogInventorySnapshot(TEXT("MAKO"), MakoSnapshot);
	LogInventorySnapshot(TEXT("Shared Warehouse"), WarehouseSnapshot);
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::ResetGameplayInventorySession(
	const UObject* WorldContextObject)
{
	UAIREGameplayInventorySubsystem* InventorySubsystem = FindGameplayInventorySubsystem(WorldContextObject);
	return IsValid(InventorySubsystem) && InventorySubsystem->ResetInventorySession().IsValid();
}

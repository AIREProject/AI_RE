#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/AIRECompanionAIController.h"
#include "AIRECompanionTestingBlueprintLibrary.generated.h"

class UDataTable;

/** Temporary PIE-only Inventory transfer directions. Replace with M03-E08-T02 UI requests. */
UENUM(BlueprintType)
enum class EAIRECompanionTestingInventoryTransferDirection : uint8
{
	MakoToStorage,
	StorageToMako,
	PlayerToStorage,
	StorageToPlayer
};

/** Temporary PIE fixture helpers. Replace these with feature-owned inputs as each behavior is implemented. */
UCLASS()
class AI_RE_API UAIRECompanionTestingBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool SetFirstCompanionTestBehaviorRequest(
		const UObject* WorldContextObject,
		EAIRECompanionTestBehaviorRequest Request,
		bool bIsRequested);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool ClearFirstCompanionTestBehaviorRequests(const UObject* WorldContextObject);

	/** Temporary M03-E07-T02 PIE fixture. Replace with production work input. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool RequestFirstCompanionNearestCraftingWork(
		const UObject* WorldContextObject,
		UDataTable* CraftingRecipeTable);

	/** Temporary M03-E07-T02 PIE fixture. Replace with production work input. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool RequestFirstCompanionNearestHarvestWork(const UObject* WorldContextObject);

	/** Temporary M03-E07-T02 PIE fixture. Replace with production work cancellation input. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool CancelFirstCompanionWorkOrder(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool ApplyDamageToFirstCompanion(const UObject* WorldContextObject, float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool ResetFirstCompanionAttributes(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool LogFirstCompanionAbilityState(const UObject* WorldContextObject);

	/** Temporary M03-E08-T01 PIE setup. Replace with M03-E08-T02 Inventory UI input. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool SeedFirstCompanionInventoryItem(
		const UObject* WorldContextObject,
		FName ItemId,
		int32 Count);

	/** Temporary M03-E08-T01 PIE setup. Replace with M03-E08-T02 Inventory UI input. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool SeedFirstPlayerInventoryItem(
		const UObject* WorldContextObject,
		FName ItemId,
		int32 Count);

	/** Temporary M03-E08-T01 transfer fixture. Replace with M03-E08-T02 Inventory UI requests. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool TransferFirstInventoryItem(
		const UObject* WorldContextObject,
		EAIRECompanionTestingInventoryTransferDirection Direction,
		FName ItemId,
		int32 Count);

	/** Temporary M03-E08-T01 diagnostic. Replace with M03-E08-T02 Inventory UI presentation. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool LogFirstCompanionInventoryState(const UObject* WorldContextObject);

	/** Temporary M03-E08-T01 session fixture. Replace with M03-E08-T03 startup/load ownership. */
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Testing", meta = (WorldContext = "WorldContextObject"))
	static bool ResetGameplayInventorySession(const UObject* WorldContextObject);
};

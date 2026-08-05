// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI_RECraftingTypes.h"
#include "AI_REPlayerCraftingComponent.generated.h"

class UAI_REPlayerInventoryComponent;
class UAIREGameplayInventorySubsystem;
class UDataTable;
struct FAIREPlayerCraftRequest;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCraftingCompletedSignature, FName, CraftedItemId);

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAI_REPlayerCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAI_REPlayerCraftingComponent();

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void SetInventoryComponent(UAI_REPlayerInventoryComponent* InInventory) { CachedInventory = InInventory; }

	UFUNCTION(BlueprintPure, Category = "Crafting")
	UAI_REPlayerInventoryComponent* GetInventoryComponent() const { return CachedInventory; }

	UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
	FCraftingCompletedSignature OnCraftingCompletedEvent;

	// Recipe Data Table assigned in BP
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UDataTable> CraftingRecipeTable;

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool CanCraftRecipe(FName RecipeRowName) const;

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool StartCrafting(FName RecipeRowName);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void CancelCrafting();

	// Return a list of Recipe RowNames that can be crafted at the given WorkbenchType
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	TArray<FName> GetRecipesByWorkbench(EWorkbenchType WorkbenchType) const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsCrafting() const { return bIsCrafting; }

protected:
	virtual void BeginPlay() override;

private:
	bool BuildCraftRequest(
		const FAI_RECraftingRecipe& Recipe,
		const FGuid& MutationId,
		const FGuid& SessionId,
		FAIREPlayerCraftRequest& OutRequest) const;
	UAIREGameplayInventorySubsystem* GetInventorySubsystem() const;
	void CompleteCrafting();
	void ResetCraftingState();

	UPROPERTY()
	TObjectPtr<UAI_REPlayerInventoryComponent> CachedInventory;

	bool bIsCrafting = false;
	FName CurrentRecipeName;
	FGuid CurrentCraftMutationId;
	FGuid CurrentCraftSessionId;
	
	UPROPERTY()
	FTimerHandle CraftingTimerHandle;
};

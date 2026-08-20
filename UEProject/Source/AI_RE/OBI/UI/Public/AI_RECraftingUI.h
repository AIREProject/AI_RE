// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AI_RECraftingTypes.h"
#include "AI_RECraftingUI.generated.h"

class UAI_REPlayerCraftingComponent;
class UScrollBox;
class UButton;
class UAI_RECraftingRecipeRowUI;

/**
 * Base UI class for the Crafting Interface
 */
UCLASS(Abstract)
class AI_RE_API UAI_RECraftingUI : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Called by Row UI when clicked
	void OnRecipeSelected(FName SelectedRecipeName);

	// Called by character when interacting with a workbench
	void InitializeCrafting(UAI_REPlayerCraftingComponent* InCraftingComp, EWorkbenchType InFilterType);

protected:
	virtual void NativeOnInitialized() override;

	// Automatically bound widgets from blueprint
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> RecipeScrollBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Craft;

	// Right panel detail image
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> RecipeIMG;

	// Right panel detail name (Title)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> RecipeText;

	// Top panel Workbench Name
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> WorkbenchNameText;

	// Blueprint class to use for spawning row buttons
	UPROPERTY(EditDefaultsOnly, Category = "Crafting UI")
	TSubclassOf<UAI_RECraftingRecipeRowUI> RecipeRowClass;

	// Allow Blueprint to update detail panel visuals (Icon, Ingredients list) when selected
	UFUNCTION(BlueprintImplementableEvent, Category = "Crafting UI")
	void BP_UpdateRecipeDetails(FName SelectedRecipeName);

	UFUNCTION()
	void OnCraftButtonClicked();

private:
	UPROPERTY()
	TObjectPtr<UAI_REPlayerCraftingComponent> CraftingComp;

	EWorkbenchType CurrentFilterType;
	FName CurrentSelectedRecipe;
	
	void PopulateRecipeList();
};

// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AI_RECraftingRecipeRowUI.generated.h"

class UTextBlock;
class UButton;
class UAI_RECraftingUI;

/**
 * Single Recipe Row Button in the Crafting UI List
 */
UCLASS(Abstract)
class AI_RE_API UAI_RECraftingRecipeRowUI : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void InitializeRow(FName InRecipeName, struct FAI_RECraftingRecipe* RecipeData, class UAI_REItemDataAsset* ResultItemDA, UAI_RECraftingUI* InMainUI);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_SelectRecipe;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> RecipeIMG;

	UFUNCTION()
	void OnButtonClicked();

private:
	FName RecipeName;
	
	UPROPERTY()
	TObjectPtr<UAI_RECraftingUI> MainUI;
};

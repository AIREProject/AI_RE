// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECraftingRecipeRowUI.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "AI_REItemDataAsset.h"
#include "AI_RECraftingUI.h"

void UAI_RECraftingRecipeRowUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (Btn_SelectRecipe)
	{
		Btn_SelectRecipe->OnClicked.AddUniqueDynamic(this, &UAI_RECraftingRecipeRowUI::OnButtonClicked);
	}
}

void UAI_RECraftingRecipeRowUI::InitializeRow(FName InRecipeName, FAI_RECraftingRecipe* RecipeData, UAI_REItemDataAsset* ResultItemDA, UAI_RECraftingUI* InMainUI)
{
	RecipeName = InRecipeName;
	MainUI = InMainUI;

	if (RecipeNameText)
	{
		// Use DisplayName from DA if available, otherwise fallback to RecipeName
		if (ResultItemDA && !ResultItemDA->DisplayName.IsEmpty())
		{
			RecipeNameText->SetText(ResultItemDA->DisplayName);
		}
		else
		{
			RecipeNameText->SetText(FText::FromName(RecipeName));
		}
	}

	if (RecipeIMG)
	{
		UTexture2D* CraftingImage = ResultItemDA
			? ResultItemDA->CraftingImage.Get()
			: nullptr;

		RecipeIMG->SetBrushFromTexture(CraftingImage);
		RecipeIMG->SetVisibility(
			CraftingImage ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UAI_RECraftingRecipeRowUI::OnButtonClicked()
{
	if (MainUI)
	{
		MainUI->OnRecipeSelected(RecipeName);
	}
}

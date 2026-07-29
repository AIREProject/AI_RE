// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECraftingRecipeRowUI.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "AI_REItemDataAsset.h"
#include "AI_RECraftingUI.h"

void UAI_RECraftingRecipeRowUI::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (Btn_SelectRecipe)
	{
		Btn_SelectRecipe->OnClicked.AddDynamic(this, &UAI_RECraftingRecipeRowUI::OnButtonClicked);
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

	if (RecipeIMG && ResultItemDA)
	{
		if (ResultItemDA->CraftingImage)
		{
			RecipeIMG->SetBrushFromTexture(ResultItemDA->CraftingImage);
		}
		else if (ResultItemDA->ItemIcon)
		{
			RecipeIMG->SetBrushFromTexture(ResultItemDA->ItemIcon);
		}
	}
}

void UAI_RECraftingRecipeRowUI::OnButtonClicked()
{
	if (MainUI)
	{
		MainUI->OnRecipeSelected(RecipeName);
	}
}

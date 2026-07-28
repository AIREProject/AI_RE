// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECraftingRecipeRowUI.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "../../Global/Components/Public/AI_REItemSubsystem.h"
#include "../../OBI/Component/Public/AI_REItemDataAsset.h"
#include "AI_RECraftingUI.h"

void UAI_RECraftingRecipeRowUI::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (Btn_SelectRecipe)
	{
		Btn_SelectRecipe->OnClicked.AddDynamic(this, &UAI_RECraftingRecipeRowUI::OnButtonClicked);
	}
}

void UAI_RECraftingRecipeRowUI::InitializeRow(FName InRecipeName, UAI_RECraftingUI* InMainUI)
{
	RecipeName = InRecipeName;
	MainUI = InMainUI;

	if (RecipeNameText)
	{
		RecipeNameText->SetText(FText::FromName(RecipeName));
	}

	if (RecipeIMG)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UAI_REItemSubsystem* ItemSubsystem = GI->GetSubsystem<UAI_REItemSubsystem>())
			{
				if (UAI_REItemDataAsset* DA = ItemSubsystem->GetItemDataAsset(RecipeName))
				{
					if (DA->ItemIcon)
					{
						RecipeIMG->SetBrushFromTexture(DA->ItemIcon);
					}
				}
			}
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

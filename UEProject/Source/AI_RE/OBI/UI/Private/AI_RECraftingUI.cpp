// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECraftingUI.h"
#include "AI_RECraftingRecipeRowUI.h"
#include "AI_REPlayerCraftingComponent.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "AI_REItemSubsystem.h"
#include "AI_REItemDataAsset.h"
#include "Engine/Engine.h"

void UAI_RECraftingUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Btn_Craft)
	{
		Btn_Craft->OnClicked.AddUniqueDynamic(this, &UAI_RECraftingUI::OnCraftButtonClicked);
	}
}

void UAI_RECraftingUI::InitializeCrafting(UAI_REPlayerCraftingComponent* InCraftingComp, EWorkbenchType InFilterType)
{
	CraftingComp = InCraftingComp;
	CurrentFilterType = InFilterType;
	CurrentSelectedRecipe = NAME_None;

	if (WorkbenchNameText)
	{
		FString TypeString;
		switch (CurrentFilterType)
		{
			case EWorkbenchType::Basic: TypeString = TEXT("Basic Workbench"); break;
			case EWorkbenchType::Blacksmith: TypeString = TEXT("Blacksmith Forge"); break;
			case EWorkbenchType::Alchemy: TypeString = TEXT("Alchemy Table"); break;
			case EWorkbenchType::None: TypeString = TEXT("Handcraft"); break;
			default: TypeString = TEXT("Workbench"); break;
		}
		WorkbenchNameText->SetText(FText::FromString(TypeString));
	}

	PopulateRecipeList();
}

void UAI_RECraftingUI::PopulateRecipeList()
{
	if (!RecipeScrollBox || !CraftingComp || !RecipeRowClass || !CraftingComp->CraftingRecipeTable)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("PopulateRecipeList Failed! Check BindWidgets or RecipeRowClass in BP."));
		return;
	}

	RecipeScrollBox->ClearChildren();

	TArray<FName> AvailableRecipes = CraftingComp->GetRecipesByWorkbench(CurrentFilterType);
	
	UAI_REItemSubsystem* ItemSubsystem = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		ItemSubsystem = GI->GetSubsystem<UAI_REItemSubsystem>();
	}

	for (const FName& RecipeName : AvailableRecipes)
	{
		if (UAI_RECraftingRecipeRowUI* RowUI = CreateWidget<UAI_RECraftingRecipeRowUI>(this, RecipeRowClass))
		{
			UAI_REItemDataAsset* ResultDA = nullptr;
			FAI_RECraftingRecipe* RecipeData = CraftingComp->CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(RecipeName, TEXT("CraftingUI"));
			
			if (RecipeData && ItemSubsystem)
			{
				ResultDA = ItemSubsystem->GetItemDataAsset(RecipeData->ResultItemId);
			}

			RowUI->InitializeRow(RecipeName, RecipeData, ResultDA, this);
			RecipeScrollBox->AddChild(RowUI);
		}
	}
}

void UAI_RECraftingUI::OnRecipeSelected(FName SelectedRecipeName)
{
	CurrentSelectedRecipe = SelectedRecipeName;
	
	// Automate setting the Detail Image
	if (RecipeIMG)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UAI_REItemSubsystem* ItemSubsystem = GI->GetSubsystem<UAI_REItemSubsystem>())
			{
				if (CraftingComp && CraftingComp->CraftingRecipeTable)
				{
					if (FAI_RECraftingRecipe* RecipeData = CraftingComp->CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(SelectedRecipeName, TEXT("CraftingUI")))
					{
						if (UAI_REItemDataAsset* DA = ItemSubsystem->GetItemDataAsset(RecipeData->ResultItemId))
						{
							if (DA->CraftingImage)
							{
								RecipeIMG->SetBrushFromTexture(DA->CraftingImage);
							}
							else if (DA->ItemIcon)
							{
								RecipeIMG->SetBrushFromTexture(DA->ItemIcon);
							}
						}
					}
				}
			}
		}
	}

	if (RecipeText)
	{
		RecipeText->SetText(FText::FromName(SelectedRecipeName));
	}

	// Fire blueprint event to update right panel visuals (Text, Ingredients, etc)
	BP_UpdateRecipeDetails(SelectedRecipeName);
}

void UAI_RECraftingUI::OnCraftButtonClicked()
{
	if (CraftingComp && !CurrentSelectedRecipe.IsNone())
	{
		bool bSuccess = CraftingComp->StartCrafting(CurrentSelectedRecipe);
		if (!bSuccess)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Crafting Failed! (Missing ingredients or busy)"));
		}
	}
}

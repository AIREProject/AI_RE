// Copyright MixUpProject. All Rights Reserved.

#include "AI_RECraftingUI.h"
#include "AI_RECraftingRecipeRowUI.h"
#include "AI_REPlayerCraftingComponent.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "AI_REItemSubsystem.h"
#include "AI_REItemDataAsset.h"
#include "AI_REWeaponItemDataAsset.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "Engine/Engine.h"

void UAI_RECraftingUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Btn_Craft)
	{
		Btn_Craft->OnClicked.AddUniqueDynamic(this, &UAI_RECraftingUI::OnCraftButtonClicked);
	}
}

void UAI_RECraftingUI::NativeDestruct()
{
	ResetRecipeDetails();
	CraftingComp = nullptr;
	CurrentFilterType = EWorkbenchType::None;

	Super::NativeDestruct();
}

void UAI_RECraftingUI::InitializeCrafting(UAI_REPlayerCraftingComponent* InCraftingComp, EWorkbenchType InFilterType)
{
	CraftingComp = InCraftingComp;
	CurrentFilterType = InFilterType;
	ResetRecipeDetails();

	if (WorkbenchNameText)
	{
		FString TypeString;
		switch (CurrentFilterType)
		{
			case EWorkbenchType::Basic: TypeString = TEXT("Basic Workbench"); break;
			case EWorkbenchType::Blacksmith: TypeString = TEXT("Blacksmith Forge"); break;
			case EWorkbenchType::Smelter: TypeString = TEXT("Smelting Furnace"); break;
			case EWorkbenchType::Alchemy: TypeString = TEXT("Alchemy Table"); break;
			case EWorkbenchType::Cook: TypeString = TEXT("Cooking Station"); break;
			case EWorkbenchType::None: TypeString = TEXT("Handcraft"); break;
			default: TypeString = TEXT("Workbench"); break;
		}
		WorkbenchNameText->SetText(FText::FromString(TypeString));
	}

	BP_UpdateWorkbenchContext(CurrentFilterType);

	PopulateRecipeList();
}

void UAI_RECraftingUI::ResetRecipeDetails()
{
	CurrentSelectedRecipe = NAME_None;

	if (RecipeIMG)
	{
		RecipeIMG->SetBrushFromTexture(nullptr);
		RecipeIMG->SetVisibility(ESlateVisibility::Hidden);
	}

	if (RecipeText)
	{
		RecipeText->SetText(NSLOCTEXT("CraftingUI", "SelectRecipePrompt", "레시피를 선택하세요"));
	}

	if (RecipeDescriptionText)
	{
		RecipeDescriptionText->SetText(NSLOCTEXT(
			"CraftingUI",
			"RecipeDescriptionPrompt",
			"레시피를 선택하면 상세 설명이 표시됩니다."));
	}

	if (IngredientSummaryText)
	{
		IngredientSummaryText->SetText(NSLOCTEXT(
			"CraftingUI",
			"IngredientSummaryPrompt",
			"재료 정보 대기 중"));
	}

	if (CraftingTimeText)
	{
		CraftingTimeText->SetText(NSLOCTEXT("CraftingUI", "CraftingTimePrompt", "제작 시간  --"));
	}

	if (WeaponStatsPanel)
	{
		WeaponStatsPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (WeaponStatsText)
	{
		WeaponStatsText->SetText(NSLOCTEXT(
			"CraftingUI",
			"WeaponStatsPrompt",
			"공격력 --   |   경직 --   |   사거리 --"));
	}
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

	const FAI_RECraftingRecipe* SelectedRecipeData = nullptr;
	UAI_REItemDataAsset* ResultItemData = nullptr;
	UAI_REItemSubsystem* ItemSubsystem = nullptr;

	if (UGameInstance* GI = GetGameInstance())
	{
		ItemSubsystem = GI->GetSubsystem<UAI_REItemSubsystem>();
	}

	if (CraftingComp && CraftingComp->CraftingRecipeTable)
	{
		SelectedRecipeData = CraftingComp->CraftingRecipeTable->FindRow<FAI_RECraftingRecipe>(
			SelectedRecipeName,
			TEXT("CraftingUI"));

		if (SelectedRecipeData && ItemSubsystem)
		{
			ResultItemData = ItemSubsystem->GetItemDataAsset(SelectedRecipeData->ResultItemId);
		}
	}

	// Automate setting the detail image.
	if (RecipeIMG)
	{
		UTexture2D* PreviewTexture = nullptr;
		if (ResultItemData)
		{
			PreviewTexture = ResultItemData->CraftingImage
				? ResultItemData->CraftingImage.Get()
				: ResultItemData->ItemIcon.Get();
		}

		RecipeIMG->SetBrushFromTexture(PreviewTexture);
		RecipeIMG->SetVisibility(
			PreviewTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (RecipeText)
	{
		RecipeText->SetText(
			ResultItemData && !ResultItemData->DisplayName.IsEmpty()
				? ResultItemData->DisplayName
				: FText::FromName(SelectedRecipeName));
	}

	if (RecipeDescriptionText)
	{
		RecipeDescriptionText->SetText(
			ResultItemData && !ResultItemData->Description.IsEmpty()
				? ResultItemData->Description
				: NSLOCTEXT("CraftingUI", "MissingRecipeDescription", "상세 설명이 없습니다."));
	}

	if (SelectedRecipeData && IngredientSummaryText)
	{
		TArray<FString> IngredientLines;
		IngredientLines.Reserve(SelectedRecipeData->Ingredients.Num());

		for (const FAI_RECraftingIngredient& Ingredient : SelectedRecipeData->Ingredients)
		{
			FText IngredientName = FText::FromName(Ingredient.ItemId);
			if (ItemSubsystem)
			{
				if (const UAI_REItemDataAsset* IngredientData = ItemSubsystem->GetItemDataAsset(Ingredient.ItemId))
				{
					if (!IngredientData->DisplayName.IsEmpty())
					{
						IngredientName = IngredientData->DisplayName;
					}
				}
			}

			IngredientLines.Add(FString::Printf(
				TEXT("%s  ×%d"),
				*IngredientName.ToString(),
				Ingredient.Amount));
		}

		IngredientSummaryText->SetText(FText::FromString(FString::Join(IngredientLines, TEXT("\n"))));
	}

	if (SelectedRecipeData && CraftingTimeText)
	{
		CraftingTimeText->SetText(FText::Format(
			NSLOCTEXT("CraftingUI", "CraftingTimeFormat", "제작 시간  {0}초"),
			FText::AsNumber(SelectedRecipeData->CraftingTime)));
	}

	const UAI_REWeaponItemDataAsset* WeaponItemData = Cast<UAI_REWeaponItemDataAsset>(ResultItemData);
	const bool bShowWeaponStats =
		CurrentFilterType == EWorkbenchType::Blacksmith && WeaponItemData != nullptr;

	if (WeaponStatsPanel)
	{
		WeaponStatsPanel->SetVisibility(
			bShowWeaponStats ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (bShowWeaponStats && WeaponStatsText)
	{
		if (const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition = WeaponItemData->WeaponDefinition)
		{
			WeaponStatsText->SetText(FText::Format(
				NSLOCTEXT(
					"CraftingUI",
					"WeaponStatsFormat",
					"공격력 {0}   |   경직 {1}   |   사거리 {2}"),
				FText::AsNumber(WeaponDefinition->Damage),
				FText::AsNumber(WeaponDefinition->StaggerValue),
				FText::AsNumber(WeaponDefinition->AttackRange)));
		}
		else
		{
			WeaponStatsText->SetText(
				NSLOCTEXT("CraftingUI", "MissingWeaponStats", "무기 전투 정보가 없습니다."));
		}
	}

	// Fire blueprint event to update right panel visuals (Text, Ingredients, etc)
	BP_UpdateRecipeDetails(SelectedRecipeName);

	if (SelectedRecipeData)
	{
		BP_UpdateRecipeData(SelectedRecipeName, *SelectedRecipeData, ResultItemData);
	}
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

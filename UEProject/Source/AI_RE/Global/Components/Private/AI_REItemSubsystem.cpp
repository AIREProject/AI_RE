// Copyright MixUpProject. All Rights Reserved.

#include "AI_REItemSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"

void UAI_REItemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	LoadAllItemDataAssets();
}

void UAI_REItemSubsystem::LoadAllItemDataAssets()
{
	ItemCache.Empty();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if (AssetRegistry.IsLoadingAssets())
	{
		AssetRegistry.SearchAllAssets(true);
	}

	FARFilter Filter;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	Filter.ClassPaths.Add(UAI_REItemDataAsset::StaticClass()->GetClassPathName());
#else
	Filter.ClassNames.Add(UAI_REItemDataAsset::StaticClass()->GetFName());
#endif
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	for (const FAssetData& AssetData : AssetList)
	{
		if (UObject* LoadedAsset = AssetData.GetAsset())
		{
			if (UAI_REItemDataAsset* ItemDA = Cast<UAI_REItemDataAsset>(LoadedAsset))
			{
				if (!ItemDA->ItemId.IsNone())
				{
					ItemCache.Add(ItemDA->ItemId, ItemDA);
				}
			}
		}
	}
	
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("ItemSubsystem: Successfully loaded %d DataAssets!"), ItemCache.Num()));
}

UAI_REItemDataAsset* UAI_REItemSubsystem::GetItemDataAsset(FName ItemId) const
{
	if (ItemId.IsNone())
		return nullptr;

	if (UAI_REItemDataAsset* const* FoundAsset = ItemCache.Find(ItemId))
	{
		return *FoundAsset;
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("ItemSubsystem: Could NOT find DataAsset for ItemId [%s]! Cache size: %d"), *ItemId.ToString(), ItemCache.Num()));
	return nullptr;
}

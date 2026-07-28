// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "../../OBI/Component/Public/AI_REItemDataAsset.h"
#include "AI_REItemSubsystem.generated.h"

/**
 * Global Subsystem for managing item data lookups efficiently.
 */
UCLASS()
class AI_RE_API UAI_REItemSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Retrieves the Item DataAsset for the given ItemId. Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category = "Item|Data")
	UAI_REItemDataAsset* GetItemDataAsset(FName ItemId) const;

	/** Sets the DataTable used for looking up item data. Usually called early on or set via BP. */
	UFUNCTION(BlueprintCallable, Category = "Item|Data")
	void SetItemDataTable(UDataTable* InTable);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Item|Data")
	TObjectPtr<UDataTable> ItemDataTable;

	// Optional: Cache for fast lookups if the table is large
	TMap<FName, UAI_REItemDataAsset*> ItemCache;
};

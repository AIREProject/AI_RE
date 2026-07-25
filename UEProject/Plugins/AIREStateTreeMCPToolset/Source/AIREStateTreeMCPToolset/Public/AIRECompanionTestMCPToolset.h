#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AIRECompanionTestMCPToolset.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct FAIRECompanionTestFixtureResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Testing")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Testing")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Testing")
	int32 NodeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Testing")
	TArray<FString> CompilerMessages;
};

/** Project-scoped MCP tools for temporary behavior and Companion GAS Level Blueprint inputs. */
UCLASS(BlueprintType)
class AIRESTATETREEMCPTOOLSET_API UAIRECompanionTestMCPToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable), Category = "AIRE|Companion|Testing")
	static FAIRECompanionTestFixtureResult InspectBehaviorTestInputs(UWorld* LevelWorld);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Companion|Testing")
	static FAIRECompanionTestFixtureResult ConfigureBehaviorTestInputs(UWorld* LevelWorld);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Companion|Testing")
	static FAIRECompanionTestFixtureResult RemoveBehaviorTestInputs(UWorld* LevelWorld);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Companion|Testing")
	static FAIRECompanionTestFixtureResult ValidateAndCompileLevelBlueprint(UWorld* LevelWorld);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Companion|Testing")
	static FAIRECompanionTestFixtureResult SaveLevelBlueprint(UWorld* LevelWorld);
};

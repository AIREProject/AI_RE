#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AIREUMGMCPToolset.generated.h"

class UWidgetBlueprint;

USTRUCT(BlueprintType)
struct FAIREWidgetAnimationInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	FName AnimationName;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	FName WidgetName;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	float DurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	int32 TrackCount = 0;
};

USTRUCT(BlueprintType)
struct FAIREWidgetBlueprintInspection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	int32 WidgetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	TArray<FAIREWidgetAnimationInfo> Animations;
};

USTRUCT(BlueprintType)
struct FAIREWidgetMutationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|UMG")
	TArray<FString> CompilerMessages;
};

/**
 * Project-scoped UMG animation and lifecycle tools.
 * Widget tree editing remains owned by UE's built-in UMGToolSet.
 */
UCLASS(BlueprintType)
class AIRESTATETREEMCPTOOLSET_API UAIREUMGMCPToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable), Category = "AIRE|UMG|Query")
	static FAIREWidgetBlueprintInspection InspectWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint);

	UFUNCTION(meta = (AICallable), Category = "AIRE|UMG|Animation")
	static FAIREWidgetMutationResult CreateOrReplaceSlideFadeAnimation(
		UWidgetBlueprint* WidgetBlueprint,
		FName AnimationName,
		FName WidgetName,
		float DurationSeconds,
		float StartTranslationX,
		float EndTranslationX,
		float StartOpacity,
		float EndOpacity);

	UFUNCTION(meta = (AICallable), Category = "AIRE|UMG|Animation")
	static FAIREWidgetMutationResult RemoveWidgetAnimation(
		UWidgetBlueprint* WidgetBlueprint,
		FName AnimationName);

	UFUNCTION(meta = (AICallable), Category = "AIRE|UMG|Lifecycle")
	static FAIREWidgetMutationResult ValidateAndCompileWidgetBlueprint(
		UWidgetBlueprint* WidgetBlueprint);

	UFUNCTION(meta = (AICallable), Category = "AIRE|UMG|Lifecycle")
	static FAIREWidgetMutationResult SaveWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint);
};

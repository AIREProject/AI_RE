#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AIREAnimationMCPToolset.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FAIREAnimationComboMontageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 SectionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 HitNotifyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 ComboWindowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 AddedComboWindowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	TArray<FString> Entries;
};

/** Project-scoped MCP tools for inspecting and configuring Companion combo montages. */
UCLASS(BlueprintType)
class AIRESTATETREEMCPTOOLSET_API UAIREAnimationMCPToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Query")
	static FAIREAnimationComboMontageResult InspectBasicAttackComboMontage(UAnimMontage* Montage);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationComboMontageResult ConfigureBasicAttackComboSectionsFromHits(
		UAnimMontage* Montage,
		const TArray<FName>& SectionNames,
		float TransitionBias);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationComboMontageResult ConfigureBasicAttackComboWindows(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		float WindowStartOffsetAfterHit,
		float SectionEndPadding);
};

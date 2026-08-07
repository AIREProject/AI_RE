#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AIREAnimationMCPToolset.generated.h"

class UAnimMontage;
class UControlRigBlueprint;
class USkeletalMesh;

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

USTRUCT(BlueprintType)
struct FAIREControlRigHierarchySyncResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|ControlRig")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|ControlRig")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|ControlRig")
	int32 SyncedBoneCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|ControlRig")
	int32 DiscrepancyCountBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|ControlRig")
	int32 DiscrepancyCountAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|ControlRig")
	TArray<FString> Entries;
};

/** Project-scoped MCP tools for inspecting and configuring Companion animation assets. */
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

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|ControlRig|Mutation")
	static FAIREControlRigHierarchySyncResult SyncControlRigBoneHierarchy(
		UControlRigBlueprint* ControlRigBlueprint,
		USkeletalMesh* SkeletalMesh);
};

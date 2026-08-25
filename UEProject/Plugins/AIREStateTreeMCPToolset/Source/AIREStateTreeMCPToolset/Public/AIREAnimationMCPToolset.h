#pragma once

#include "CoreMinimal.h"
#include "Animation/AIRECompanionMeleeTraceAnimNotifyState.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AIREAnimationMCPToolset.generated.h"

class UAnimMontage;
class UAnimSequence;
class UAnimSequenceBase;
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
struct FAIREAnimationNotifyMutationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 LegacyNotifyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 TraceWindowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 AttackMovementWindowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 TempoWindowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 RemovedLegacyNotifyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 ReplacedTraceWindowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 ReplacedAttackMovementWindowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 ReplacedTempoWindowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	TArray<FString> Entries;
};

USTRUCT(BlueprintType)
struct FAIREEnemyMeleeTraceWindowDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float StartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float EndTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	int32 StrikeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float DamageScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float StaggerScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	FName TraceStartSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	FName TraceEndSocket = NAME_None;
};

USTRUCT(BlueprintType)
struct FAIREEnemyAttackTempoWindowDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float StartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float StrikeStartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float EndTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	int32 StrikeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float AnticipationPlayRateMultiplier = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float StrikePlayRateMultiplier = 1.60f;
};

USTRUCT(BlueprintType)
struct FAIRECompanionMeleeTraceWindowDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float StartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float EndTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	EAIRECompanionMeleeTraceMode TraceMode =
		EAIRECompanionMeleeTraceMode::BasicAttack;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "AIRE|Animation",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 ComboStepIndex = 0;
};

USTRUCT(BlueprintType)
struct FAIREAnimationMontageSegmentDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation")
	int32 LoopingCount = 1;
};

USTRUCT(BlueprintType)
struct FAIREAnimationMontageTrackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	int32 SegmentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	float PlayLength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	TArray<FString> Entries;
};

USTRUCT(BlueprintType)
struct FAIREAnimationBoneMotionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation")
	float PlayLength = 0.0f;

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

USTRUCT(BlueprintType)
struct FAIREControlRigVMResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|ControlRig")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|ControlRig")
	FString Message;

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
	static FAIREAnimationComboMontageResult ConfigureBasicAttackHitNotifies(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		const TArray<float>& HitTimes);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationComboMontageResult ConfigureKatanaAttachmentNotify(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		float NotifyTime,
		bool bAttachToHand);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationComboMontageResult ConfigureBasicAttackComboSectionsFromHits(
		UAnimMontage* Montage,
		const TArray<FName>& SectionNames,
		float TransitionBias);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationComboMontageResult ConfigureMontageSections(
		UAnimMontage* Montage,
		const TArray<FName>& SectionNames,
		const TArray<float>& SectionStartTimes);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationComboMontageResult ConfigureBasicAttackComboWindows(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		float WindowStartOffsetAfterHit,
		float SectionEndPadding,
		int32 ComboStepCount,
		const TArray<int32>& ComboVariantStepCounts);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Query")
	static FAIREAnimationNotifyMutationResult InspectEnemyMeleeTraceMontage(
		UAnimMontage* Montage);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationNotifyMutationResult ConfigureEnemyMeleeTraceWindow(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		float WindowStartTime,
		float WindowEndTime);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationNotifyMutationResult ConfigureEnemyAttackMovementWindow(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		float WindowStartTime,
		float WindowEndTime);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationNotifyMutationResult ConfigureEnemyMeleeTraceWindows(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		const TArray<FAIREEnemyMeleeTraceWindowDefinition>& Windows);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Query")
	static FAIREAnimationNotifyMutationResult InspectEnemyAttackTempoMontage(
		UAnimMontage* Montage);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationNotifyMutationResult ConfigureEnemyAttackTempoWindows(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		const TArray<FAIREEnemyAttackTempoWindowDefinition>& Windows);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Query")
	static FAIREAnimationNotifyMutationResult InspectCompanionMeleeTraceMontage(
		UAnimMontage* Montage);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationNotifyMutationResult ConfigureCompanionMeleeTraceWindows(
		UAnimMontage* Montage,
		FName NotifyTrackName,
		const TArray<FAIRECompanionMeleeTraceWindowDefinition>& Windows);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationMontageTrackResult ConfigureMontageAnimationTrack(
		UAnimMontage* Montage,
		FName SlotName,
		const TArray<FAIREAnimationMontageSegmentDefinition>& Segments,
		bool bClearMontageNotifies);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Query")
	static FAIREAnimationBoneMotionResult InspectAnimationBoneMotion(
		UAnimSequence* Animation,
		USkeletalMesh* SkeletalMesh,
		const TArray<FName>& BoneNames,
		int32 SampleCount);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Query")
	static FAIREAnimationBoneMotionResult InspectAnimationBoneLocalRotations(
		UAnimSequence* Animation,
		USkeletalMesh* SkeletalMesh,
		const TArray<FName>& BoneNames,
		const TArray<float>& NormalizedTimes);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Query")
	static FAIREAnimationBoneMotionResult InspectAnimationBoneLocalTransforms(
		UAnimSequence* Animation,
		USkeletalMesh* SkeletalMesh,
		const TArray<FName>& BoneNames,
		const TArray<float>& NormalizedTimes);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationBoneMotionResult BakePlanarRootMotionFromBone(
		UAnimSequence* Animation,
		USkeletalMesh* SkeletalMesh,
		FName MotionBoneName);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Mutation")
	static FAIREAnimationBoneMotionResult NormalizeRootRotationToReference(
		UAnimSequence* Animation,
		USkeletalMesh* SkeletalMesh,
		FName MotionBoneName);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|ControlRig|Mutation")
	static FAIREControlRigHierarchySyncResult SyncControlRigBoneHierarchy(
		UControlRigBlueprint* ControlRigBlueprint,
		USkeletalMesh* SkeletalMesh);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|ControlRig|Query")
	static FAIREControlRigVMResult InspectControlRigVMPins(
		UControlRigBlueprint* ControlRigBlueprint,
		const FString& Filter);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|ControlRig|Mutation")
	static FAIREControlRigVMResult SetControlRigVMPinDefault(
		UControlRigBlueprint* ControlRigBlueprint,
		const FString& GraphName,
		const FString& PinPath,
		const FString& DefaultValue);
};

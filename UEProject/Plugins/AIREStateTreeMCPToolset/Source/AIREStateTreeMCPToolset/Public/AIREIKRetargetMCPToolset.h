#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AIREIKRetargetMCPToolset.generated.h"

class UAnimationAsset;
class UIKRigDefinition;
class UIKRetargeter;
class UObject;
class USkeletalMesh;

UENUM(BlueprintType)
enum class EAIRERetargetChainRotationMode : uint8
{
	Interpolated,
	OneToOne,
	OneToOneReversed,
	None
};

UENUM(BlueprintType)
enum class EAIRERetargetAutoAlignMethod : uint8
{
	Direction,
	Mesh,
	LocalRotationAxes,
	GlobalRotationAxes
};

USTRUCT(BlueprintType)
struct FAIREIKRetargetSetupResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	TObjectPtr<UIKRigDefinition> SourceIKRig;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	TObjectPtr<UIKRigDefinition> TargetIKRig;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	TObjectPtr<UIKRetargeter> Retargeter;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	TArray<FString> Entries;
};

USTRUCT(BlueprintType)
struct FAIRERetargetPoseBoneRotation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation|Retarget")
	FName BoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Animation|Retarget")
	FRotator RotationOffset = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FAIRERetargetBatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	TArray<TObjectPtr<UObject>> CreatedAssets;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	TArray<FString> Entries;
};

USTRUCT(BlueprintType)
struct FAIREMakoWeaponSocketSetupResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	TObjectPtr<USkeletalMesh> MakoMesh;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Animation|Retarget")
	TArray<FString> Entries;
};

/** Project-scoped tools for creating and tuning the Frank Dual to MAKO retarget setup. */
UCLASS(BlueprintType)
class AIRESTATETREEMCPTOOLSET_API UAIREIKRetargetMCPToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Retarget|Mutation")
	static FAIREIKRetargetSetupResult CreateOrUpdateFrankToMakoRetargetSetup(
		USkeletalMesh* FrankMesh,
		USkeletalMesh* MakoMesh);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Retarget|Mutation")
	static FAIREMakoWeaponSocketSetupResult CreateOrUpdateMakoWeaponSockets(
		USkeletalMesh* FrankMesh,
		USkeletalMesh* MakoMesh);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Retarget|Query")
	static FAIREIKRetargetSetupResult InspectFrankToMakoRetargetSetup(
		UIKRetargeter* Retargeter);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Retarget|Mutation")
	static FAIREIKRetargetSetupResult SetMakoTargetRetargetPose(
		UIKRetargeter* Retargeter,
		const TArray<FAIRERetargetPoseBoneRotation>& BoneRotations,
		bool bResetExistingPose);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Retarget|Mutation")
	static FAIREIKRetargetSetupResult AutoAlignMakoTargetRetargetPoseBones(
		UIKRetargeter* Retargeter,
		const TArray<FName>& BoneNames,
		EAIRERetargetAutoAlignMethod AlignmentMethod,
		bool bResetBones);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Retarget|Mutation")
	static FAIREIKRetargetSetupResult SetFrankToMakoFKChainSettings(
		UIKRetargeter* Retargeter,
		FName TargetChainName,
		EAIRERetargetChainRotationMode RotationMode,
		float RotationAlpha);

	UFUNCTION(meta = (AICallable), Category = "AIRE|Animation|Retarget|Mutation")
	static FAIRERetargetBatchResult RetargetFrankAnimationsToMako(
		UIKRetargeter* Retargeter,
		USkeletalMesh* FrankMesh,
		USkeletalMesh* MakoMesh,
		const TArray<UAnimationAsset*>& Animations,
		const FString& Prefix);
};

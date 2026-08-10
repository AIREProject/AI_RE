#include "AIREIKRetargetMCPToolset.h"

#include "Animation/AnimationAsset.h"
#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "RetargetEditor/IKRetargetFactory.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargetChainMapping.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigController.h"
#include "RigEditor/IKRigDefinitionFactory.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AIREIKRetargetMCPToolset"

namespace
{
	const FString AllowedAssetRoot = TEXT("/Game/Work/LMK/");
	const FString SetupAssetFolder =
		TEXT("/Game/Work/LMK/Animations/Retarget/Setup");
	const FString FixedAnimationFolder =
		TEXT("/Game/Work/LMK/Animations/Retarget/Fixed");
	const FString SourceIKRigName = TEXT("IK_Frank_Dual");
	const FString TargetIKRigName = TEXT("IK_MAKO");
	const FString RetargeterName = TEXT("RTG_Frank_Dual_To_MAKO");

	struct FAIRERetargetChainDefinition
	{
		FName ChainName;
		FName StartBone;
		FName EndBone;
	};

	const TArray<FAIRERetargetChainDefinition> FrankChains =
	{
		{ TEXT("Spine"), TEXT("spine_01"), TEXT("spine_03") },
		{ TEXT("Neck"), TEXT("neck_01"), TEXT("head") },
		{ TEXT("LeftArm"), TEXT("clavicle_l"), TEXT("hand_l") },
		{ TEXT("RightArm"), TEXT("clavicle_r"), TEXT("hand_r") },
		{ TEXT("LeftThumb"), TEXT("thumb_01_l"), TEXT("thumb_03_l") },
		{ TEXT("LeftIndex"), TEXT("index_01_l"), TEXT("index_03_l") },
		{ TEXT("LeftMiddle"), TEXT("middle_01_l"), TEXT("middle_03_l") },
		{ TEXT("LeftRing"), TEXT("ring_01_l"), TEXT("ring_03_l") },
		{ TEXT("LeftPinky"), TEXT("pinky_01_l"), TEXT("pinky_03_l") },
		{ TEXT("RightThumb"), TEXT("thumb_01_r"), TEXT("thumb_03_r") },
		{ TEXT("RightIndex"), TEXT("index_01_r"), TEXT("index_03_r") },
		{ TEXT("RightMiddle"), TEXT("middle_01_r"), TEXT("middle_03_r") },
		{ TEXT("RightRing"), TEXT("ring_01_r"), TEXT("ring_03_r") },
		{ TEXT("RightPinky"), TEXT("pinky_01_r"), TEXT("pinky_03_r") },
		{ TEXT("LeftLeg"), TEXT("thigh_l"), TEXT("ball_l") },
		{ TEXT("RightLeg"), TEXT("thigh_r"), TEXT("ball_r") }
	};

	const TArray<FAIRERetargetChainDefinition> MakoChains =
	{
		{ TEXT("Spine"), TEXT("spine_01"), TEXT("spine_05") },
		{ TEXT("Neck"), TEXT("neck_01"), TEXT("head") },
		{ TEXT("LeftArm"), TEXT("clavicle_l"), TEXT("hand_l") },
		{ TEXT("RightArm"), TEXT("clavicle_r"), TEXT("hand_r") },
		{ TEXT("LeftThumb"), TEXT("thumb_01_l"), TEXT("thumb_03_l") },
		{ TEXT("LeftIndex"), TEXT("index_01_l"), TEXT("index_03_l") },
		{ TEXT("LeftMiddle"), TEXT("middle_01_l"), TEXT("middle_03_l") },
		{ TEXT("LeftRing"), TEXT("ring_01_l"), TEXT("ring_03_l") },
		{ TEXT("LeftPinky"), TEXT("pinky_01_l"), TEXT("pinky_03_l") },
		{ TEXT("RightThumb"), TEXT("thumb_01_r"), TEXT("thumb_03_r") },
		{ TEXT("RightIndex"), TEXT("index_01_r"), TEXT("index_03_r") },
		{ TEXT("RightMiddle"), TEXT("middle_01_r"), TEXT("middle_03_r") },
		{ TEXT("RightRing"), TEXT("ring_01_r"), TEXT("ring_03_r") },
		{ TEXT("RightPinky"), TEXT("pinky_01_r"), TEXT("pinky_03_r") },
		{ TEXT("LeftLeg"), TEXT("thigh_l"), TEXT("ball_l") },
		{ TEXT("RightLeg"), TEXT("thigh_r"), TEXT("ball_r") }
	};

	const TArray<FName> MakoLimbBonesToAutoAlign =
	{
		TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
		TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
		TEXT("thumb_01_l"), TEXT("thumb_02_l"), TEXT("thumb_03_l"),
		TEXT("index_01_l"), TEXT("index_02_l"), TEXT("index_03_l"),
		TEXT("middle_01_l"), TEXT("middle_02_l"), TEXT("middle_03_l"),
		TEXT("ring_01_l"), TEXT("ring_02_l"), TEXT("ring_03_l"),
		TEXT("pinky_01_l"), TEXT("pinky_02_l"), TEXT("pinky_03_l"),
		TEXT("thumb_01_r"), TEXT("thumb_02_r"), TEXT("thumb_03_r"),
		TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r"),
		TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r"),
		TEXT("ring_01_r"), TEXT("ring_02_r"), TEXT("ring_03_r"),
		TEXT("pinky_01_r"), TEXT("pinky_02_r"), TEXT("pinky_03_r"),
		TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"),
		TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r")
	};

	const TArray<FName> MakoOneToOneChains =
	{
		TEXT("Spine"), TEXT("LeftArm"), TEXT("RightArm"),
		TEXT("LeftThumb"), TEXT("LeftIndex"), TEXT("LeftMiddle"),
		TEXT("LeftRing"), TEXT("LeftPinky"),
		TEXT("RightThumb"), TEXT("RightIndex"), TEXT("RightMiddle"),
		TEXT("RightRing"), TEXT("RightPinky")
	};

	const TArray<FName> MakoTorsoBones =
	{
		TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_05"),
		TEXT("neck_01"), TEXT("head")
	};

	FString GetObjectPath(const FString& FolderPath, const FString& AssetName)
	{
		return FString::Printf(TEXT("%s/%s.%s"), *FolderPath, *AssetName, *AssetName);
	}

	bool IsAllowedProjectAsset(const UObject* Asset)
	{
		return IsValid(Asset)
			&& Asset->GetOutermost()->GetName().StartsWith(AllowedAssetRoot);
	}

	bool ValidateMeshBones(
		const USkeletalMesh& Mesh,
		const TArray<FAIRERetargetChainDefinition>& Chains,
		FString& OutError)
	{
		const FReferenceSkeleton& ReferenceSkeleton = Mesh.GetRefSkeleton();
		if (ReferenceSkeleton.FindBoneIndex(TEXT("root")) == INDEX_NONE
			|| ReferenceSkeleton.FindBoneIndex(TEXT("pelvis")) == INDEX_NONE)
		{
			OutError = FString::Printf(
				TEXT("Mesh %s must contain root and pelvis bones."),
				*Mesh.GetName());
			return false;
		}

		for (const FAIRERetargetChainDefinition& Chain : Chains)
		{
			if (ReferenceSkeleton.FindBoneIndex(Chain.StartBone) == INDEX_NONE
				|| ReferenceSkeleton.FindBoneIndex(Chain.EndBone) == INDEX_NONE)
			{
				OutError = FString::Printf(
					TEXT("Mesh %s is missing chain %s bones %s -> %s."),
					*Mesh.GetName(),
					*Chain.ChainName.ToString(),
					*Chain.StartBone.ToString(),
					*Chain.EndBone.ToString());
				return false;
			}
		}
		return true;
	}

	UIKRigDefinition* LoadOrCreateIKRig(
		const FString& AssetName,
		TArray<FString>& OutEntries)
	{
		if (UIKRigDefinition* Existing = LoadObject<UIKRigDefinition>(
			nullptr,
			*GetObjectPath(SetupAssetFolder, AssetName)))
		{
			OutEntries.Add(FString::Printf(TEXT("Reused %s."), *Existing->GetPathName()));
			return Existing;
		}

		UIKRigDefinition* Created =
			UIKRigDefinitionFactory::CreateNewIKRigAsset(SetupAssetFolder, AssetName);
		if (IsValid(Created))
		{
			OutEntries.Add(FString::Printf(TEXT("Created %s."), *Created->GetPathName()));
		}
		return Created;
	}

	UIKRetargeter* LoadOrCreateRetargeter(TArray<FString>& OutEntries)
	{
		if (UIKRetargeter* Existing = LoadObject<UIKRetargeter>(
			nullptr,
			*GetObjectPath(SetupAssetFolder, RetargeterName)))
		{
			OutEntries.Add(FString::Printf(TEXT("Reused %s."), *Existing->GetPathName()));
			return Existing;
		}

		const FAssetToolsModule& AssetToolsModule =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UIKRetargetFactory* Factory = NewObject<UIKRetargetFactory>();
		UIKRetargeter* Created = Cast<UIKRetargeter>(
			AssetToolsModule.Get().CreateAsset(
				RetargeterName,
				SetupAssetFolder,
				UIKRetargeter::StaticClass(),
				Factory));
		if (IsValid(Created))
		{
			OutEntries.Add(FString::Printf(TEXT("Created %s."), *Created->GetPathName()));
		}
		return Created;
	}

	bool ConfigureIKRig(
		UIKRigDefinition& IKRig,
		USkeletalMesh& Mesh,
		const TArray<FAIRERetargetChainDefinition>& Chains,
		TArray<FString>& OutEntries)
	{
		UIKRigController* Controller = UIKRigController::GetController(&IKRig);
		if (!IsValid(Controller) || !Controller->SetSkeletalMesh(&Mesh))
		{
			OutEntries.Add(FString::Printf(
				TEXT("Failed to assign mesh %s to %s."),
				*Mesh.GetName(),
				*IKRig.GetName()));
			return false;
		}

		TArray<FName> ExistingChainNames;
		for (const FBoneChain& ExistingChain : Controller->GetRetargetChains())
		{
			ExistingChainNames.Add(ExistingChain.ChainName);
		}
		for (const FName ExistingChainName : ExistingChainNames)
		{
			Controller->RemoveRetargetChain(ExistingChainName);
		}

		if (!Controller->SetRetargetRoot(TEXT("pelvis"))
			|| !Controller->SetRootMotionBone(TEXT("root")))
		{
			OutEntries.Add(FString::Printf(
				TEXT("Failed to set pelvis/root on %s."),
				*IKRig.GetName()));
			return false;
		}

		for (const FAIRERetargetChainDefinition& Chain : Chains)
		{
			const FName AddedName = Controller->AddRetargetChain(
				Chain.ChainName,
				Chain.StartBone,
				Chain.EndBone,
				NAME_None);
			if (AddedName != Chain.ChainName)
			{
				OutEntries.Add(FString::Printf(
					TEXT("Failed to add exact chain %s to %s."),
					*Chain.ChainName.ToString(),
					*IKRig.GetName()));
				return false;
			}
		}

		IKRig.MarkPackageDirty();
		OutEntries.Add(FString::Printf(
			TEXT("Configured %s with %d exact chains."),
			*IKRig.GetName(),
			Chains.Num()));
		return true;
	}

	void AppendRigInspection(
		const TCHAR* Label,
		const UIKRigDefinition& IKRig,
		TArray<FString>& OutEntries)
	{
		const UIKRigController* Controller = UIKRigController::GetController(&IKRig);
		OutEntries.Add(FString::Printf(
			TEXT("%s mesh=%s pelvis=%s rootMotion=%s"),
			Label,
			*GetNameSafe(Controller->GetSkeletalMesh()),
			*Controller->GetRetargetRoot().ToString(),
			*Controller->GetRootMotionBone().ToString()));
		for (const FBoneChain& Chain : Controller->GetRetargetChains())
		{
			OutEntries.Add(FString::Printf(
				TEXT("%s chain %s: %s -> %s"),
				Label,
				*Chain.ChainName.ToString(),
				*Chain.StartBone.BoneName.ToString(),
				*Chain.EndBone.BoneName.ToString()));
		}
	}

	FAIREIKRetargetSetupResult MakeSetupFailure(const FString& Message)
	{
		FAIREIKRetargetSetupResult Result;
		Result.Message = Message;
		return Result;
	}

	FAIREMakoWeaponSocketSetupResult MakeWeaponSocketFailure(const FString& Message)
	{
		FAIREMakoWeaponSocketSetupResult Result;
		Result.Message = Message;
		return Result;
	}

	bool CreateOrUpdateSocketFromSourceBone(
		USkeletalMesh& FrankMesh,
		USkeletalMesh& MakoMesh,
		const FName SourceBoneName,
		const FName TargetBoneName,
		const FName SocketName,
		TArray<FString>& OutEntries)
	{
		const FReferenceSkeleton& SourceSkeleton = FrankMesh.GetRefSkeleton();
		const int32 SourceBoneIndex = SourceSkeleton.FindBoneIndex(SourceBoneName);
		if (SourceBoneIndex == INDEX_NONE
			|| MakoMesh.GetRefSkeleton().FindBoneIndex(TargetBoneName) == INDEX_NONE)
		{
			return false;
		}

		TArray<TObjectPtr<USkeletalMeshSocket>>& Sockets =
			MakoMesh.GetMeshOnlySocketList();
		USkeletalMeshSocket* Socket = nullptr;
		for (USkeletalMeshSocket* Candidate : Sockets)
		{
			if (IsValid(Candidate) && Candidate->SocketName == SocketName)
			{
				Socket = Candidate;
				break;
			}
		}
		if (!IsValid(Socket))
		{
			Socket = NewObject<USkeletalMeshSocket>(&MakoMesh);
			Sockets.Add(Socket);
		}

		Socket->Modify();
		Socket->SocketName = SocketName;
		Socket->BoneName = TargetBoneName;
		Socket->SetSocketLocalTransform(SourceSkeleton.GetRefBonePose()[SourceBoneIndex]);

		const FTransform SocketTransform = Socket->GetSocketLocalTransform();
		const FRotator SocketRotation = SocketTransform.Rotator();
		OutEntries.Add(FString::Printf(
			TEXT("Socket %s parent=%s source=%s location=(%.3f, %.3f, %.3f) rotation=(P=%.3f, Y=%.3f, R=%.3f)"),
			*SocketName.ToString(),
			*TargetBoneName.ToString(),
			*SourceBoneName.ToString(),
			SocketTransform.GetTranslation().X,
			SocketTransform.GetTranslation().Y,
			SocketTransform.GetTranslation().Z,
			SocketRotation.Pitch,
			SocketRotation.Yaw,
			SocketRotation.Roll));
		return true;
	}
}

FAIREIKRetargetSetupResult UAIREIKRetargetMCPToolset::CreateOrUpdateFrankToMakoRetargetSetup(
	USkeletalMesh* FrankMesh,
	USkeletalMesh* MakoMesh)
{
	if (!IsAllowedProjectAsset(FrankMesh) || !IsAllowedProjectAsset(MakoMesh))
	{
		return MakeSetupFailure(
			TEXT("FrankMesh and MakoMesh must be project assets under /Game/Work/LMK/."));
	}

	FString Error;
	if (!ValidateMeshBones(*FrankMesh, FrankChains, Error)
		|| !ValidateMeshBones(*MakoMesh, MakoChains, Error))
	{
		return MakeSetupFailure(Error);
	}

	const FScopedTransaction Transaction(
		LOCTEXT("CreateOrUpdateFrankToMakoRetargetSetup", "Create or Update Frank to MAKO Retarget Setup"));

	FAIREIKRetargetSetupResult Result;
	Result.SourceIKRig = LoadOrCreateIKRig(SourceIKRigName, Result.Entries);
	Result.TargetIKRig = LoadOrCreateIKRig(TargetIKRigName, Result.Entries);
	Result.Retargeter = LoadOrCreateRetargeter(Result.Entries);
	if (!IsValid(Result.SourceIKRig)
		|| !IsValid(Result.TargetIKRig)
		|| !IsValid(Result.Retargeter))
	{
		Result.Message = TEXT("Failed to create one or more retarget setup assets.");
		return Result;
	}

	if (!ConfigureIKRig(*Result.SourceIKRig, *FrankMesh, FrankChains, Result.Entries)
		|| !ConfigureIKRig(*Result.TargetIKRig, *MakoMesh, MakoChains, Result.Entries))
	{
		Result.Message = TEXT("Failed to configure one or more IK Rig assets.");
		return Result;
	}

	UIKRetargeterController* Controller =
		UIKRetargeterController::GetController(Result.Retargeter);
	if (!IsValid(Controller))
	{
		Result.Message = TEXT("Failed to acquire the IK Retargeter controller.");
		return Result;
	}

	{
		FScopedReinitializeIKRetargeter Reinitialize(
			Controller,
			ERetargetRefreshMode::ProcessorAndOpStack);
		Controller->SetIKRig(ERetargetSourceOrTarget::Source, Result.SourceIKRig);
		Controller->SetIKRig(ERetargetSourceOrTarget::Target, Result.TargetIKRig);
		Controller->SetPreviewMesh(ERetargetSourceOrTarget::Source, FrankMesh);
		Controller->SetPreviewMesh(ERetargetSourceOrTarget::Target, MakoMesh);
		Controller->RemoveAllOps();
		Controller->AddDefaultOps();

		FIKRetargetFKChainsOp* FKChainOp =
			Controller->GetFirstRetargetOpOfType<FIKRetargetFKChainsOp>();
		if (!FKChainOp)
		{
			Result.Message = TEXT("The retargeter did not create an FK Chains operation.");
			return Result;
		}
		FKChainOp->Settings.ChainMapping.AutoMapChains(EAutoMapChainType::Exact, true);
		for (FRetargetFKChainSettings& ChainSettings :
			FKChainOp->Settings.ChainsToRetarget)
		{
			ChainSettings.RotationMode = MakoOneToOneChains.Contains(
				ChainSettings.TargetChainName)
				? EFKChainRotationMode::OneToOne
				: EFKChainRotationMode::Interpolated;
			ChainSettings.RotationAlpha = 1.0;
			ChainSettings.TranslationMode = EFKChainTranslationMode::None;
			ChainSettings.TranslationAlpha = 1.0;
		}

		const FName TargetPoseName = Controller->GetCurrentRetargetPoseName(
			ERetargetSourceOrTarget::Target);
		Controller->ResetRetargetPose(
			TargetPoseName,
			TArray<FName>(),
			ERetargetSourceOrTarget::Target);
		Controller->AutoAlignBones(
			MakoLimbBonesToAutoAlign,
			ERetargetAutoAlignMethod::ChainToChain,
			ERetargetSourceOrTarget::Target);
		Controller->ResetRetargetPose(
			TargetPoseName,
			MakoTorsoBones,
			ERetargetSourceOrTarget::Target);

		if (FIKRetargetRunIKRigOp* RunIKOp =
			Controller->GetFirstRetargetOpOfType<FIKRetargetRunIKRigOp>())
		{
			const int32 RunIKIndex = Controller->GetIndexOfRetargetOp(RunIKOp);
			Controller->SetRetargetOpEnabled(RunIKIndex, false);
		}
	}

	Result.Retargeter->MarkPackageDirty();
	Result.bSuccess = true;
	Result.Message =
		TEXT("Created the Frank-to-MAKO retarget setup with torso pose offsets reset, limb and finger auto alignment, and one-to-one spine, arm, and finger rotation. Assets are dirty and not saved.");
	return Result;
}

FAIREMakoWeaponSocketSetupResult UAIREIKRetargetMCPToolset::CreateOrUpdateMakoWeaponSockets(
	USkeletalMesh* FrankMesh,
	USkeletalMesh* MakoMesh)
{
	if (!IsAllowedProjectAsset(FrankMesh) || !IsAllowedProjectAsset(MakoMesh))
	{
		return MakeWeaponSocketFailure(
			TEXT("FrankMesh and MakoMesh must be project assets under /Game/Work/LMK/."));
	}
	const FReferenceSkeleton& FrankReferenceSkeleton = FrankMesh->GetRefSkeleton();
	const FReferenceSkeleton& MakoReferenceSkeleton = MakoMesh->GetRefSkeleton();
	if (FrankReferenceSkeleton.FindBoneIndex(TEXT("R_Hand_Weapon001")) == INDEX_NONE
		|| FrankReferenceSkeleton.FindBoneIndex(TEXT("R_Hand_Weapon")) == INDEX_NONE
		|| MakoReferenceSkeleton.FindBoneIndex(TEXT("hand_l")) == INDEX_NONE
		|| MakoReferenceSkeleton.FindBoneIndex(TEXT("hand_r")) == INDEX_NONE)
	{
		return MakeWeaponSocketFailure(
			TEXT("Frank weapon handle bones or MAKO hand bones are missing."));
	}

	const FScopedTransaction Transaction(
		LOCTEXT("CreateOrUpdateMakoWeaponSockets", "Create or Update MAKO Weapon Sockets"));
	MakoMesh->Modify();

	FAIREMakoWeaponSocketSetupResult Result;
	Result.MakoMesh = MakoMesh;
	const bool bLeftCreated = CreateOrUpdateSocketFromSourceBone(
		*FrankMesh,
		*MakoMesh,
		TEXT("R_Hand_Weapon001"),
		TEXT("hand_l"),
		TEXT("weapon_l"),
		Result.Entries);
	const bool bRightCreated = CreateOrUpdateSocketFromSourceBone(
		*FrankMesh,
		*MakoMesh,
		TEXT("R_Hand_Weapon"),
		TEXT("hand_r"),
		TEXT("weapon_r"),
		Result.Entries);
	if (!bLeftCreated || !bRightCreated)
	{
		Result.Message =
			TEXT("Frank weapon handle bones or MAKO hand bones are missing.");
		return Result;
	}

	MakoMesh->MarkPackageDirty();
	Result.bSuccess = true;
	Result.Message =
		TEXT("Created or updated MAKO weapon_l and weapon_r mesh sockets from Frank weapon handle reference transforms. Asset is dirty and not saved.");
	return Result;
}

FAIREIKRetargetSetupResult UAIREIKRetargetMCPToolset::InspectFrankToMakoRetargetSetup(
	UIKRetargeter* Retargeter)
{
	if (!IsAllowedProjectAsset(Retargeter))
	{
		return MakeSetupFailure(
			TEXT("Retargeter must be a project asset under /Game/Work/LMK/."));
	}

	UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	if (!IsValid(Controller))
	{
		return MakeSetupFailure(TEXT("Failed to acquire the IK Retargeter controller."));
	}

	FAIREIKRetargetSetupResult Result;
	Result.Retargeter = Retargeter;
	Result.SourceIKRig = const_cast<UIKRigDefinition*>(
		Controller->GetIKRig(ERetargetSourceOrTarget::Source));
	Result.TargetIKRig = const_cast<UIKRigDefinition*>(
		Controller->GetIKRig(ERetargetSourceOrTarget::Target));
	if (!IsValid(Result.SourceIKRig) || !IsValid(Result.TargetIKRig))
	{
		Result.Message = TEXT("The retargeter is missing a source or target IK Rig.");
		return Result;
	}

	AppendRigInspection(TEXT("Source"), *Result.SourceIKRig, Result.Entries);
	AppendRigInspection(TEXT("Target"), *Result.TargetIKRig, Result.Entries);
	Result.Entries.Add(FString::Printf(
		TEXT("Target pose=%s"),
		*Controller->GetCurrentRetargetPoseName(
			ERetargetSourceOrTarget::Target).ToString()));
	for (const FName BoneName : MakoTorsoBones)
	{
		const FRotator Offset = Controller->GetRotationOffsetForRetargetPoseBone(
			BoneName,
			ERetargetSourceOrTarget::Target).Rotator();
		Result.Entries.Add(FString::Printf(
			TEXT("Target pose %s offset P=%.3f Y=%.3f R=%.3f"),
			*BoneName.ToString(),
			Offset.Pitch,
			Offset.Yaw,
			Offset.Roll));
	}

	if (const FIKRetargetFKChainsOp* FKChainOp =
		Controller->GetFirstRetargetOpOfType<FIKRetargetFKChainsOp>())
	{
		for (const FRetargetFKChainSettings& Settings :
			FKChainOp->Settings.ChainsToRetarget)
		{
			Result.Entries.Add(FString::Printf(
				TEXT("FK %s rotation=%d alpha=%.3f translation=%d"),
				*Settings.TargetChainName.ToString(),
				static_cast<int32>(Settings.RotationMode),
				Settings.RotationAlpha,
				static_cast<int32>(Settings.TranslationMode)));
		}
	}

	Result.bSuccess = true;
	Result.Message = TEXT("Inspected the Frank-to-MAKO retarget setup.");
	return Result;
}

FAIREIKRetargetSetupResult UAIREIKRetargetMCPToolset::SetMakoTargetRetargetPose(
	UIKRetargeter* Retargeter,
	const TArray<FAIRERetargetPoseBoneRotation>& BoneRotations,
	const bool bResetExistingPose)
{
	if (!IsAllowedProjectAsset(Retargeter))
	{
		return MakeSetupFailure(
			TEXT("Retargeter must be a project asset under /Game/Work/LMK/."));
	}

	UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	if (!IsValid(Controller))
	{
		return MakeSetupFailure(TEXT("Failed to acquire the IK Retargeter controller."));
	}

	UIKRigDefinition* TargetIKRig = const_cast<UIKRigDefinition*>(
		Controller->GetIKRig(ERetargetSourceOrTarget::Target));
	USkeletalMesh* TargetPreviewMesh = Controller->GetPreviewMesh(
		ERetargetSourceOrTarget::Target);
	if (!IsValid(TargetIKRig) || !IsValid(TargetPreviewMesh))
	{
		return MakeSetupFailure(
			TEXT("The retargeter has no valid target IK Rig or target preview mesh."));
	}

	const FReferenceSkeleton& ReferenceSkeleton =
		TargetPreviewMesh->GetRefSkeleton();
	for (const FAIRERetargetPoseBoneRotation& BoneRotation : BoneRotations)
	{
		if (BoneRotation.BoneName.IsNone()
			|| ReferenceSkeleton.FindBoneIndex(BoneRotation.BoneName) == INDEX_NONE)
		{
			return MakeSetupFailure(FString::Printf(
				TEXT("Target bone %s does not exist."),
				*BoneRotation.BoneName.ToString()));
		}
	}

	const FScopedTransaction Transaction(
		LOCTEXT("SetMakoTargetRetargetPose", "Set MAKO Target Retarget Pose"));
	const FName TargetPoseName = Controller->GetCurrentRetargetPoseName(
		ERetargetSourceOrTarget::Target);
	if (bResetExistingPose)
	{
		Controller->ResetRetargetPose(
			TargetPoseName,
			TArray<FName>(),
			ERetargetSourceOrTarget::Target);
	}
	for (const FAIRERetargetPoseBoneRotation& BoneRotation : BoneRotations)
	{
		Controller->SetRotationOffsetForRetargetPoseBone(
			BoneRotation.BoneName,
			BoneRotation.RotationOffset.Quaternion(),
			ERetargetSourceOrTarget::Target);
	}
	Retargeter->MarkPackageDirty();

	FAIREIKRetargetSetupResult Result = InspectFrankToMakoRetargetSetup(Retargeter);
	Result.Message = FString::Printf(
		TEXT("Applied %d target retarget-pose bone rotations. Asset is dirty and not saved."),
		BoneRotations.Num());
	return Result;
}

FAIREIKRetargetSetupResult UAIREIKRetargetMCPToolset::AutoAlignMakoTargetRetargetPoseBones(
	UIKRetargeter* Retargeter,
	const TArray<FName>& BoneNames,
	const EAIRERetargetAutoAlignMethod AlignmentMethod,
	const bool bResetBones)
{
	if (!IsAllowedProjectAsset(Retargeter) || BoneNames.IsEmpty())
	{
		return MakeSetupFailure(
			TEXT("A project retargeter and at least one target bone name are required."));
	}

	UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	if (!IsValid(Controller))
	{
		return MakeSetupFailure(TEXT("Failed to acquire the IK Retargeter controller."));
	}

	USkeletalMesh* TargetPreviewMesh = Controller->GetPreviewMesh(
		ERetargetSourceOrTarget::Target);
	if (!IsValid(TargetPreviewMesh))
	{
		return MakeSetupFailure(TEXT("The retargeter has no valid target preview mesh."));
	}

	const FReferenceSkeleton& ReferenceSkeleton = TargetPreviewMesh->GetRefSkeleton();
	for (const FName BoneName : BoneNames)
	{
		if (BoneName.IsNone() || ReferenceSkeleton.FindBoneIndex(BoneName) == INDEX_NONE)
		{
			return MakeSetupFailure(FString::Printf(
				TEXT("Target bone %s does not exist."),
				*BoneName.ToString()));
		}
	}

	ERetargetAutoAlignMethod EngineAlignmentMethod =
		ERetargetAutoAlignMethod::ChainToChain;
	switch (AlignmentMethod)
	{
	case EAIRERetargetAutoAlignMethod::Mesh:
		EngineAlignmentMethod = ERetargetAutoAlignMethod::MeshToMesh;
		break;
	case EAIRERetargetAutoAlignMethod::LocalRotationAxes:
		EngineAlignmentMethod = ERetargetAutoAlignMethod::LocalRotationAxes;
		break;
	case EAIRERetargetAutoAlignMethod::GlobalRotationAxes:
		EngineAlignmentMethod = ERetargetAutoAlignMethod::GlobalRotationAxes;
		break;
	case EAIRERetargetAutoAlignMethod::Direction:
	default:
		break;
	}

	const FScopedTransaction Transaction(
		LOCTEXT("AutoAlignMakoTargetRetargetPoseBones", "Auto Align MAKO Target Retarget Pose Bones"));
	Retargeter->Modify();
	const FName TargetPoseName = Controller->GetCurrentRetargetPoseName(
		ERetargetSourceOrTarget::Target);
	if (bResetBones)
	{
		Controller->ResetRetargetPose(
			TargetPoseName,
			BoneNames,
			ERetargetSourceOrTarget::Target);
	}
	Controller->AutoAlignBones(
		BoneNames,
		EngineAlignmentMethod,
		ERetargetSourceOrTarget::Target);
	Retargeter->MarkPackageDirty();

	FAIREIKRetargetSetupResult Result = InspectFrankToMakoRetargetSetup(Retargeter);
	for (const FName BoneName : BoneNames)
	{
		const FRotator Offset = Controller->GetRotationOffsetForRetargetPoseBone(
			BoneName,
			ERetargetSourceOrTarget::Target).Rotator();
		Result.Entries.Add(FString::Printf(
			TEXT("Auto-aligned %s offset P=%.3f Y=%.3f R=%.3f"),
			*BoneName.ToString(),
			Offset.Pitch,
			Offset.Yaw,
			Offset.Roll));
	}
	Result.Message = FString::Printf(
		TEXT("Auto-aligned %d target retarget-pose bones with method %d. Asset is dirty and not saved."),
		BoneNames.Num(),
		static_cast<int32>(EngineAlignmentMethod));
	return Result;
}

FAIREIKRetargetSetupResult UAIREIKRetargetMCPToolset::SetFrankToMakoFKChainSettings(
	UIKRetargeter* Retargeter,
	const FName TargetChainName,
	const EAIRERetargetChainRotationMode RotationMode,
	const float RotationAlpha)
{
	if (!IsAllowedProjectAsset(Retargeter) || TargetChainName.IsNone())
	{
		return MakeSetupFailure(
			TEXT("A project retargeter and target chain name are required."));
	}

	UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	if (!IsValid(Controller))
	{
		return MakeSetupFailure(TEXT("Failed to acquire the IK Retargeter controller."));
	}

	FIKRetargetFKChainsOp* FKChainOp =
		Controller->GetFirstRetargetOpOfType<FIKRetargetFKChainsOp>();
	if (!FKChainOp)
	{
		return MakeSetupFailure(TEXT("The retargeter has no FK Chains operation."));
	}

	FRetargetFKChainSettings* ChainSettings =
		FKChainOp->Settings.ChainsToRetarget.FindByPredicate(
			[TargetChainName](const FRetargetFKChainSettings& Settings)
			{
				return Settings.TargetChainName == TargetChainName;
			});
	if (!ChainSettings)
	{
		return MakeSetupFailure(FString::Printf(
			TEXT("Target FK chain %s does not exist."),
			*TargetChainName.ToString()));
	}

	EFKChainRotationMode EngineRotationMode = EFKChainRotationMode::Interpolated;
	switch (RotationMode)
	{
	case EAIRERetargetChainRotationMode::OneToOne:
		EngineRotationMode = EFKChainRotationMode::OneToOne;
		break;
	case EAIRERetargetChainRotationMode::OneToOneReversed:
		EngineRotationMode = EFKChainRotationMode::OneToOneReversed;
		break;
	case EAIRERetargetChainRotationMode::None:
		EngineRotationMode = EFKChainRotationMode::None;
		break;
	case EAIRERetargetChainRotationMode::Interpolated:
	default:
		break;
	}

	const FScopedTransaction Transaction(
		LOCTEXT("SetFrankToMakoFKChainSettings", "Set Frank to MAKO FK Chain Settings"));
	{
		FScopedReinitializeIKRetargeter Reinitialize(
			Controller,
			ERetargetRefreshMode::ProcessorAndOpStack);
		Retargeter->Modify();
		ChainSettings->RotationMode = EngineRotationMode;
		ChainSettings->RotationAlpha = FMath::Clamp(RotationAlpha, 0.0f, 1.0f);
	}
	Retargeter->MarkPackageDirty();

	FAIREIKRetargetSetupResult Result = InspectFrankToMakoRetargetSetup(Retargeter);
	Result.Message = FString::Printf(
		TEXT("Set FK chain %s rotation mode to %d with alpha %.3f. Asset is dirty and not saved."),
		*TargetChainName.ToString(),
		static_cast<int32>(EngineRotationMode),
		FMath::Clamp(RotationAlpha, 0.0f, 1.0f));
	return Result;
}

FAIRERetargetBatchResult UAIREIKRetargetMCPToolset::RetargetFrankAnimationsToMako(
	UIKRetargeter* Retargeter,
	USkeletalMesh* FrankMesh,
	USkeletalMesh* MakoMesh,
	const TArray<UAnimationAsset*>& Animations,
	const FString& Prefix)
{
	FAIRERetargetBatchResult Result;
	if (!IsAllowedProjectAsset(Retargeter)
		|| !IsAllowedProjectAsset(FrankMesh)
		|| !IsAllowedProjectAsset(MakoMesh)
		|| Animations.IsEmpty()
		|| Prefix.IsEmpty())
	{
		Result.Message =
			TEXT("A project retargeter, Frank mesh, MAKO mesh, animations, and a non-empty prefix are required.");
		return Result;
	}

	FIKRetargetBatchOperationInputs Inputs;
	Inputs.SourceMesh = FrankMesh;
	Inputs.TargetMesh = MakoMesh;
	Inputs.IKRetargetAsset = Retargeter;
	Inputs.Prefix = Prefix;
	Inputs.TargetPath = FixedAnimationFolder;
	Inputs.bUseSourcePath = false;
	Inputs.bIncludeReferencedAssets = false;
	Inputs.bOverwriteExistingFiles = false;
	Inputs.bRetainAdditiveFlags = true;
	for (UAnimationAsset* Animation : Animations)
	{
		if (!IsAllowedProjectAsset(Animation)
			|| Animation->GetSkeleton() != FrankMesh->GetSkeleton())
		{
			Result.Message = FString::Printf(
				TEXT("Animation %s is outside the project root or does not use the Frank skeleton."),
				*GetNameSafe(Animation));
			return Result;
		}
		Inputs.AssetsToRetarget.Add(FAssetData(Animation));
	}

	const TArray<FAssetData> CreatedAssets =
		UIKRetargetBatchOperation::RunBatchRetarget(Inputs);
	for (const FAssetData& CreatedAssetData : CreatedAssets)
	{
		if (UObject* CreatedAsset = CreatedAssetData.GetAsset())
		{
			Result.CreatedAssets.Add(CreatedAsset);
			Result.Entries.Add(CreatedAsset->GetPathName());
		}
	}

	Result.bSuccess = Result.CreatedAssets.Num() == Animations.Num();
	Result.Message = Result.bSuccess
		? FString::Printf(
			TEXT("Retargeted %d animations into %s. Inspect before saving."),
			Result.CreatedAssets.Num(),
			*FixedAnimationFolder)
		: FString::Printf(
			TEXT("Requested %d animations but created %d assets. Inspect the Output Log."),
			Animations.Num(),
			Result.CreatedAssets.Num());
	return Result;
}

#undef LOCTEXT_NAMESPACE

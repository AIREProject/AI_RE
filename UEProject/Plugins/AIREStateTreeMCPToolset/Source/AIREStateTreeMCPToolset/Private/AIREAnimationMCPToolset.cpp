#include "AIREAnimationMCPToolset.h"

#include "Animation/AIRECompanionAttackHitAnimNotify.h"
#include "Animation/AIRECompanionComboWindowAnimNotifyState.h"
#include "Animation/AIRECompanionMeleeTraceAnimNotifyState.h"
#include "AIREEnemyAttackMovementAnimNotifyState.h"
#include "AIREEnemyMeleeTraceAnimNotifyState.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "AnimationBlueprintLibrary.h"
#include "ControlRigBlueprintLegacy.h"
#include "Engine/SkeletalMesh.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AIREAnimationMCPToolset"

namespace
{
	const FString AllowedAnimationAssetRoot = TEXT("/Game/Work/LMK/");

	struct FAIREComboNotifyEntry
	{
		int32 StepIndex = INDEX_NONE;
		float StartTime = 0.0f;
		float Duration = 0.0f;
	};

	FAIREAnimationComboMontageResult MakeAnimationFailure(
		const FString& Message)
	{
		FAIREAnimationComboMontageResult Result;
		Result.Message = Message;
		return Result;
	}

	FAIREAnimationNotifyMutationResult MakeNotifyMutationFailure(
		const FString& Message)
	{
		FAIREAnimationNotifyMutationResult Result;
		Result.Message = Message;
		return Result;
	}

	FAIREAnimationMontageTrackResult MakeMontageTrackFailure(
		const FString& Message)
	{
		FAIREAnimationMontageTrackResult Result;
		Result.Message = Message;
		return Result;
	}

	FAIREAnimationBoneMotionResult MakeBoneMotionFailure(
		const FString& Message)
	{
		FAIREAnimationBoneMotionResult Result;
		Result.Message = Message;
		return Result;
	}

	FAIREControlRigHierarchySyncResult MakeControlRigHierarchyFailure(
		const FString& Message)
	{
		FAIREControlRigHierarchySyncResult Result;
		Result.Message = Message;
		return Result;
	}

	FAIREControlRigVMResult MakeControlRigVMFailure(const FString& Message)
	{
		FAIREControlRigVMResult Result;
		Result.Message = Message;
		return Result;
	}

	bool IsAllowedAnimationAsset(const UObject* Asset)
	{
		return IsValid(Asset) &&
			Asset->GetOutermost()->GetName().StartsWith(AllowedAnimationAssetRoot);
	}

	void GatherHierarchyDiscrepancies(
		const URigHierarchy& Hierarchy,
		const USkeletalMesh& SkeletalMesh,
		TArray<FString>& OutEntries)
	{
		const TArray<FMeshBoneInfo>& BoneInfos =
			SkeletalMesh.GetRefSkeleton().GetRefBoneInfo();
		for (const FMeshBoneInfo& BoneInfo : BoneInfos)
		{
			const FRigElementKey BoneKey(BoneInfo.Name, ERigElementType::Bone);
			if (!Hierarchy.Contains(BoneKey))
			{
				continue;
			}

			const FName RigParentName = Hierarchy.GetFirstParent(BoneKey).Name;
			FName MeshParentName = NAME_None;
			if (BoneInfo.ParentIndex != INDEX_NONE)
			{
				MeshParentName = BoneInfos[BoneInfo.ParentIndex].Name;
			}

			if (RigParentName != MeshParentName)
			{
				OutEntries.Add(
					FString::Printf(
						TEXT("%s: ControlRig parent=%s, SkeletalMesh parent=%s"),
						*BoneInfo.Name.ToString(),
						*RigParentName.ToString(),
						*MeshParentName.ToString()));
			}
		}
	}

	bool ValidateMontage(const UAnimMontage* Montage, FString& OutError)
	{
		if (!IsValid(Montage))
		{
			OutError = TEXT("A valid AnimMontage is required.");
			return false;
		}

		if (!Montage->GetPathName().StartsWith(
			AllowedAnimationAssetRoot))
		{
			OutError = FString::Printf(
				TEXT("Only montages under %s may be edited by this tool."),
				*AllowedAnimationAssetRoot);
			return false;
		}

		if (Montage->GetNumSections() < 1)
		{
			OutError = TEXT("The montage has no sections.");
			return false;
		}

		return true;
	}

	void GatherComboNotifies(
		const UAnimMontage& Montage,
		TArray<FAIREComboNotifyEntry>& OutHitNotifies,
		TArray<FAIREComboNotifyEntry>& OutComboWindows)
	{
		for (const FAnimNotifyEvent& NotifyEvent : Montage.Notifies)
		{
			if (const UAIRECompanionAttackHitAnimNotify* HitNotify =
				Cast<UAIRECompanionAttackHitAnimNotify>(NotifyEvent.Notify))
			{
				OutHitNotifies.Add(
					{HitNotify->ComboStepIndex, NotifyEvent.GetTime(), 0.0f});
			}

			if (const UAIRECompanionComboWindowAnimNotifyState* ComboWindow =
				Cast<UAIRECompanionComboWindowAnimNotifyState>(NotifyEvent.NotifyStateClass))
			{
				OutComboWindows.Add(
					{
						ComboWindow->ComboStepIndex,
						NotifyEvent.GetTime(),
						NotifyEvent.GetDuration()
					});
			}
		}

		OutHitNotifies.Sort(
			[](const FAIREComboNotifyEntry& Left, const FAIREComboNotifyEntry& Right)
			{
				return Left.StepIndex < Right.StepIndex;
			});
		OutComboWindows.Sort(
			[](const FAIREComboNotifyEntry& Left, const FAIREComboNotifyEntry& Right)
			{
				return Left.StepIndex < Right.StepIndex;
			});
	}

	void PopulateInspection(
		const UAnimMontage& Montage,
		FAIREAnimationComboMontageResult& OutResult)
	{
		TArray<FAIREComboNotifyEntry> HitNotifies;
		TArray<FAIREComboNotifyEntry> ComboWindows;
		GatherComboNotifies(Montage, HitNotifies, ComboWindows);

		OutResult.SectionCount = Montage.GetNumSections();
		OutResult.HitNotifyCount = HitNotifies.Num();
		OutResult.ComboWindowCount = ComboWindows.Num();

		for (int32 SectionIndex = 0; SectionIndex < Montage.GetNumSections(); ++SectionIndex)
		{
			float StartTime = 0.0f;
			float EndTime = 0.0f;
			Montage.GetSectionStartAndEndTime(SectionIndex, StartTime, EndTime);
			OutResult.Entries.Add(
				FString::Printf(
					TEXT("Section[%d] %s: %.3f-%.3f"),
					SectionIndex,
					*Montage.GetSectionName(SectionIndex).ToString(),
					StartTime,
					EndTime));
		}

		for (const FAnimSyncMarker& Marker : Montage.MarkerData.AuthoredSyncMarkers)
		{
			OutResult.Entries.Add(
				FString::Printf(
					TEXT("Marker %s: %.3f"),
					*Marker.MarkerName.ToString(),
					Marker.Time));
		}

		for (const FAIREComboNotifyEntry& HitNotify : HitNotifies)
		{
			OutResult.Entries.Add(
				FString::Printf(
					TEXT("Hit[%d]: %.3f"),
					HitNotify.StepIndex,
					HitNotify.StartTime));
		}

		for (const FAIREComboNotifyEntry& ComboWindow : ComboWindows)
		{
			OutResult.Entries.Add(
				FString::Printf(
					TEXT("Window[%d]: %.3f-%.3f"),
					ComboWindow.StepIndex,
					ComboWindow.StartTime,
					ComboWindow.StartTime + ComboWindow.Duration));
		}
	}

	FString GetNotifyDisplayName(const FAnimNotifyEvent& NotifyEvent)
	{
		if (IsValid(NotifyEvent.Notify))
		{
			return NotifyEvent.Notify->GetNotifyName();
		}
		if (IsValid(NotifyEvent.NotifyStateClass))
		{
			return NotifyEvent.NotifyStateClass->GetNotifyName();
		}
		return NotifyEvent.NotifyName.ToString();
	}

	void PopulateEnemyMeleeTraceInspection(
		const UAnimMontage& Montage,
		FAIREAnimationNotifyMutationResult& OutResult)
	{
		for (const FAnimNotifyEvent& NotifyEvent : Montage.Notifies)
		{
			const FString NotifyName = GetNotifyDisplayName(NotifyEvent);
			if (NotifyName == TEXT("SaveAttack") || NotifyName == TEXT("ResetCombo"))
			{
				++OutResult.LegacyNotifyCount;
			}

			if (const UAIREEnemyMeleeTraceAnimNotifyState* TraceWindow =
				Cast<UAIREEnemyMeleeTraceAnimNotifyState>(
					NotifyEvent.NotifyStateClass))
			{
				++OutResult.TraceWindowCount;
				OutResult.Entries.Add(
					FString::Printf(
						TEXT("%s[%d] xDamage=%.2f xStagger=%.2f sockets=%s->%s: %.3f-%.3f"),
						*NotifyName,
						TraceWindow->StrikeIndex,
						TraceWindow->DamageScale,
						TraceWindow->StaggerScale,
						*TraceWindow->TraceStartSocket.ToString(),
						*TraceWindow->TraceEndSocket.ToString(),
						NotifyEvent.GetTime(),
						NotifyEvent.GetTime() + NotifyEvent.GetDuration()));
				continue;
			}

			if (IsValid(Cast<UAIREEnemyAttackMovementAnimNotifyState>(
				NotifyEvent.NotifyStateClass)))
			{
				++OutResult.AttackMovementWindowCount;
				OutResult.Entries.Add(
					FString::Printf(
						TEXT("%s: %.3f-%.3f"),
						*NotifyName,
						NotifyEvent.GetTime(),
						NotifyEvent.GetTime() + NotifyEvent.GetDuration()));
				continue;
			}

			OutResult.Entries.Add(
				FString::Printf(
					TEXT("%s: %.3f-%.3f"),
					*NotifyName,
					NotifyEvent.GetTime(),
					NotifyEvent.GetTime() + NotifyEvent.GetDuration()));
		}
	}

	void PopulateCompanionMeleeTraceInspection(
		const UAnimMontage& Montage,
		FAIREAnimationNotifyMutationResult& OutResult)
	{
		for (const FAnimNotifyEvent& NotifyEvent : Montage.Notifies)
		{
			const UAIRECompanionMeleeTraceAnimNotifyState* TraceWindow =
				Cast<UAIRECompanionMeleeTraceAnimNotifyState>(
					NotifyEvent.NotifyStateClass);
			if (!IsValid(TraceWindow))
			{
				continue;
			}

			++OutResult.TraceWindowCount;
			OutResult.Entries.Add(
				FString::Printf(
					TEXT("%s[%d]: %.3f-%.3f"),
					TraceWindow->TraceMode ==
							EAIRECompanionMeleeTraceMode::CombatSkill
						? TEXT("CombatSkill")
						: TEXT("BasicAttack"),
					TraceWindow->ComboStepIndex,
					NotifyEvent.GetTime(),
					NotifyEvent.GetTime() + NotifyEvent.GetDuration()));
		}
	}

	bool ValidateComboLayout(
		const UAnimMontage& Montage,
		const TArray<FAIREComboNotifyEntry>& HitNotifies,
		const TArray<FAIREComboNotifyEntry>& ComboWindows,
		FString& OutError)
	{
		const int32 SectionCount = Montage.GetNumSections();
		if (HitNotifies.Num() != SectionCount)
		{
			OutError = FString::Printf(
				TEXT("Expected one hit notify per section (%d), but found %d."),
				SectionCount,
				HitNotifies.Num());
			return false;
		}

		TSet<int32> HitSteps;
		for (const FAIREComboNotifyEntry& HitNotify : HitNotifies)
		{
			if (HitNotify.StepIndex < 0 || HitNotify.StepIndex >= SectionCount)
			{
				OutError = FString::Printf(
					TEXT("Hit notify step %d is outside the section range."),
					HitNotify.StepIndex);
				return false;
			}
			if (HitSteps.Contains(HitNotify.StepIndex))
			{
				OutError = FString::Printf(
					TEXT("Multiple hit notifies use combo step %d."),
					HitNotify.StepIndex);
				return false;
			}

			const int32 HitSectionIndex = Montage.GetSectionIndexFromPosition(HitNotify.StartTime);
			if (HitSectionIndex != HitNotify.StepIndex)
			{
				OutError = FString::Printf(
					TEXT("Hit notify step %d is inside section index %d."),
					HitNotify.StepIndex,
					HitSectionIndex);
				return false;
			}
			HitSteps.Add(HitNotify.StepIndex);
		}

		TSet<int32> WindowSteps;
		for (const FAIREComboNotifyEntry& ComboWindow : ComboWindows)
		{
			if (ComboWindow.StepIndex < 0 || ComboWindow.StepIndex >= SectionCount - 1)
			{
				OutError = FString::Printf(
					TEXT("Combo window step %d does not have a following section."),
					ComboWindow.StepIndex);
				return false;
			}
			if (WindowSteps.Contains(ComboWindow.StepIndex))
			{
				OutError = FString::Printf(
					TEXT("Multiple combo windows use combo step %d."),
					ComboWindow.StepIndex);
				return false;
			}
			WindowSteps.Add(ComboWindow.StepIndex);
		}

		return true;
	}
}

FAIREAnimationComboMontageResult UAIREAnimationMCPToolset::ConfigureBasicAttackComboSectionsFromHits(
	UAnimMontage* Montage,
	const TArray<FName>& SectionNames,
	const float TransitionBias)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeAnimationFailure(Error);
	}
	if (SectionNames.Num() < 2)
	{
		return MakeAnimationFailure(
			TEXT("At least two section names are required."));
	}
	if (!FMath::IsFinite(TransitionBias) || TransitionBias <= 0.0f || TransitionBias >= 1.0f)
	{
		return MakeAnimationFailure(
			TEXT("TransitionBias must be finite and between zero and one."));
	}

	TSet<FName> UniqueSectionNames;
	for (const FName SectionName : SectionNames)
	{
		if (SectionName.IsNone() || UniqueSectionNames.Contains(SectionName))
		{
			return MakeAnimationFailure(
				TEXT("Section names must be non-empty and unique."));
		}
		UniqueSectionNames.Add(SectionName);
	}

	TArray<FAnimNotifyEvent*> HitNotifyEvents;
	for (FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (IsValid(Cast<UAIRECompanionAttackHitAnimNotify>(NotifyEvent.Notify)))
		{
			HitNotifyEvents.Add(&NotifyEvent);
		}
	}
	HitNotifyEvents.Sort(
		[](const FAnimNotifyEvent& Left, const FAnimNotifyEvent& Right)
		{
			return Left.GetTime() < Right.GetTime();
		});

	if (HitNotifyEvents.Num() != SectionNames.Num())
	{
		return MakeAnimationFailure(
			FString::Printf(
				TEXT("Expected one hit notify per requested section (%d), but found %d."),
				SectionNames.Num(),
				HitNotifyEvents.Num()));
	}

	TArray<float> SectionStartTimes;
	SectionStartTimes.Reserve(SectionNames.Num());
	SectionStartTimes.Add(0.0f);
	for (int32 StepIndex = 1; StepIndex < HitNotifyEvents.Num(); ++StepIndex)
	{
		SectionStartTimes.Add(
			FMath::Lerp(
				HitNotifyEvents[StepIndex - 1]->GetTime(),
				HitNotifyEvents[StepIndex]->GetTime(),
				TransitionBias));
	}

	for (int32 StepIndex = 0; StepIndex < HitNotifyEvents.Num(); ++StepIndex)
	{
		const float HitTime = HitNotifyEvents[StepIndex]->GetTime();
		const float SectionStartTime = SectionStartTimes[StepIndex];
		const float SectionEndTime = SectionStartTimes.IsValidIndex(StepIndex + 1)
			? SectionStartTimes[StepIndex + 1]
			: Montage->GetPlayLength();
		if (HitTime < SectionStartTime || HitTime >= SectionEndTime)
		{
			return MakeAnimationFailure(
				FString::Printf(
					TEXT("Chronological hit %d at %.3f is outside derived section %s (%.3f-%.3f)."),
					StepIndex,
					HitTime,
					*SectionNames[StepIndex].ToString(),
					SectionStartTime,
					SectionEndTime));
		}
	}

	bool bSectionsAlreadyMatch = Montage->GetNumSections() == SectionNames.Num();
	for (int32 SectionIndex = 0; bSectionsAlreadyMatch && SectionIndex < SectionNames.Num(); ++SectionIndex)
	{
		float ExistingStartTime = 0.0f;
		float ExistingEndTime = 0.0f;
		Montage->GetSectionStartAndEndTime(SectionIndex, ExistingStartTime, ExistingEndTime);
		bSectionsAlreadyMatch =
			Montage->GetSectionName(SectionIndex) == SectionNames[SectionIndex] &&
			FMath::IsNearlyEqual(ExistingStartTime, SectionStartTimes[SectionIndex], 0.001f);
	}

	const bool bHasReplaceableDefaultSection =
		Montage->GetNumSections() == 1 &&
		Montage->GetSectionName(0) == FName(TEXT("Default"));
	if (!bSectionsAlreadyMatch && !bHasReplaceableDefaultSection)
	{
		return MakeAnimationFailure(
			TEXT("Existing montage sections are neither the requested layout nor a single Default section."));
	}

	const FScopedTransaction Transaction(
		LOCTEXT("ConfigureBasicAttackComboSections", "Configure Basic Attack Combo Sections"));
	Montage->Modify();

	if (!bSectionsAlreadyMatch)
	{
		Montage->CompositeSections.Reset();
		for (int32 SectionIndex = 0; SectionIndex < SectionNames.Num(); ++SectionIndex)
		{
			if (Montage->AddAnimCompositeSection(
				SectionNames[SectionIndex],
				SectionStartTimes[SectionIndex]) == INDEX_NONE)
			{
				return MakeAnimationFailure(
					FString::Printf(
						TEXT("Failed to add montage section %s."),
						*SectionNames[SectionIndex].ToString()));
			}
		}

		for (FCompositeSection& Section : Montage->CompositeSections)
		{
			Section.NextSectionName = NAME_None;
		}
	}

	int32 ReindexedHitCount = 0;
	for (int32 StepIndex = 0; StepIndex < HitNotifyEvents.Num(); ++StepIndex)
	{
		UAIRECompanionAttackHitAnimNotify* HitNotify =
			CastChecked<UAIRECompanionAttackHitAnimNotify>(HitNotifyEvents[StepIndex]->Notify);
		if (HitNotify->ComboStepIndex != StepIndex)
		{
			HitNotify->Modify();
			HitNotify->ComboStepIndex = StepIndex;
			++ReindexedHitCount;
		}
	}

	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();

	FAIREAnimationComboMontageResult Result;
	PopulateInspection(*Montage, Result);
	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("Configured %d combo sections from hit timing and reindexed %d hit notifies."),
		SectionNames.Num(),
		ReindexedHitCount);
	return Result;
}

FAIREAnimationComboMontageResult UAIREAnimationMCPToolset::ConfigureMontageSections(
	UAnimMontage* Montage,
	const TArray<FName>& SectionNames,
	const TArray<float>& SectionStartTimes)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeAnimationFailure(Error);
	}
	if (SectionNames.IsEmpty()
		|| SectionNames.Num() != SectionStartTimes.Num())
	{
		return MakeAnimationFailure(
			TEXT("Section names and start times must be non-empty and have equal counts."));
	}

	TSet<FName> UniqueSectionNames;
	float PreviousStartTime = -1.0f;
	for (int32 SectionIndex = 0; SectionIndex < SectionNames.Num(); ++SectionIndex)
	{
		const FName SectionName = SectionNames[SectionIndex];
		const float StartTime = SectionStartTimes[SectionIndex];
		if (SectionName.IsNone()
			|| UniqueSectionNames.Contains(SectionName)
			|| !FMath::IsFinite(StartTime)
			|| StartTime < 0.0f
			|| StartTime >= Montage->GetPlayLength()
			|| StartTime <= PreviousStartTime)
		{
			return MakeAnimationFailure(
				FString::Printf(
					TEXT("Invalid or duplicate montage section %s at %.3f."),
					*SectionName.ToString(),
					StartTime));
		}
		UniqueSectionNames.Add(SectionName);
		PreviousStartTime = StartTime;
	}
	if (!FMath::IsNearlyZero(SectionStartTimes[0], 0.001f))
	{
		return MakeAnimationFailure(
			TEXT("The first montage section must start at zero."));
	}

	bool bSectionLayoutMatches =
		Montage->GetNumSections() == SectionNames.Num();
	bool bSectionsAlreadyMatch = bSectionLayoutMatches;
	for (int32 SectionIndex = 0;
		bSectionLayoutMatches && SectionIndex < SectionNames.Num();
		++SectionIndex)
	{
		float ExistingStartTime = 0.0f;
		float ExistingEndTime = 0.0f;
		Montage->GetSectionStartAndEndTime(
			SectionIndex,
			ExistingStartTime,
			ExistingEndTime);
		bSectionLayoutMatches =
			Montage->GetSectionName(SectionIndex) == SectionNames[SectionIndex]
			&& FMath::IsNearlyEqual(
				ExistingStartTime,
				SectionStartTimes[SectionIndex],
				0.001f);
		bSectionsAlreadyMatch = bSectionsAlreadyMatch
			&& bSectionLayoutMatches
			&& Montage->CompositeSections[SectionIndex].NextSectionName.IsNone();
	}

	const bool bHasReplaceableDefaultSection =
		Montage->GetNumSections() == 1
		&& Montage->GetSectionName(0) == FName(TEXT("Default"));
	if (!bSectionLayoutMatches && !bHasReplaceableDefaultSection)
	{
		return MakeAnimationFailure(
			TEXT("Existing montage sections are neither the requested layout nor a single Default section."));
	}
	if (bSectionsAlreadyMatch)
	{
		FAIREAnimationComboMontageResult Result;
		PopulateInspection(*Montage, Result);
		Result.bSuccess = true;
		Result.Message = TEXT("The requested montage sections are already configured.");
		return Result;
	}

	const FScopedTransaction Transaction(
		LOCTEXT("ConfigureMontageSections", "Configure Montage Sections"));
	Montage->Modify();
	if (!bSectionLayoutMatches)
	{
		Montage->CompositeSections.Reset();
		for (int32 SectionIndex = 0; SectionIndex < SectionNames.Num(); ++SectionIndex)
		{
			if (Montage->AddAnimCompositeSection(
				SectionNames[SectionIndex],
				SectionStartTimes[SectionIndex]) == INDEX_NONE)
			{
				return MakeAnimationFailure(
					FString::Printf(
						TEXT("Failed to add montage section %s."),
						*SectionNames[SectionIndex].ToString()));
			}
		}
	}
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		Section.NextSectionName = NAME_None;
	}

	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();

	FAIREAnimationComboMontageResult Result;
	PopulateInspection(*Montage, Result);
	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("Configured %d montage sections with no automatic next sections."),
		SectionNames.Num());
	return Result;
}

FAIREAnimationComboMontageResult UAIREAnimationMCPToolset::InspectBasicAttackComboMontage(
	UAnimMontage* Montage)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeAnimationFailure(Error);
	}

	FAIREAnimationComboMontageResult Result;
	PopulateInspection(*Montage, Result);
	Result.bSuccess = true;
	Result.Message = TEXT("Combo montage inspected successfully.");
	return Result;
}

FAIREAnimationComboMontageResult UAIREAnimationMCPToolset::ConfigureBasicAttackComboWindows(
	UAnimMontage* Montage,
	const FName NotifyTrackName,
	const float WindowStartOffsetAfterHit,
	const float SectionEndPadding)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeAnimationFailure(Error);
	}
	if (NotifyTrackName.IsNone())
	{
		return MakeAnimationFailure(
			TEXT("NotifyTrackName must not be None."));
	}
	if (!FMath::IsFinite(WindowStartOffsetAfterHit) || WindowStartOffsetAfterHit < 0.0f)
	{
		return MakeAnimationFailure(
			TEXT("WindowStartOffsetAfterHit must be finite and non-negative."));
	}
	if (!FMath::IsFinite(SectionEndPadding) || SectionEndPadding <= 0.0f)
	{
		return MakeAnimationFailure(
			TEXT("SectionEndPadding must be finite and greater than zero."));
	}

	TArray<FAIREComboNotifyEntry> HitNotifies;
	TArray<FAIREComboNotifyEntry> ComboWindows;
	GatherComboNotifies(*Montage, HitNotifies, ComboWindows);
	if (!ValidateComboLayout(*Montage, HitNotifies, ComboWindows, Error))
	{
		return MakeAnimationFailure(Error);
	}

	TSet<int32> ExistingWindowSteps;
	for (const FAIREComboNotifyEntry& ComboWindow : ComboWindows)
	{
		ExistingWindowSteps.Add(ComboWindow.StepIndex);
	}

	struct FAIREPendingComboWindow
	{
		int32 StepIndex = INDEX_NONE;
		float StartTime = 0.0f;
		float Duration = 0.0f;
	};
	TArray<FAIREPendingComboWindow> PendingWindows;

	for (int32 StepIndex = 0; StepIndex < Montage->GetNumSections() - 1; ++StepIndex)
	{
		if (ExistingWindowSteps.Contains(StepIndex))
		{
			continue;
		}

		const FAIREComboNotifyEntry* HitNotify = HitNotifies.FindByPredicate(
			[StepIndex](const FAIREComboNotifyEntry& Entry)
			{
				return Entry.StepIndex == StepIndex;
			});
		check(HitNotify != nullptr);

		float SectionStartTime = 0.0f;
		float SectionEndTime = 0.0f;
		Montage->GetSectionStartAndEndTime(StepIndex, SectionStartTime, SectionEndTime);

		const float WindowStartTime = HitNotify->StartTime + WindowStartOffsetAfterHit;
		const float WindowEndTime = SectionEndTime - SectionEndPadding;
		if (WindowEndTime <= WindowStartTime + KINDA_SMALL_NUMBER)
		{
			return MakeAnimationFailure(
				FString::Printf(
					TEXT("Section %s has no room for a combo window after hit %.3f with end padding %.3f."),
					*Montage->GetSectionName(StepIndex).ToString(),
					HitNotify->StartTime,
					SectionEndPadding));
		}

		PendingWindows.Add(
			{StepIndex, WindowStartTime, WindowEndTime - WindowStartTime});
	}

	if (!PendingWindows.IsEmpty())
	{
		const FScopedTransaction Transaction(
			LOCTEXT("ConfigureBasicAttackComboWindows", "Configure Basic Attack Combo Windows"));
		Montage->Modify();

		if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(Montage, NotifyTrackName))
		{
			UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Montage, NotifyTrackName);
		}

		for (const FAIREPendingComboWindow& PendingWindow : PendingWindows)
		{
			UAIRECompanionComboWindowAnimNotifyState* ComboWindow =
				NewObject<UAIRECompanionComboWindowAnimNotifyState>(
					Montage,
					NAME_None,
					RF_Transactional);
			ComboWindow->ComboStepIndex = PendingWindow.StepIndex;

			UAnimationBlueprintLibrary::AddAnimationNotifyStateEventObject(
				Montage,
				PendingWindow.StartTime,
				PendingWindow.Duration,
				ComboWindow,
				NotifyTrackName);
		}

		Montage->RefreshCacheData();
		Montage->MarkPackageDirty();
	}

	FAIREAnimationComboMontageResult Result;
	PopulateInspection(*Montage, Result);
	Result.AddedComboWindowCount = PendingWindows.Num();
	Result.bSuccess = true;
	Result.Message = PendingWindows.IsEmpty()
		? TEXT("All required combo windows already exist; no changes were made.")
		: FString::Printf(TEXT("Added %d combo windows."), PendingWindows.Num());
	return Result;
}

FAIREAnimationNotifyMutationResult UAIREAnimationMCPToolset::InspectEnemyMeleeTraceMontage(
	UAnimMontage* Montage)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeNotifyMutationFailure(Error);
	}

	FAIREAnimationNotifyMutationResult Result;
	PopulateEnemyMeleeTraceInspection(*Montage, Result);
	Result.bSuccess = true;
	Result.Message = TEXT("Enemy melee trace montage inspected successfully.");
	return Result;
}

FAIREAnimationNotifyMutationResult UAIREAnimationMCPToolset::ConfigureEnemyMeleeTraceWindow(
	UAnimMontage* Montage,
	const FName NotifyTrackName,
	const float WindowStartTime,
	const float WindowEndTime)
{
	FAIREEnemyMeleeTraceWindowDefinition Window;
	Window.StartTime = WindowStartTime;
	Window.EndTime = WindowEndTime;
	TArray<FAIREEnemyMeleeTraceWindowDefinition> Windows;
	Windows.Add(Window);
	return ConfigureEnemyMeleeTraceWindows(
		Montage,
		NotifyTrackName,
		Windows);
}

FAIREAnimationNotifyMutationResult UAIREAnimationMCPToolset::ConfigureEnemyAttackMovementWindow(
	UAnimMontage* Montage,
	const FName NotifyTrackName,
	const float WindowStartTime,
	const float WindowEndTime)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeNotifyMutationFailure(Error);
	}
	if (NotifyTrackName.IsNone()
		|| !FMath::IsFinite(WindowStartTime)
		|| !FMath::IsFinite(WindowEndTime)
		|| WindowStartTime < 0.0f
		|| WindowEndTime <= WindowStartTime
		|| WindowEndTime > Montage->GetPlayLength())
	{
		return MakeNotifyMutationFailure(
			FString::Printf(
				TEXT("Invalid attack movement window %.3f-%.3f for montage length %.3f."),
				WindowStartTime,
				WindowEndTime,
				Montage->GetPlayLength()));
	}

	FAIREAnimationNotifyMutationResult ExistingResult;
	PopulateEnemyMeleeTraceInspection(*Montage, ExistingResult);
	const FAnimNotifyEvent* ExistingEvent = Montage->Notifies.FindByPredicate(
		[](const FAnimNotifyEvent& NotifyEvent)
		{
			return IsValid(Cast<UAIREEnemyAttackMovementAnimNotifyState>(
				NotifyEvent.NotifyStateClass));
		});
	const bool bAlreadyConfigured =
		ExistingResult.AttackMovementWindowCount == 1
		&& ExistingEvent
		&& FMath::IsNearlyEqual(
			ExistingEvent->GetTime(), WindowStartTime, 0.001f)
		&& FMath::IsNearlyEqual(
			ExistingEvent->GetTime() + ExistingEvent->GetDuration(),
			WindowEndTime,
			0.001f);
	if (bAlreadyConfigured)
	{
		ExistingResult.bSuccess = true;
		ExistingResult.Message =
			TEXT("The requested enemy attack movement window is already configured.");
		return ExistingResult;
	}

	const FScopedTransaction Transaction(
		LOCTEXT(
			"ConfigureEnemyAttackMovementWindow",
			"Configure Enemy Attack Movement Window"));
	Montage->Modify();

	FAIREAnimationNotifyMutationResult Result;
	Result.ReplacedAttackMovementWindowCount = Montage->Notifies.RemoveAll(
		[](const FAnimNotifyEvent& NotifyEvent)
		{
			return IsValid(Cast<UAIREEnemyAttackMovementAnimNotifyState>(
				NotifyEvent.NotifyStateClass));
		});
	if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(
		Montage,
		NotifyTrackName))
	{
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(
			Montage,
			NotifyTrackName);
	}

	UAIREEnemyAttackMovementAnimNotifyState* MovementWindow =
		NewObject<UAIREEnemyAttackMovementAnimNotifyState>(
			Montage,
			NAME_None,
			RF_Transactional);
	UAnimationBlueprintLibrary::AddAnimationNotifyStateEventObject(
		Montage,
		WindowStartTime,
		WindowEndTime - WindowStartTime,
		MovementWindow,
		NotifyTrackName);

	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	PopulateEnemyMeleeTraceInspection(*Montage, Result);
	Result.bSuccess = Result.AttackMovementWindowCount == 1;
	Result.Message = Result.bSuccess
		? FString::Printf(
			TEXT("Replaced %d attack movement windows and configured one at %.3f-%.3f."),
			Result.ReplacedAttackMovementWindowCount,
			WindowStartTime,
			WindowEndTime)
		: TEXT("The montage did not end with exactly one attack movement window.");
	return Result;
}

FAIREAnimationNotifyMutationResult UAIREAnimationMCPToolset::ConfigureEnemyMeleeTraceWindows(
	UAnimMontage* Montage,
	const FName NotifyTrackName,
	const TArray<FAIREEnemyMeleeTraceWindowDefinition>& Windows)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeNotifyMutationFailure(Error);
	}
	if (NotifyTrackName.IsNone())
	{
		return MakeNotifyMutationFailure(
			TEXT("NotifyTrackName must not be None."));
	}
	if (Windows.IsEmpty())
	{
		return MakeNotifyMutationFailure(
			TEXT("At least one trace window is required."));
	}
	TSet<int32> StrikeIndices;
	for (const FAIREEnemyMeleeTraceWindowDefinition& Window : Windows)
	{
		const bool bHasStartSocket = !Window.TraceStartSocket.IsNone();
		const bool bHasEndSocket = !Window.TraceEndSocket.IsNone();
		if (!FMath::IsFinite(Window.StartTime)
			|| !FMath::IsFinite(Window.EndTime)
			|| Window.StartTime < 0.0f
			|| Window.EndTime <= Window.StartTime
			|| Window.EndTime > Montage->GetPlayLength()
			|| Window.StrikeIndex < 0
			|| StrikeIndices.Contains(Window.StrikeIndex)
			|| !FMath::IsFinite(Window.DamageScale)
			|| Window.DamageScale < 0.0f
			|| !FMath::IsFinite(Window.StaggerScale)
			|| Window.StaggerScale < 0.0f
			|| (Window.DamageScale <= 0.0f && Window.StaggerScale <= 0.0f)
			|| bHasStartSocket != bHasEndSocket)
		{
			return MakeNotifyMutationFailure(
				FString::Printf(
					TEXT("Invalid or duplicate strike %d at %.3f-%.3f for montage length %.3f."),
					Window.StrikeIndex,
					Window.StartTime,
					Window.EndTime,
					Montage->GetPlayLength()));
		}
		StrikeIndices.Add(Window.StrikeIndex);
	}

	FAIREAnimationNotifyMutationResult ExistingResult;
	PopulateEnemyMeleeTraceInspection(*Montage, ExistingResult);
	bool bAlreadyConfigured = ExistingResult.LegacyNotifyCount == 0
		&& ExistingResult.TraceWindowCount == Windows.Num();
	for (const FAIREEnemyMeleeTraceWindowDefinition& Window : Windows)
	{
		const FAnimNotifyEvent* ExistingEvent =
			Montage->Notifies.FindByPredicate(
				[&Window](const FAnimNotifyEvent& NotifyEvent)
				{
					const UAIREEnemyMeleeTraceAnimNotifyState* TraceWindow =
						Cast<UAIREEnemyMeleeTraceAnimNotifyState>(
							NotifyEvent.NotifyStateClass);
					return IsValid(TraceWindow)
						&& TraceWindow->StrikeIndex == Window.StrikeIndex;
				});
		const UAIREEnemyMeleeTraceAnimNotifyState* ExistingWindow =
			ExistingEvent
				? Cast<UAIREEnemyMeleeTraceAnimNotifyState>(
					ExistingEvent->NotifyStateClass)
				: nullptr;
		bAlreadyConfigured = bAlreadyConfigured
			&& IsValid(ExistingWindow)
			&& FMath::IsNearlyEqual(
				ExistingEvent->GetTime(), Window.StartTime, 0.001f)
			&& FMath::IsNearlyEqual(
				ExistingEvent->GetTime() + ExistingEvent->GetDuration(),
				Window.EndTime,
				0.001f)
			&& FMath::IsNearlyEqual(
				ExistingWindow->DamageScale, Window.DamageScale)
			&& FMath::IsNearlyEqual(
				ExistingWindow->StaggerScale, Window.StaggerScale)
			&& ExistingWindow->TraceStartSocket == Window.TraceStartSocket
			&& ExistingWindow->TraceEndSocket == Window.TraceEndSocket;
	}
	if (bAlreadyConfigured)
	{
		ExistingResult.bSuccess = true;
		ExistingResult.Message =
			TEXT("The requested enemy melee trace windows are already configured.");
		return ExistingResult;
	}

	const FScopedTransaction Transaction(
		LOCTEXT("ConfigureEnemyMeleeTraceWindows", "Configure Enemy Melee Trace Windows"));
	Montage->Modify();

	FAIREAnimationNotifyMutationResult Result;
	Result.RemovedLegacyNotifyCount +=
		UAnimationBlueprintLibrary::RemoveAnimationNotifyEventsByName(
			Montage,
			FName(TEXT("SaveAttack")));
	Result.RemovedLegacyNotifyCount +=
		UAnimationBlueprintLibrary::RemoveAnimationNotifyEventsByName(
			Montage,
			FName(TEXT("ResetCombo")));

	Result.ReplacedTraceWindowCount = Montage->Notifies.RemoveAll(
		[](const FAnimNotifyEvent& NotifyEvent)
		{
			return IsValid(Cast<UAIREEnemyMeleeTraceAnimNotifyState>(
				NotifyEvent.NotifyStateClass));
		});

	if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(Montage, NotifyTrackName))
	{
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Montage, NotifyTrackName);
	}

	for (const FAIREEnemyMeleeTraceWindowDefinition& Window : Windows)
	{
		UAIREEnemyMeleeTraceAnimNotifyState* TraceWindow =
			NewObject<UAIREEnemyMeleeTraceAnimNotifyState>(
				Montage,
				NAME_None,
				RF_Transactional);
		TraceWindow->StrikeIndex = Window.StrikeIndex;
		TraceWindow->DamageScale = Window.DamageScale;
		TraceWindow->StaggerScale = Window.StaggerScale;
		TraceWindow->TraceStartSocket = Window.TraceStartSocket;
		TraceWindow->TraceEndSocket = Window.TraceEndSocket;
		UAnimationBlueprintLibrary::AddAnimationNotifyStateEventObject(
			Montage,
			Window.StartTime,
			Window.EndTime - Window.StartTime,
			TraceWindow,
			NotifyTrackName);
	}

	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();

	PopulateEnemyMeleeTraceInspection(*Montage, Result);
	Result.bSuccess = Result.LegacyNotifyCount == 0
		&& Result.TraceWindowCount == Windows.Num();
	Result.Message = Result.bSuccess
		? FString::Printf(
			TEXT("Removed %d legacy notifies and configured %d enemy melee trace windows."),
			Result.RemovedLegacyNotifyCount,
			Windows.Num())
		: TEXT("The montage did not end with the requested trace windows and no legacy attack notifies.");
	return Result;
}

FAIREAnimationNotifyMutationResult UAIREAnimationMCPToolset::InspectCompanionMeleeTraceMontage(
	UAnimMontage* Montage)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeNotifyMutationFailure(Error);
	}

	FAIREAnimationNotifyMutationResult Result;
	PopulateCompanionMeleeTraceInspection(*Montage, Result);
	Result.bSuccess = true;
	Result.Message = TEXT("Companion melee trace montage inspected successfully.");
	return Result;
}

FAIREAnimationNotifyMutationResult UAIREAnimationMCPToolset::ConfigureCompanionMeleeTraceWindows(
	UAnimMontage* Montage,
	const FName NotifyTrackName,
	const TArray<FAIRECompanionMeleeTraceWindowDefinition>& Windows)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeNotifyMutationFailure(Error);
	}
	if (NotifyTrackName.IsNone())
	{
		return MakeNotifyMutationFailure(
			TEXT("NotifyTrackName must not be None."));
	}
	if (Windows.IsEmpty())
	{
		return MakeNotifyMutationFailure(
			TEXT("At least one companion trace window is required."));
	}

	TSet<uint64> WindowKeys;
	for (const FAIRECompanionMeleeTraceWindowDefinition& Window : Windows)
	{
		const uint64 WindowKey =
			(static_cast<uint64>(Window.TraceMode) << 32)
			| static_cast<uint32>(Window.ComboStepIndex);
		if (!FMath::IsFinite(Window.StartTime)
			|| !FMath::IsFinite(Window.EndTime)
			|| Window.StartTime < 0.0f
			|| Window.EndTime <= Window.StartTime
			|| Window.EndTime > Montage->GetPlayLength()
			|| Window.ComboStepIndex < 0
			|| WindowKeys.Contains(WindowKey))
		{
			return MakeNotifyMutationFailure(
				FString::Printf(
					TEXT("Invalid or duplicate companion trace step %d at %.3f-%.3f for montage length %.3f."),
					Window.ComboStepIndex,
					Window.StartTime,
					Window.EndTime,
					Montage->GetPlayLength()));
		}
		WindowKeys.Add(WindowKey);
	}

	FAIREAnimationNotifyMutationResult ExistingResult;
	PopulateCompanionMeleeTraceInspection(*Montage, ExistingResult);
	bool bAlreadyConfigured =
		ExistingResult.TraceWindowCount == Windows.Num();
	for (const FAIRECompanionMeleeTraceWindowDefinition& Window : Windows)
	{
		const FAnimNotifyEvent* ExistingEvent =
			Montage->Notifies.FindByPredicate(
				[&Window](const FAnimNotifyEvent& NotifyEvent)
				{
					const UAIRECompanionMeleeTraceAnimNotifyState* TraceWindow =
						Cast<UAIRECompanionMeleeTraceAnimNotifyState>(
							NotifyEvent.NotifyStateClass);
					return IsValid(TraceWindow)
						&& TraceWindow->TraceMode == Window.TraceMode
						&& TraceWindow->ComboStepIndex == Window.ComboStepIndex;
				});
		bAlreadyConfigured = bAlreadyConfigured
			&& ExistingEvent
			&& FMath::IsNearlyEqual(
				ExistingEvent->GetTime(), Window.StartTime, 0.001f)
			&& FMath::IsNearlyEqual(
				ExistingEvent->GetTime() + ExistingEvent->GetDuration(),
				Window.EndTime,
				0.001f);
	}
	if (bAlreadyConfigured)
	{
		ExistingResult.bSuccess = true;
		ExistingResult.Message =
			TEXT("The requested companion melee trace windows are already configured.");
		return ExistingResult;
	}

	const FScopedTransaction Transaction(
		LOCTEXT(
			"ConfigureCompanionMeleeTraceWindows",
			"Configure Companion Melee Trace Windows"));
	Montage->Modify();

	FAIREAnimationNotifyMutationResult Result;
	Result.ReplacedTraceWindowCount = Montage->Notifies.RemoveAll(
		[](const FAnimNotifyEvent& NotifyEvent)
		{
			return IsValid(Cast<UAIRECompanionMeleeTraceAnimNotifyState>(
				NotifyEvent.NotifyStateClass));
		});

	if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(
		Montage,
		NotifyTrackName))
	{
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(
			Montage,
			NotifyTrackName);
	}

	for (const FAIRECompanionMeleeTraceWindowDefinition& Window : Windows)
	{
		UAIRECompanionMeleeTraceAnimNotifyState* TraceWindow =
			NewObject<UAIRECompanionMeleeTraceAnimNotifyState>(
				Montage,
				NAME_None,
				RF_Transactional);
		TraceWindow->TraceMode = Window.TraceMode;
		TraceWindow->ComboStepIndex = Window.ComboStepIndex;
		UAnimationBlueprintLibrary::AddAnimationNotifyStateEventObject(
			Montage,
			Window.StartTime,
			Window.EndTime - Window.StartTime,
			TraceWindow,
			NotifyTrackName);
	}

	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	PopulateCompanionMeleeTraceInspection(*Montage, Result);
	Result.bSuccess = Result.TraceWindowCount == Windows.Num();
	Result.Message = Result.bSuccess
		? FString::Printf(
			TEXT("Replaced %d companion trace windows and configured %d windows."),
			Result.ReplacedTraceWindowCount,
			Windows.Num())
		: TEXT("The montage did not end with the requested companion trace windows.");
	return Result;
}

FAIREAnimationMontageTrackResult UAIREAnimationMCPToolset::ConfigureMontageAnimationTrack(
	UAnimMontage* Montage,
	const FName SlotName,
	const TArray<FAIREAnimationMontageSegmentDefinition>& Segments,
	const bool bClearMontageNotifies)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeMontageTrackFailure(Error);
	}
	if (SlotName.IsNone() || Segments.IsEmpty())
	{
		return MakeMontageTrackFailure(
			TEXT("SlotName and at least one animation segment are required."));
	}
	for (const FAIREAnimationMontageSegmentDefinition& Definition : Segments)
	{
		if (!IsValid(Definition.Animation)
			|| Definition.Animation->IsA<UAnimMontage>()
			|| !Definition.Animation->CanBeUsedInComposition()
			|| Definition.Animation->GetSkeleton() != Montage->GetSkeleton()
			|| !FMath::IsFinite(Definition.PlayRate)
			|| Definition.PlayRate <= 0.0f
			|| Definition.LoopingCount < 1)
		{
			return MakeMontageTrackFailure(
				TEXT("Every segment must be a compatible animation with a positive play rate and loop count."));
		}
	}

	const FScopedTransaction Transaction(
		LOCTEXT("ConfigureMontageAnimationTrack", "Configure Montage Animation Track"));
	Montage->Modify();
	Montage->SlotAnimTracks.Reset();
	FSlotAnimationTrack& Track = Montage->AddSlot(SlotName);
	float SegmentStartTime = 0.0f;
	for (const FAIREAnimationMontageSegmentDefinition& Definition : Segments)
	{
		FAnimSegment Segment;
		Segment.SetAnimReference(Definition.Animation, true);
		Segment.StartPos = SegmentStartTime;
		Segment.AnimPlayRate = Definition.PlayRate;
		Segment.LoopingCount = Definition.LoopingCount;
		SegmentStartTime += Segment.GetLength();
		Track.AnimTrack.AnimSegments.Add(Segment);
	}
	Montage->CompositeSections.Reset();
	Montage->AddAnimCompositeSection(FName(TEXT("Default")), 0.0f);
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		Section.NextSectionName = NAME_None;
	}
	if (bClearMontageNotifies)
	{
		Montage->Notifies.Reset();
	}
	Montage->SetCompositeLength(Montage->CalculateSequenceLength());
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();

	FAIREAnimationMontageTrackResult Result;
	Result.bSuccess = Montage->SlotAnimTracks.Num() == 1
		&& Track.AnimTrack.AnimSegments.Num() == Segments.Num()
		&& FMath::IsNearlyEqual(Montage->GetPlayLength(), SegmentStartTime, 0.001f);
	Result.SegmentCount = Track.AnimTrack.AnimSegments.Num();
	Result.PlayLength = Montage->GetPlayLength();
	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FAIREAnimationMontageSegmentDefinition& Definition =
			Segments[SegmentIndex];
		Result.Entries.Add(
			FString::Printf(
				TEXT("Segment[%d] %s rate=%.2f loops=%d"),
				SegmentIndex,
				*GetNameSafe(Definition.Animation),
				Definition.PlayRate,
				Definition.LoopingCount));
	}
	Result.Message = Result.bSuccess
		? FString::Printf(
			TEXT("Configured %d montage segments with play length %.3f."),
			Segments.Num(),
			Result.PlayLength)
		: TEXT("The montage track was edited but its calculated play length did not match the requested segments.");
	return Result;
}

FAIREAnimationBoneMotionResult UAIREAnimationMCPToolset::InspectAnimationBoneMotion(
	UAnimSequence* Animation,
	USkeletalMesh* SkeletalMesh,
	const TArray<FName>& BoneNames,
	const int32 SampleCount)
{
	if (!IsValid(Animation)
		|| !IsValid(SkeletalMesh)
		|| Animation->GetSkeleton() != SkeletalMesh->GetSkeleton()
		|| BoneNames.IsEmpty()
		|| SampleCount < 3
		|| SampleCount > 121)
	{
		return MakeBoneMotionFailure(
			TEXT("A compatible animation, skeletal mesh, one or more bones, and 3-121 samples are required."));
	}

	const FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh->GetRefSkeleton();
	TArray<int32> BoneIndices;
	BoneIndices.Reserve(BoneNames.Num());
	for (const FName BoneName : BoneNames)
	{
		const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			return MakeBoneMotionFailure(
				FString::Printf(
					TEXT("Bone %s does not exist on mesh %s."),
					*BoneName.ToString(),
					*GetNameSafe(SkeletalMesh)));
		}
		BoneIndices.Add(BoneIndex);
	}

	struct FBoneMotionMetric
	{
		FVector PreviousPosition = FVector::ZeroVector;
		float PathLength = 0.0f;
		float PeakSpeed = 0.0f;
		float PeakSpeedTime = 0.0f;
		float MaximumReach = 0.0f;
		float MaximumReachTime = 0.0f;
		float MaximumForward = -TNumericLimits<float>::Max();
		float MaximumForwardTime = 0.0f;
	};
	TArray<FBoneMotionMetric> Metrics;
	Metrics.SetNum(BoneNames.Num());
	const int32 BoneCount = ReferenceSkeleton.GetNum();
	TArray<FTransform> LocalTransforms;
	TArray<FTransform> ComponentTransforms;
	LocalTransforms.SetNum(BoneCount);
	ComponentTransforms.SetNum(BoneCount);
	const float PlayLength = Animation->GetPlayLength();
	const float SampleDelta = PlayLength / static_cast<float>(SampleCount - 1);

	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const float SampleTime = SampleDelta * SampleIndex;
		const FAnimExtractContext ExtractionContext(SampleTime, false);
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			LocalTransforms[BoneIndex] =
				ReferenceSkeleton.GetRefBonePose()[BoneIndex];
			Animation->GetBoneTransform(
				LocalTransforms[BoneIndex],
				FSkeletonPoseBoneIndex(BoneIndex),
				ExtractionContext,
				false);
			const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
			ComponentTransforms[BoneIndex] = ParentIndex == INDEX_NONE
				? LocalTransforms[BoneIndex]
				: LocalTransforms[BoneIndex] * ComponentTransforms[ParentIndex];
		}
		const FVector RootPosition = ComponentTransforms[0].GetLocation();
		for (int32 MetricIndex = 0; MetricIndex < BoneIndices.Num(); ++MetricIndex)
		{
			const FVector Position =
				ComponentTransforms[BoneIndices[MetricIndex]].GetLocation()
				- RootPosition;
			FBoneMotionMetric& Metric = Metrics[MetricIndex];
			if (SampleIndex > 0)
			{
				const float Distance = FVector::Distance(
					Position,
					Metric.PreviousPosition);
				Metric.PathLength += Distance;
				const float Speed = SampleDelta > 0.0f
					? Distance / SampleDelta
					: 0.0f;
				if (Speed > Metric.PeakSpeed)
				{
					Metric.PeakSpeed = Speed;
					Metric.PeakSpeedTime = SampleTime;
				}
			}
			const float Reach = Position.Size2D();
			if (Reach > Metric.MaximumReach)
			{
				Metric.MaximumReach = Reach;
				Metric.MaximumReachTime = SampleTime;
			}
			if (Position.X > Metric.MaximumForward)
			{
				Metric.MaximumForward = Position.X;
				Metric.MaximumForwardTime = SampleTime;
			}
			Metric.PreviousPosition = Position;
		}
	}

	FAIREAnimationBoneMotionResult Result;
	Result.bSuccess = true;
	Result.PlayLength = PlayLength;
	for (int32 MetricIndex = 0; MetricIndex < Metrics.Num(); ++MetricIndex)
	{
		const FBoneMotionMetric& Metric = Metrics[MetricIndex];
		Result.Entries.Add(
			FString::Printf(
				TEXT("%s path=%.1f peakSpeed=%.1f@%.3f maxReach=%.1f@%.3f maxForward=%.1f@%.3f"),
				*BoneNames[MetricIndex].ToString(),
				Metric.PathLength,
				Metric.PeakSpeed,
				Metric.PeakSpeedTime,
				Metric.MaximumReach,
				Metric.MaximumReachTime,
				Metric.MaximumForward,
				Metric.MaximumForwardTime));
	}
	Result.Message = FString::Printf(
		TEXT("Inspected %d bones across %d samples over %.3f seconds."),
		BoneNames.Num(),
		SampleCount,
		PlayLength);
	return Result;
}

FAIREAnimationBoneMotionResult UAIREAnimationMCPToolset::InspectAnimationBoneLocalRotations(
	UAnimSequence* Animation,
	USkeletalMesh* SkeletalMesh,
	const TArray<FName>& BoneNames,
	const TArray<float>& NormalizedTimes)
{
	if (!IsValid(Animation)
		|| !IsValid(SkeletalMesh)
		|| Animation->GetSkeleton() != SkeletalMesh->GetSkeleton()
		|| BoneNames.IsEmpty()
		|| NormalizedTimes.IsEmpty()
		|| NormalizedTimes.Num() > 11)
	{
		return MakeBoneMotionFailure(
			TEXT("A compatible animation, skeletal mesh, bones, and 1-11 normalized times are required."));
	}
	for (const float NormalizedTime : NormalizedTimes)
	{
		if (!FMath::IsFinite(NormalizedTime)
			|| NormalizedTime < 0.0f
			|| NormalizedTime > 1.0f)
		{
			return MakeBoneMotionFailure(
				TEXT("Every normalized time must be finite and in the inclusive 0-1 range."));
		}
	}

	const FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh->GetRefSkeleton();
	TArray<int32> BoneIndices;
	BoneIndices.Reserve(BoneNames.Num());
	for (const FName BoneName : BoneNames)
	{
		const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			return MakeBoneMotionFailure(FString::Printf(
				TEXT("Bone %s does not exist on mesh %s."),
				*BoneName.ToString(),
				*GetNameSafe(SkeletalMesh)));
		}
		BoneIndices.Add(BoneIndex);
	}

	FAIREAnimationBoneMotionResult Result;
	Result.bSuccess = true;
	Result.PlayLength = Animation->GetPlayLength();
	for (int32 BoneEntryIndex = 0; BoneEntryIndex < BoneIndices.Num(); ++BoneEntryIndex)
	{
		const int32 BoneIndex = BoneIndices[BoneEntryIndex];
		const FQuat ReferenceRotation =
			ReferenceSkeleton.GetRefBonePose()[BoneIndex].GetRotation().GetNormalized();
		for (const float NormalizedTime : NormalizedTimes)
		{
			const float SampleTime = Result.PlayLength * NormalizedTime;
			FTransform AnimatedTransform =
				ReferenceSkeleton.GetRefBonePose()[BoneIndex];
			Animation->GetBoneTransform(
				AnimatedTransform,
				FSkeletonPoseBoneIndex(BoneIndex),
				FAnimExtractContext(SampleTime, false),
				false);
			const FQuat AnimatedRotation =
				AnimatedTransform.GetRotation().GetNormalized();
			const FQuat DeltaRotation =
				ReferenceRotation.Inverse() * AnimatedRotation;
			const FRotator DeltaRotator = DeltaRotation.Rotator();
			const float DeltaAngleDegrees = FMath::RadiansToDegrees(
				ReferenceRotation.AngularDistance(AnimatedRotation));
			Result.Entries.Add(FString::Printf(
				TEXT("%s normalized=%.3f time=%.3f refDelta=%.3f delta(P=%.3f Y=%.3f R=%.3f)"),
				*BoneNames[BoneEntryIndex].ToString(),
				NormalizedTime,
				SampleTime,
				DeltaAngleDegrees,
				DeltaRotator.Pitch,
				DeltaRotator.Yaw,
				DeltaRotator.Roll));
		}
	}
	Result.Message = FString::Printf(
		TEXT("Inspected local rotations for %d bones at %d normalized times over %.3f seconds."),
		BoneNames.Num(),
		NormalizedTimes.Num(),
		Result.PlayLength);
	return Result;
}

FAIREAnimationBoneMotionResult UAIREAnimationMCPToolset::InspectAnimationBoneLocalTransforms(
	UAnimSequence* Animation,
	USkeletalMesh* SkeletalMesh,
	const TArray<FName>& BoneNames,
	const TArray<float>& NormalizedTimes)
{
	if (!IsValid(Animation)
		|| !IsValid(SkeletalMesh)
		|| Animation->GetSkeleton() != SkeletalMesh->GetSkeleton()
		|| BoneNames.IsEmpty()
		|| NormalizedTimes.IsEmpty()
		|| NormalizedTimes.Num() > 121)
	{
		return MakeBoneMotionFailure(
			TEXT("A compatible animation, skeletal mesh, one or more bones, and 1-121 normalized sample times are required."));
	}
	for (const float NormalizedTime : NormalizedTimes)
	{
		if (!FMath::IsFinite(NormalizedTime)
			|| NormalizedTime < 0.0f
			|| NormalizedTime > 1.0f)
		{
			return MakeBoneMotionFailure(
				TEXT("Every normalized sample time must be finite and between zero and one."));
		}
	}

	const FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh->GetRefSkeleton();
	TArray<int32> BoneIndices;
	BoneIndices.Reserve(BoneNames.Num());
	for (const FName BoneName : BoneNames)
	{
		const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			return MakeBoneMotionFailure(
				FString::Printf(
					TEXT("Bone %s does not exist on mesh %s."),
					*BoneName.ToString(),
					*GetNameSafe(SkeletalMesh)));
		}
		BoneIndices.Add(BoneIndex);
	}

	FAIREAnimationBoneMotionResult Result;
	Result.bSuccess = true;
	Result.PlayLength = Animation->GetPlayLength();
	for (int32 BoneEntryIndex = 0; BoneEntryIndex < BoneIndices.Num(); ++BoneEntryIndex)
	{
		const int32 BoneIndex = BoneIndices[BoneEntryIndex];
		const FTransform ReferenceTransform =
			ReferenceSkeleton.GetRefBonePose()[BoneIndex];
		FTransform FirstTransform = ReferenceTransform;
		Animation->GetBoneTransform(
			FirstTransform,
			FSkeletonPoseBoneIndex(BoneIndex),
			FAnimExtractContext(0.0f, false),
			false);

		for (const float NormalizedTime : NormalizedTimes)
		{
			const float SampleTime = Result.PlayLength * NormalizedTime;
			FTransform AnimatedTransform = ReferenceTransform;
			Animation->GetBoneTransform(
				AnimatedTransform,
				FSkeletonPoseBoneIndex(BoneIndex),
				FAnimExtractContext(SampleTime, false),
				false);

			const FVector Location = AnimatedTransform.GetLocation();
			const FVector FirstDelta = Location - FirstTransform.GetLocation();
			const FVector ReferenceDelta = Location - ReferenceTransform.GetLocation();
			const FRotator Rotation = AnimatedTransform.GetRotation().Rotator();
			Result.Entries.Add(FString::Printf(
				TEXT("%s normalized=%.3f time=%.3f location(X=%.3f Y=%.3f Z=%.3f) firstDelta(X=%.3f Y=%.3f Z=%.3f) refDelta(X=%.3f Y=%.3f Z=%.3f) rotation(P=%.3f Y=%.3f R=%.3f)"),
				*BoneNames[BoneEntryIndex].ToString(),
				NormalizedTime,
				SampleTime,
				Location.X,
				Location.Y,
				Location.Z,
				FirstDelta.X,
				FirstDelta.Y,
				FirstDelta.Z,
				ReferenceDelta.X,
				ReferenceDelta.Y,
				ReferenceDelta.Z,
				Rotation.Pitch,
				Rotation.Yaw,
				Rotation.Roll));
		}
	}
	Result.Message = FString::Printf(
		TEXT("Inspected local transforms for %d bones at %d normalized times over %.3f seconds."),
		BoneNames.Num(),
		NormalizedTimes.Num(),
		Result.PlayLength);
	return Result;
}

FAIREAnimationBoneMotionResult UAIREAnimationMCPToolset::BakePlanarRootMotionFromBone(
	UAnimSequence* Animation,
	USkeletalMesh* SkeletalMesh,
	const FName MotionBoneName)
{
	if (!IsAllowedAnimationAsset(Animation)
		|| !IsValid(SkeletalMesh)
		|| Animation->GetSkeleton() != SkeletalMesh->GetSkeleton()
		|| MotionBoneName.IsNone())
	{
		return MakeBoneMotionFailure(
			TEXT("An editable compatible animation, skeletal mesh, and motion bone are required."));
	}

	const FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh->GetRefSkeleton();
	if (ReferenceSkeleton.GetNum() < 2)
	{
		return MakeBoneMotionFailure(TEXT("The skeletal mesh has no usable root hierarchy."));
	}
	const int32 RootBoneIndex = 0;
	const int32 MotionBoneIndex = ReferenceSkeleton.FindBoneIndex(MotionBoneName);
	if (MotionBoneIndex == INDEX_NONE)
	{
		return MakeBoneMotionFailure(
			FString::Printf(TEXT("Motion bone %s does not exist."), *MotionBoneName.ToString()));
	}
	if (ReferenceSkeleton.GetParentIndex(MotionBoneIndex) != RootBoneIndex)
	{
		return MakeBoneMotionFailure(
			FString::Printf(
				TEXT("Motion bone %s must be a direct child of root for lossless planar baking."),
				*MotionBoneName.ToString()));
	}

	const FName RootBoneName = ReferenceSkeleton.GetBoneName(RootBoneIndex);
	const IAnimationDataModel* DataModel = Animation->GetDataModel();
	if (!DataModel
		|| !DataModel->IsValidBoneTrackName(RootBoneName)
		|| !DataModel->IsValidBoneTrackName(MotionBoneName))
	{
		return MakeBoneMotionFailure(
			TEXT("Both the root and motion bone must have editable animation tracks."));
	}

	TArray<FTransform> RootTransforms;
	TArray<FTransform> MotionTransforms;
	DataModel->GetBoneTrackTransforms(RootBoneName, RootTransforms);
	DataModel->GetBoneTrackTransforms(MotionBoneName, MotionTransforms);
	if (RootTransforms.Num() < 2 || RootTransforms.Num() != MotionTransforms.Num())
	{
		return MakeBoneMotionFailure(
			TEXT("Root and motion bone tracks must have the same number of two or more keys."));
	}

	const FVector InitialMotionLocation = MotionTransforms[0].GetLocation();
	TArray<FVector3f> RootPositionKeys;
	TArray<FQuat4f> RootRotationKeys;
	TArray<FVector3f> RootScaleKeys;
	TArray<FVector3f> MotionPositionKeys;
	TArray<FQuat4f> MotionRotationKeys;
	TArray<FVector3f> MotionScaleKeys;
	RootPositionKeys.Reserve(RootTransforms.Num());
	RootRotationKeys.Reserve(RootTransforms.Num());
	RootScaleKeys.Reserve(RootTransforms.Num());
	MotionPositionKeys.Reserve(MotionTransforms.Num());
	MotionRotationKeys.Reserve(MotionTransforms.Num());
	MotionScaleKeys.Reserve(MotionTransforms.Num());

	float MaximumPlanarOffset = 0.0f;
	for (int32 KeyIndex = 0; KeyIndex < RootTransforms.Num(); ++KeyIndex)
	{
		const FTransform OriginalRootTransform = RootTransforms[KeyIndex];
		const FTransform OriginalMotionTransform = MotionTransforms[KeyIndex];
		const FTransform OriginalComponentTransform =
			OriginalMotionTransform * OriginalRootTransform;

		FVector PlanarOffset =
			OriginalMotionTransform.GetLocation() - InitialMotionLocation;
		PlanarOffset.Z = 0.0;
		MaximumPlanarOffset = FMath::Max(
			MaximumPlanarOffset,
			static_cast<float>(PlanarOffset.Size()));

		FTransform BakedRootTransform = OriginalRootTransform;
		BakedRootTransform.AddToTranslation(PlanarOffset);
		const FTransform BakedMotionTransform =
			OriginalComponentTransform.GetRelativeTransform(BakedRootTransform);

		RootPositionKeys.Add(FVector3f(BakedRootTransform.GetLocation()));
		RootRotationKeys.Add(FQuat4f(BakedRootTransform.GetRotation()));
		RootScaleKeys.Add(FVector3f(BakedRootTransform.GetScale3D()));
		MotionPositionKeys.Add(FVector3f(BakedMotionTransform.GetLocation()));
		MotionRotationKeys.Add(FQuat4f(BakedMotionTransform.GetRotation()));
		MotionScaleKeys.Add(FVector3f(BakedMotionTransform.GetScale3D()));
	}

	const FScopedTransaction Transaction(
		LOCTEXT("BakePlanarRootMotionFromBone", "Bake Planar Root Motion From Bone"));
	Animation->Modify();
	IAnimationDataController& Controller = Animation->GetController();
	Controller.OpenBracket(
		LOCTEXT("BakePlanarRootMotionFromBoneBracket", "Bake Planar Root Motion From Bone"));
	const bool bRootUpdated = Controller.SetBoneTrackKeys(
		RootBoneName,
		RootPositionKeys,
		RootRotationKeys,
		RootScaleKeys);
	const bool bMotionBoneUpdated = Controller.SetBoneTrackKeys(
		MotionBoneName,
		MotionPositionKeys,
		MotionRotationKeys,
		MotionScaleKeys);
	Controller.CloseBracket();
	if (!bRootUpdated || !bMotionBoneUpdated)
	{
		return MakeBoneMotionFailure(
			TEXT("Failed to update the root or motion bone track."));
	}

	Animation->MarkPackageDirty();
	FAIREAnimationBoneMotionResult Result;
	Result.bSuccess = true;
	Result.PlayLength = Animation->GetPlayLength();
	Result.Entries.Add(FString::Printf(
		TEXT("root=%s motionBone=%s keys=%d maxPlanarOffset=%.3f"),
		*RootBoneName.ToString(),
		*MotionBoneName.ToString(),
		RootTransforms.Num(),
		MaximumPlanarOffset));
	Result.Message =
		TEXT("Baked planar motion into root while preserving component-space pose and vertical motion on the source bone.");
	return Result;
}

FAIREAnimationBoneMotionResult UAIREAnimationMCPToolset::NormalizeRootRotationToReference(
	UAnimSequence* Animation,
	USkeletalMesh* SkeletalMesh,
	const FName MotionBoneName)
{
	if (!IsAllowedAnimationAsset(Animation)
		|| !IsValid(SkeletalMesh)
		|| Animation->GetSkeleton() != SkeletalMesh->GetSkeleton()
		|| MotionBoneName.IsNone())
	{
		return MakeBoneMotionFailure(
			TEXT("An editable compatible animation, skeletal mesh, and motion bone are required."));
	}

	const FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh->GetRefSkeleton();
	if (ReferenceSkeleton.GetNum() < 2)
	{
		return MakeBoneMotionFailure(TEXT("The skeletal mesh has no usable root hierarchy."));
	}
	const int32 RootBoneIndex = 0;
	const int32 MotionBoneIndex = ReferenceSkeleton.FindBoneIndex(MotionBoneName);
	if (MotionBoneIndex == INDEX_NONE
		|| ReferenceSkeleton.GetParentIndex(MotionBoneIndex) != RootBoneIndex)
	{
		return MakeBoneMotionFailure(
			FString::Printf(
				TEXT("Motion bone %s must be a direct child of root."),
				*MotionBoneName.ToString()));
	}

	const FName RootBoneName = ReferenceSkeleton.GetBoneName(RootBoneIndex);
	const IAnimationDataModel* DataModel = Animation->GetDataModel();
	if (!DataModel
		|| !DataModel->IsValidBoneTrackName(RootBoneName)
		|| !DataModel->IsValidBoneTrackName(MotionBoneName))
	{
		return MakeBoneMotionFailure(
			TEXT("Both the root and motion bone must have editable animation tracks."));
	}

	TArray<FTransform> RootTransforms;
	TArray<FTransform> MotionTransforms;
	DataModel->GetBoneTrackTransforms(RootBoneName, RootTransforms);
	DataModel->GetBoneTrackTransforms(MotionBoneName, MotionTransforms);
	if (RootTransforms.Num() < 2 || RootTransforms.Num() != MotionTransforms.Num())
	{
		return MakeBoneMotionFailure(
			TEXT("Root and motion bone tracks must have the same number of two or more keys."));
	}

	const FQuat ReferenceRootRotation =
		ReferenceSkeleton.GetRefBonePose()[RootBoneIndex].GetRotation().GetNormalized();
	float MaximumRootRotationDelta = 0.0f;
	TArray<FVector3f> RootPositionKeys;
	TArray<FQuat4f> RootRotationKeys;
	TArray<FVector3f> RootScaleKeys;
	TArray<FVector3f> MotionPositionKeys;
	TArray<FQuat4f> MotionRotationKeys;
	TArray<FVector3f> MotionScaleKeys;
	RootPositionKeys.Reserve(RootTransforms.Num());
	RootRotationKeys.Reserve(RootTransforms.Num());
	RootScaleKeys.Reserve(RootTransforms.Num());
	MotionPositionKeys.Reserve(MotionTransforms.Num());
	MotionRotationKeys.Reserve(MotionTransforms.Num());
	MotionScaleKeys.Reserve(MotionTransforms.Num());

	for (int32 KeyIndex = 0; KeyIndex < RootTransforms.Num(); ++KeyIndex)
	{
		const FTransform OriginalRootTransform = RootTransforms[KeyIndex];
		const FTransform OriginalMotionTransform = MotionTransforms[KeyIndex];
		const FTransform OriginalComponentTransform =
			OriginalMotionTransform * OriginalRootTransform;
		MaximumRootRotationDelta = FMath::Max(
			MaximumRootRotationDelta,
			FMath::RadiansToDegrees(
				ReferenceRootRotation.AngularDistance(
					OriginalRootTransform.GetRotation().GetNormalized())));

		FTransform CorrectedRootTransform = OriginalRootTransform;
		CorrectedRootTransform.SetRotation(ReferenceRootRotation);
		const FTransform CorrectedMotionTransform =
			OriginalComponentTransform.GetRelativeTransform(CorrectedRootTransform);

		RootPositionKeys.Add(FVector3f(CorrectedRootTransform.GetLocation()));
		RootRotationKeys.Add(FQuat4f(CorrectedRootTransform.GetRotation()));
		RootScaleKeys.Add(FVector3f(CorrectedRootTransform.GetScale3D()));
		MotionPositionKeys.Add(FVector3f(CorrectedMotionTransform.GetLocation()));
		MotionRotationKeys.Add(FQuat4f(CorrectedMotionTransform.GetRotation()));
		MotionScaleKeys.Add(FVector3f(CorrectedMotionTransform.GetScale3D()));
	}

	const FScopedTransaction Transaction(
		LOCTEXT("NormalizeRootRotationToReference", "Normalize Root Rotation To Reference"));
	Animation->Modify();
	IAnimationDataController& Controller = Animation->GetController();
	Controller.OpenBracket(
		LOCTEXT("NormalizeRootRotationToReferenceBracket", "Normalize Root Rotation To Reference"));
	const bool bRootUpdated = Controller.SetBoneTrackKeys(
		RootBoneName,
		RootPositionKeys,
		RootRotationKeys,
		RootScaleKeys);
	const bool bMotionBoneUpdated = Controller.SetBoneTrackKeys(
		MotionBoneName,
		MotionPositionKeys,
		MotionRotationKeys,
		MotionScaleKeys);
	Controller.CloseBracket();
	if (!bRootUpdated || !bMotionBoneUpdated)
	{
		return MakeBoneMotionFailure(
			TEXT("Failed to update the root or motion bone track."));
	}

	Animation->MarkPackageDirty();
	FAIREAnimationBoneMotionResult Result;
	Result.bSuccess = true;
	Result.PlayLength = Animation->GetPlayLength();
	Result.Entries.Add(FString::Printf(
		TEXT("root=%s motionBone=%s keys=%d maxRootRotationDelta=%.3f"),
		*RootBoneName.ToString(),
		*MotionBoneName.ToString(),
		RootTransforms.Num(),
		MaximumRootRotationDelta));
	Result.Message =
		TEXT("Normalized root rotation to the skeletal reference while preserving the motion bone component-space pose.");
	return Result;
}

FAIREControlRigHierarchySyncResult UAIREAnimationMCPToolset::SyncControlRigBoneHierarchy(
	UControlRigBlueprint* ControlRigBlueprint,
	USkeletalMesh* SkeletalMesh)
{
	if (!IsAllowedAnimationAsset(ControlRigBlueprint))
	{
		return MakeControlRigHierarchyFailure(
			FString::Printf(
				TEXT("A valid Control Rig Blueprint under %s is required."),
				*AllowedAnimationAssetRoot));
	}
	if (!IsAllowedAnimationAsset(SkeletalMesh))
	{
		return MakeControlRigHierarchyFailure(
			FString::Printf(
				TEXT("A valid Skeletal Mesh under %s is required."),
				*AllowedAnimationAssetRoot));
	}
	if (!IsValid(SkeletalMesh->GetSkeleton()))
	{
		return MakeControlRigHierarchyFailure(
			TEXT("The Skeletal Mesh has no assigned Skeleton."));
	}

	URigHierarchy* Hierarchy = ControlRigBlueprint->GetHierarchy();
	URigHierarchyController* HierarchyController =
		ControlRigBlueprint->GetHierarchyController();
	if (!IsValid(Hierarchy) || !IsValid(HierarchyController))
	{
		return MakeControlRigHierarchyFailure(
			TEXT("The Control Rig Blueprint has no editable hierarchy."));
	}

	FAIREControlRigHierarchySyncResult Result;
	TArray<FString> DiscrepanciesBefore;
	GatherHierarchyDiscrepancies(
		*Hierarchy,
		*SkeletalMesh,
		DiscrepanciesBefore);
	Result.DiscrepancyCountBefore = DiscrepanciesBefore.Num();

	const FScopedTransaction Transaction(
		LOCTEXT("SyncControlRigBoneHierarchy", "Sync Control Rig Bone Hierarchy"));
	ControlRigBlueprint->UObject::Modify();
	Hierarchy->Modify();

	{
		TGuardValue<bool> SuspendHierarchyNotifications(
			HierarchyController->GetSuspendNotificationsFlag(),
			true);
		HierarchyController->ImportBonesFromSkeletalMesh(
			SkeletalMesh,
			NAME_None,
			true,
			true,
			false,
			true,
			false);
	}

	ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
	ControlRigBlueprint->GetRigVMAssetInterface()->MarkAssetAsModified();
	ControlRigBlueprint->GetRigVMAssetInterface()->BroadcastRefreshEditor();
	ControlRigBlueprint->MarkPackageDirty();

	Result.SyncedBoneCount =
		Hierarchy->GetAllKeys(false, ERigElementType::Bone).Num();
	GatherHierarchyDiscrepancies(
		*Hierarchy,
		*SkeletalMesh,
		Result.Entries);
	Result.DiscrepancyCountAfter = Result.Entries.Num();
	Result.bSuccess =
		Result.SyncedBoneCount > 0 && Result.DiscrepancyCountAfter == 0;
	if (Result.bSuccess)
	{
		Result.Message = FString::Printf(
			TEXT("Synchronized %d Control Rig bones. Compile and save the Control Rig explicitly."),
			Result.SyncedBoneCount);
	}
	else if (Result.SyncedBoneCount == 0)
	{
		Result.Message = TEXT("Hierarchy sync produced no Control Rig bones.");
	}
	else
	{
		Result.Message = FString::Printf(
			TEXT("Hierarchy sync completed, but %d parent discrepancies remain."),
			Result.DiscrepancyCountAfter);
	}
	return Result;
}

FAIREControlRigVMResult UAIREAnimationMCPToolset::InspectControlRigVMPins(
	UControlRigBlueprint* ControlRigBlueprint,
	const FString& Filter)
{
	if (!IsAllowedAnimationAsset(ControlRigBlueprint))
	{
		return MakeControlRigVMFailure(
			FString::Printf(
				TEXT("A valid Control Rig Blueprint under %s is required."),
				*AllowedAnimationAssetRoot));
	}

	FAIREControlRigVMResult Result;
	const FString NormalizedFilter = Filter.ToLower();
	for (URigVMGraph* Graph : ControlRigBlueprint->GetAllModels())
	{
		if (!IsValid(Graph))
		{
			continue;
		}

		for (URigVMNode* Node : Graph->GetNodes())
		{
			if (!IsValid(Node))
			{
				continue;
			}

			for (URigVMPin* Pin : Node->GetAllPinsRecursively())
			{
				if (!IsValid(Pin))
				{
					continue;
				}

				const FString Entry = FString::Printf(
					TEXT("graph=%s node=%s title=%s pin=%s type=%s default=%s"),
					*Graph->GetName(),
					*Node->GetNodePath(true),
					*Node->GetNodeTitle(),
					*Pin->GetPinPath(true),
					*Pin->GetCPPType(),
					*Pin->GetDefaultValue());
				if (NormalizedFilter.IsEmpty() || Entry.ToLower().Contains(NormalizedFilter))
				{
					Result.Entries.Add(Entry);
				}
			}
		}
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("Found %d matching Control Rig VM pins."),
		Result.Entries.Num());
	return Result;
}

FAIREControlRigVMResult UAIREAnimationMCPToolset::SetControlRigVMPinDefault(
	UControlRigBlueprint* ControlRigBlueprint,
	const FString& GraphName,
	const FString& PinPath,
	const FString& DefaultValue)
{
	if (!IsAllowedAnimationAsset(ControlRigBlueprint))
	{
		return MakeControlRigVMFailure(
			FString::Printf(
				TEXT("A valid Control Rig Blueprint under %s is required."),
				*AllowedAnimationAssetRoot));
	}

	URigVMGraph* MatchingGraph = nullptr;
	for (URigVMGraph* Graph : ControlRigBlueprint->GetAllModels())
	{
		if (IsValid(Graph) &&
			(Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase) ||
			 Graph->GetNodePath().Equals(GraphName, ESearchCase::IgnoreCase)))
		{
			MatchingGraph = Graph;
			break;
		}
	}
	if (!IsValid(MatchingGraph))
	{
		return MakeControlRigVMFailure(
			FString::Printf(TEXT("Control Rig graph '%s' was not found."), *GraphName));
	}

	URigVMPin* Pin = MatchingGraph->FindPin(PinPath);
	if (!IsValid(Pin))
	{
		return MakeControlRigVMFailure(
			FString::Printf(
				TEXT("Pin '%s' was not found in graph '%s'."),
				*PinPath,
				*MatchingGraph->GetName()));
	}

	URigVMController* Controller = ControlRigBlueprint->GetController(MatchingGraph);
	if (!IsValid(Controller))
	{
		return MakeControlRigVMFailure(TEXT("No RigVM controller is available for the graph."));
	}

	const FString PreviousValue = Pin->GetDefaultValue();
	const FScopedTransaction Transaction(
		LOCTEXT("SetControlRigVMPinDefault", "Set Control Rig VM Pin Default"));
	ControlRigBlueprint->UObject::Modify();
	if (!Controller->SetPinDefaultValue(
		Pin,
		DefaultValue,
		true,
		true,
		false,
		true))
	{
		return MakeControlRigVMFailure(
			FString::Printf(
				TEXT("Failed to set pin '%s' to '%s'."),
				*PinPath,
				*DefaultValue));
	}

	ControlRigBlueprint->GetRigVMAssetInterface()->MarkAssetAsModified();
	ControlRigBlueprint->GetRigVMAssetInterface()->BroadcastRefreshEditor();
	ControlRigBlueprint->MarkPackageDirty();

	FAIREControlRigVMResult Result;
	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("Updated Control Rig pin '%s'. Compile and save the Control Rig explicitly."),
		*PinPath);
	Result.Entries.Add(
		FString::Printf(
			TEXT("graph=%s pin=%s previous=%s current=%s"),
			*MatchingGraph->GetName(),
			*Pin->GetPinPath(true),
			*PreviousValue,
			*Pin->GetDefaultValue()));
	return Result;
}

#undef LOCTEXT_NAMESPACE

#include "AIREAnimationMCPToolset.h"

#include "Animation/AIRECompanionAttackHitAnimNotify.h"
#include "Animation/AIRECompanionComboWindowAnimNotifyState.h"
#include "Animation/AnimMontage.h"
#include "AnimationBlueprintLibrary.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AIREAnimationMCPToolset"

namespace
{
	const FString AllowedAssetRoot = TEXT("/Game/Work/LMK/");

	struct FAIREComboNotifyEntry
	{
		int32 StepIndex = INDEX_NONE;
		float StartTime = 0.0f;
		float Duration = 0.0f;
	};

	FAIREAnimationComboMontageResult MakeFailure(const FString& Message)
	{
		FAIREAnimationComboMontageResult Result;
		Result.Message = Message;
		return Result;
	}

	bool ValidateMontage(const UAnimMontage* Montage, FString& OutError)
	{
		if (!IsValid(Montage))
		{
			OutError = TEXT("A valid AnimMontage is required.");
			return false;
		}

		if (!Montage->GetPathName().StartsWith(AllowedAssetRoot))
		{
			OutError = FString::Printf(
				TEXT("Only montages under %s may be edited by this tool."),
				*AllowedAssetRoot);
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
		return MakeFailure(Error);
	}
	if (SectionNames.Num() < 2)
	{
		return MakeFailure(TEXT("At least two section names are required."));
	}
	if (!FMath::IsFinite(TransitionBias) || TransitionBias <= 0.0f || TransitionBias >= 1.0f)
	{
		return MakeFailure(TEXT("TransitionBias must be finite and between zero and one."));
	}

	TSet<FName> UniqueSectionNames;
	for (const FName SectionName : SectionNames)
	{
		if (SectionName.IsNone() || UniqueSectionNames.Contains(SectionName))
		{
			return MakeFailure(TEXT("Section names must be non-empty and unique."));
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
		return MakeFailure(
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
			return MakeFailure(
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
		return MakeFailure(
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
				return MakeFailure(
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

FAIREAnimationComboMontageResult UAIREAnimationMCPToolset::InspectBasicAttackComboMontage(
	UAnimMontage* Montage)
{
	FString Error;
	if (!ValidateMontage(Montage, Error))
	{
		return MakeFailure(Error);
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
		return MakeFailure(Error);
	}
	if (NotifyTrackName.IsNone())
	{
		return MakeFailure(TEXT("NotifyTrackName must not be None."));
	}
	if (!FMath::IsFinite(WindowStartOffsetAfterHit) || WindowStartOffsetAfterHit < 0.0f)
	{
		return MakeFailure(TEXT("WindowStartOffsetAfterHit must be finite and non-negative."));
	}
	if (!FMath::IsFinite(SectionEndPadding) || SectionEndPadding <= 0.0f)
	{
		return MakeFailure(TEXT("SectionEndPadding must be finite and greater than zero."));
	}

	TArray<FAIREComboNotifyEntry> HitNotifies;
	TArray<FAIREComboNotifyEntry> ComboWindows;
	GatherComboNotifies(*Montage, HitNotifies, ComboWindows);
	if (!ValidateComboLayout(*Montage, HitNotifies, ComboWindows, Error))
	{
		return MakeFailure(Error);
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
			return MakeFailure(
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

#undef LOCTEXT_NAMESPACE

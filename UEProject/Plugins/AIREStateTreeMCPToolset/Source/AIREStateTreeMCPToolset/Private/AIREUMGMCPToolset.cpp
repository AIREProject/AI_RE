#include "AIREUMGMCPToolset.h"

#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "MovieScene.h"
#include "ScopedTransaction.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "AIREUMGMCPToolset"

namespace
{
	constexpr TCHAR EditableContentRoot[] = TEXT("/Game/Work/LMK/");
	constexpr float MinAnimationDurationSeconds = 0.01f;
	constexpr float MaxAnimationDurationSeconds = 60.0f;
	const FFrameRate AnimationDisplayRate(20, 1);
	const FFrameRate AnimationTickResolution(24000, 1);

	FAIREWidgetMutationResult MakeWidgetFailure(const FString& Message)
	{
		FAIREWidgetMutationResult Result;
		Result.Message = Message;
		return Result;
	}

	FAIREWidgetMutationResult MakeWidgetSuccess(const FString& Message)
	{
		FAIREWidgetMutationResult Result;
		Result.bSuccess = true;
		Result.Message = Message;
		return Result;
	}

	bool TryGetEditableWidgetBlueprint(
		UWidgetBlueprint* WidgetBlueprint,
		FString& OutError)
	{
		if (!IsValid(WidgetBlueprint))
		{
			OutError = TEXT("WidgetBlueprint is required.");
			return false;
		}

		const FString PackageName = WidgetBlueprint->GetOutermost()->GetName();
		if (!PackageName.StartsWith(EditableContentRoot))
		{
			OutError = FString::Printf(
				TEXT("Widget Blueprint mutations are restricted to %s"),
				EditableContentRoot);
			return false;
		}
		if (!IsValid(WidgetBlueprint->WidgetTree))
		{
			OutError = TEXT("Widget Blueprint has no WidgetTree.");
			return false;
		}

		OutError.Reset();
		return true;
	}

	UWidgetAnimation* FindAnimation(
		const UWidgetBlueprint& WidgetBlueprint,
		const FName AnimationName)
	{
		for (UWidgetAnimation* Animation : WidgetBlueprint.Animations)
		{
			if (IsValid(Animation)
				&& (Animation->GetFName() == AnimationName
					|| Animation->GetDisplayLabel() == AnimationName.ToString()))
			{
				return Animation;
			}
		}
		return nullptr;
	}

	int32 CountMovieSceneTracks(const UMovieScene& MovieScene)
	{
		int32 TrackCount = MovieScene.GetTracks().Num();
		for (const FMovieSceneBinding& Binding : MovieScene.GetBindings())
		{
			TrackCount += Binding.GetTracks().Num();
		}
		return TrackCount;
	}

	UWidgetAnimation* CreateOrResetAnimation(
		UWidgetBlueprint& WidgetBlueprint,
		const FName AnimationName)
	{
		UWidgetAnimation* Animation = FindAnimation(WidgetBlueprint, AnimationName);
		if (!IsValid(Animation))
		{
			Animation = NewObject<UWidgetAnimation>(
				&WidgetBlueprint,
				AnimationName,
				RF_Transactional);
			WidgetBlueprint.Animations.Add(Animation);
		}

		Animation->Modify();
		Animation->SetDisplayLabel(AnimationName.ToString());
		Animation->AnimationBindings.Reset();
		Animation->MovieScene = NewObject<UMovieScene>(
			Animation,
			NAME_None,
			RF_Transactional);
		Animation->MovieScene->SetDisplayRate(AnimationDisplayRate);
		Animation->MovieScene->SetTickResolutionDirectly(AnimationTickResolution);
		return Animation;
	}

	void AddOpacityTrack(
		UMovieScene& MovieScene,
		const FGuid& WidgetGuid,
		const FFrameNumber StartFrame,
		const FFrameNumber EndFrame,
		const float StartOpacity,
		const float EndOpacity)
	{
		UMovieSceneFloatTrack* Track =
			MovieScene.AddTrack<UMovieSceneFloatTrack>(WidgetGuid);
		check(Track);
		Track->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));

		UMovieSceneFloatSection* Section =
			CastChecked<UMovieSceneFloatSection>(Track->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>(StartFrame, EndFrame + 1));
		Track->AddSection(*Section);

		FMovieSceneFloatChannel* Channel =
			Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
		check(Channel);
		Channel->AddCubicKey(StartFrame, StartOpacity);
		Channel->AddCubicKey(EndFrame, EndOpacity);
	}

	void AddTranslationTrack(
		UMovieScene& MovieScene,
		const FGuid& WidgetGuid,
		const FFrameNumber StartFrame,
		const FFrameNumber EndFrame,
		const float StartTranslationX,
		const float EndTranslationX)
	{
		UMovieScene2DTransformTrack* Track =
			MovieScene.AddTrack<UMovieScene2DTransformTrack>(WidgetGuid);
		check(Track);
		Track->SetPropertyNameAndPath(TEXT("RenderTransform"), TEXT("RenderTransform"));

		UMovieScene2DTransformSection* Section =
			CastChecked<UMovieScene2DTransformSection>(Track->CreateNewSection());
		Section->SetMask(
			FMovieScene2DTransformMask(
				EMovieScene2DTransformChannel::TranslationX));
		Section->SetRange(TRange<FFrameNumber>(StartFrame, EndFrame + 1));
		Track->AddSection(*Section);
		Section->Translation[0].AddCubicKey(StartFrame, StartTranslationX);
		Section->Translation[0].AddCubicKey(EndFrame, EndTranslationX);
		Section->Translation[1].SetDefault(0.0f);
	}
}

FAIREWidgetBlueprintInspection UAIREUMGMCPToolset::InspectWidgetBlueprint(
	UWidgetBlueprint* WidgetBlueprint)
{
	FAIREWidgetBlueprintInspection Result;
	if (!IsValid(WidgetBlueprint) || !IsValid(WidgetBlueprint->WidgetTree))
	{
		Result.Message = TEXT("A valid Widget Blueprint with a WidgetTree is required.");
		return Result;
	}

	Result.AssetPath = WidgetBlueprint->GetPathName();
	WidgetBlueprint->WidgetTree->ForEachWidget(
		[&Result](UWidget*)
		{
			++Result.WidgetCount;
		});

	for (const UWidgetAnimation* Animation : WidgetBlueprint->Animations)
	{
		if (!IsValid(Animation) || !IsValid(Animation->MovieScene))
		{
			continue;
		}

		FAIREWidgetAnimationInfo& Info = Result.Animations.AddDefaulted_GetRef();
		Info.AnimationName = Animation->GetFName();
		Info.DurationSeconds = Animation->GetEndTime() - Animation->GetStartTime();
		Info.TrackCount = CountMovieSceneTracks(*Animation->MovieScene);
		if (!Animation->AnimationBindings.IsEmpty())
		{
			Info.WidgetName = Animation->AnimationBindings[0].WidgetName;
		}
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("Found %d widgets and %d animations."),
		Result.WidgetCount,
		Result.Animations.Num());
	return Result;
}

FAIREWidgetMutationResult UAIREUMGMCPToolset::CreateOrReplaceSlideFadeAnimation(
	UWidgetBlueprint* WidgetBlueprint,
	const FName AnimationName,
	const FName WidgetName,
	const float DurationSeconds,
	const float StartTranslationX,
	const float EndTranslationX,
	const float StartOpacity,
	const float EndOpacity)
{
	FString Error;
	if (!TryGetEditableWidgetBlueprint(WidgetBlueprint, Error))
	{
		return MakeWidgetFailure(Error);
	}
	if (AnimationName.IsNone() || WidgetName.IsNone())
	{
		return MakeWidgetFailure(TEXT("AnimationName and WidgetName are required."));
	}
	if (!FMath::IsFinite(DurationSeconds)
		|| DurationSeconds < MinAnimationDurationSeconds
		|| DurationSeconds > MaxAnimationDurationSeconds)
	{
		return MakeWidgetFailure(TEXT("DurationSeconds must be finite and between 0.01 and 60."));
	}
	if (!FMath::IsFinite(StartTranslationX)
		|| !FMath::IsFinite(EndTranslationX)
		|| !FMath::IsFinite(StartOpacity)
		|| !FMath::IsFinite(EndOpacity)
		|| StartOpacity < 0.0f
		|| StartOpacity > 1.0f
		|| EndOpacity < 0.0f
		|| EndOpacity > 1.0f)
	{
		return MakeWidgetFailure(TEXT("Translation values must be finite and opacity must be between 0 and 1."));
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(WidgetName);
	if (!IsValid(Widget))
	{
		return MakeWidgetFailure(
			FString::Printf(TEXT("Widget '%s' was not found."), *WidgetName.ToString()));
	}

	const FScopedTransaction Transaction(
		LOCTEXT("CreateOrReplaceSlideFadeAnimation", "Create or Replace AIRE Widget Animation"));
	WidgetBlueprint->Modify();

	UWidgetAnimation* Animation =
		CreateOrResetAnimation(*WidgetBlueprint, AnimationName);
	UMovieScene* MovieScene = Animation->MovieScene;
	check(MovieScene);

	const FFrameNumber StartFrame(0);
	const FFrameNumber EndFrame =
		AnimationTickResolution.AsFrameTime(DurationSeconds).RoundToFrame();
	MovieScene->SetPlaybackRange(StartFrame, EndFrame.Value + 1);

	const FGuid WidgetGuid =
		MovieScene->AddPossessable(Widget->GetName(), Widget->GetClass());
	FWidgetAnimationBinding& Binding =
		Animation->AnimationBindings.AddDefaulted_GetRef();
	Binding.WidgetName = WidgetName;
	Binding.AnimationGuid = WidgetGuid;
	Binding.bIsRootWidget = Widget == WidgetBlueprint->WidgetTree->RootWidget;

	AddOpacityTrack(
		*MovieScene,
		WidgetGuid,
		StartFrame,
		EndFrame,
		StartOpacity,
		EndOpacity);
	AddTranslationTrack(
		*MovieScene,
		WidgetGuid,
		StartFrame,
		EndFrame,
		StartTranslationX,
		EndTranslationX);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return MakeWidgetSuccess(
		FString::Printf(
			TEXT("Animation '%s' now targets '%s'. Compile and save explicitly."),
			*AnimationName.ToString(),
			*WidgetName.ToString()));
}

FAIREWidgetMutationResult UAIREUMGMCPToolset::RemoveWidgetAnimation(
	UWidgetBlueprint* WidgetBlueprint,
	const FName AnimationName)
{
	FString Error;
	if (!TryGetEditableWidgetBlueprint(WidgetBlueprint, Error))
	{
		return MakeWidgetFailure(Error);
	}

	UWidgetAnimation* Animation = FindAnimation(*WidgetBlueprint, AnimationName);
	if (!IsValid(Animation))
	{
		return MakeWidgetFailure(
			FString::Printf(TEXT("Animation '%s' was not found."), *AnimationName.ToString()));
	}

	const FScopedTransaction Transaction(
		LOCTEXT("RemoveWidgetAnimation", "Remove AIRE Widget Animation"));
	WidgetBlueprint->Modify();
	Animation->Modify();
	WidgetBlueprint->Animations.Remove(Animation);
	Animation->Rename(
		nullptr,
		GetTransientPackage(),
		REN_DontCreateRedirectors | REN_NonTransactional);
	Animation->MarkAsGarbage();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return MakeWidgetSuccess(
		FString::Printf(
			TEXT("Animation '%s' was removed. Compile and save explicitly."),
			*AnimationName.ToString()));
}

FAIREWidgetMutationResult UAIREUMGMCPToolset::ValidateAndCompileWidgetBlueprint(
	UWidgetBlueprint* WidgetBlueprint)
{
	FString Error;
	if (!TryGetEditableWidgetBlueprint(WidgetBlueprint, Error))
	{
		return MakeWidgetFailure(Error);
	}

	for (const UWidgetAnimation* Animation : WidgetBlueprint->Animations)
	{
		if (!IsValid(Animation) || !IsValid(Animation->MovieScene))
		{
			return MakeWidgetFailure(TEXT("Widget Blueprint contains an invalid animation."));
		}
		for (const FWidgetAnimationBinding& Binding : Animation->AnimationBindings)
		{
			if (!IsValid(WidgetBlueprint->WidgetTree->FindWidget(Binding.WidgetName)))
			{
				return MakeWidgetFailure(
					FString::Printf(
						TEXT("Animation '%s' references missing widget '%s'."),
						*Animation->GetName(),
						*Binding.WidgetName.ToString()));
			}
		}
	}

	FCompilerResultsLog CompilerLog;
	FKismetEditorUtilities::CompileBlueprint(
		WidgetBlueprint,
		EBlueprintCompileOptions::SkipSave,
		&CompilerLog);

	FAIREWidgetMutationResult Result;
	Result.bSuccess =
		CompilerLog.NumErrors == 0 && WidgetBlueprint->Status != BS_Error;
	for (const TSharedRef<FTokenizedMessage>& Message : CompilerLog.Messages)
	{
		Result.CompilerMessages.Add(Message->ToText().ToString());
	}
	Result.Message = Result.bSuccess
		? TEXT("Widget Blueprint compiled successfully.")
		: TEXT("Widget Blueprint compilation failed.");
	return Result;
}

FAIREWidgetMutationResult UAIREUMGMCPToolset::SaveWidgetBlueprint(
	UWidgetBlueprint* WidgetBlueprint)
{
	FString Error;
	if (!TryGetEditableWidgetBlueprint(WidgetBlueprint, Error))
	{
		return MakeWidgetFailure(Error);
	}
	if (WidgetBlueprint->Status == BS_Dirty || WidgetBlueprint->Status == BS_Error)
	{
		return MakeWidgetFailure(
			TEXT("Widget Blueprint requires a successful compile before saving."));
	}
	if (GEditor == nullptr)
	{
		return MakeWidgetFailure(TEXT("Unreal Editor is unavailable."));
	}

	UEditorAssetSubsystem* AssetSubsystem =
		GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (!IsValid(AssetSubsystem)
		|| !AssetSubsystem->SaveLoadedAsset(WidgetBlueprint, true))
	{
		return MakeWidgetFailure(TEXT("Widget Blueprint could not be saved."));
	}
	return MakeWidgetSuccess(TEXT("Widget Blueprint saved successfully."));
}

#undef LOCTEXT_NAMESPACE

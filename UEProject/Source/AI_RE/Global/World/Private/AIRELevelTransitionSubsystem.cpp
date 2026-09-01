#include "AIRELevelTransitionSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "MoviePlayer.h"

namespace
{
constexpr TCHAR LoadingWidgetClassPath[] =
	TEXT("/Game/Work/Global/UI/Loading/WBP_AIRELoadingScreen.WBP_AIRELoadingScreen_C");
}

void UAIRELevelTransitionSubsystem::Deinitialize()
{
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle.Reset();
	}
	if (TravelTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TravelTickerHandle);
		TravelTickerHandle.Reset();
	}
	LoadingWidget = nullptr;
	PendingWorldContext.Reset();
	PendingLevelPackage = NAME_None;
	bTravelRequested = false;
	Super::Deinitialize();
}

void UAIRELevelTransitionSubsystem::PreloadLevel(FName LevelPackageName)
{
	if (LevelPackageName.IsNone() || bTravelRequested
		|| (PreloadedLevelPackage == LevelPackageName
			&& PreloadHandle.IsValid()))
	{
		return;
	}
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
	}
	PreloadedLevelPackage = LevelPackageName;
	PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		MakeWorldPath(LevelPackageName),
		FStreamableDelegate(),
		FStreamableManager::AsyncLoadHighPriority);
}

void UAIRELevelTransitionSubsystem::CancelPreload(FName LevelPackageName)
{
	if (bTravelRequested || PreloadedLevelPackage != LevelPackageName)
	{
		return;
	}
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle.Reset();
	}
	PreloadedLevelPackage = NAME_None;
}

bool UAIRELevelTransitionSubsystem::RequestTravel(
	UObject* WorldContextObject,
	FName LevelPackageName)
{
	if (!IsValid(WorldContextObject) || LevelPackageName.IsNone()
		|| bTravelRequested)
	{
		return false;
	}

	bTravelRequested = true;
	PendingWorldContext = WorldContextObject;
	PendingLevelPackage = LevelPackageName;
	PrepareLoadingScreen();
	TravelTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UAIRELevelTransitionSubsystem::ExecutePendingTravel));
	return true;
}

bool UAIRELevelTransitionSubsystem::ExecutePendingTravel(const float DeltaSeconds)
{
	(void)DeltaSeconds;
	UObject* WorldContextObject = PendingWorldContext.Get();
	const FName LevelPackageName = PendingLevelPackage;
	TravelTickerHandle.Reset();
	PendingWorldContext.Reset();
	PendingLevelPackage = NAME_None;
	bTravelRequested = false;
	if (IsValid(WorldContextObject) && !LevelPackageName.IsNone())
	{
		UGameplayStatics::OpenLevel(WorldContextObject, LevelPackageName);
	}
	return false;
}

FSoftObjectPath UAIRELevelTransitionSubsystem::MakeWorldPath(
	FName LevelPackageName) const
{
	const FString PackageName = LevelPackageName.ToString();
	const FString AssetName = FPackageName::GetShortName(PackageName);
	return FSoftObjectPath(
		FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName));
}

void UAIRELevelTransitionSubsystem::PrepareLoadingScreen()
{
	UClass* LoadingClass = LoadClass<UUserWidget>(nullptr, LoadingWidgetClassPath);
	if (!IsValid(LoadingClass) || !IsValid(GetGameInstance()))
	{
		return;
	}

	LoadingWidget = CreateWidget<UUserWidget>(GetGameInstance(), LoadingClass);
	if (!IsValid(LoadingWidget))
	{
		return;
	}

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bWaitForManualStop = false;
	Attributes.MinimumLoadingScreenDisplayTime = 0.1f;
	Attributes.WidgetLoadingScreen = LoadingWidget->TakeWidget();
	GetMoviePlayer()->SetupLoadingScreen(Attributes);
}

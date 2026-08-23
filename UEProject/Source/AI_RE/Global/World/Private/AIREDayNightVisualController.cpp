#include "AIREDayNightVisualController.h"

#include "AIREWorldTimeMath.h"
#include "AIREWorldTimeSubsystem.h"
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"

AAIREDayNightVisualController::AAIREDayNightVisualController()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AAIREDayNightVisualController::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bHasTargetLighting)
	{
		return;
	}

	if (SunLight != nullptr)
	{
		SunLight->SetActorRotation(FMath::RInterpTo(
			SunLight->GetActorRotation(),
			TargetSunRotation,
			DeltaTime,
			RotationInterpolationSpeed));
		if (ULightComponent* SunComponent = SunLight->GetLightComponent())
		{
			SunComponent->SetIntensity(FMath::FInterpTo(
				SunComponent->Intensity,
				TargetSunIntensity,
				DeltaTime,
				LightInterpolationSpeed));
			SunComponent->SetLightColor(FLinearColor::LerpUsingHSV(
				SunComponent->GetLightColor(),
				TargetSunColor,
				FMath::Clamp(
					DeltaTime * LightInterpolationSpeed,
					0.0f,
					1.0f)));
		}
	}

	if (MoonLight != nullptr)
	{
		MoonLight->SetActorRotation(FMath::RInterpTo(
			MoonLight->GetActorRotation(),
			TargetMoonRotation,
			DeltaTime,
			RotationInterpolationSpeed));
		if (ULightComponent* MoonComponent = MoonLight->GetLightComponent())
		{
			MoonComponent->SetIntensity(FMath::FInterpTo(
				MoonComponent->Intensity,
				TargetMoonIntensity,
				DeltaTime,
				LightInterpolationSpeed));
			MoonComponent->SetLightColor(FLinearColor::LerpUsingHSV(
				MoonComponent->GetLightColor(),
				TargetMoonColor,
				FMath::Clamp(
					DeltaTime * LightInterpolationSpeed,
					0.0f,
					1.0f)));
		}
	}

	if (SkyLight != nullptr)
	{
		if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
		{
			CurrentSkyLightIntensity = FMath::FInterpTo(
				CurrentSkyLightIntensity,
				TargetSkyLightIntensity,
				DeltaTime,
				LightInterpolationSpeed);
			CurrentSkyLightColor = FLinearColor::LerpUsingHSV(
				CurrentSkyLightColor,
				TargetSkyLightColor,
				FMath::Clamp(
					DeltaTime * LightInterpolationSpeed,
					0.0f,
					1.0f));
			SkyComponent->SetIntensity(CurrentSkyLightIntensity);
			SkyComponent->SetLightColor(CurrentSkyLightColor);
		}
	}

}

void AAIREDayNightVisualController::BeginPlay()
{
	Super::BeginPlay();
	BindWorldTime();
}

void AAIREDayNightVisualController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	UnbindWorldTime();
	Super::EndPlay(EndPlayReason);
}

void AAIREDayNightVisualController::HandleWorldTimeChanged(
	const int32 CurrentDay,
	const float CurrentHour)
{
	(void)CurrentDay;
	SetTargetLighting(CurrentHour);
}

void AAIREDayNightVisualController::BindWorldTime()
{
	UWorld* World = GetWorld();
	CachedWorldTime = World != nullptr
		? World->GetSubsystem<UAIREWorldTimeSubsystem>()
		: nullptr;
	if (CachedWorldTime == nullptr)
	{
		return;
	}

	CachedWorldTime->OnWorldTimeChanged.AddDynamic(
		this,
		&AAIREDayNightVisualController::HandleWorldTimeChanged);
	SetTargetLighting(CachedWorldTime->GetCurrentTimeOfDay());
}

void AAIREDayNightVisualController::UnbindWorldTime()
{
	if (CachedWorldTime != nullptr)
	{
		CachedWorldTime->OnWorldTimeChanged.RemoveDynamic(
			this,
			&AAIREDayNightVisualController::HandleWorldTimeChanged);
	}
	CachedWorldTime = nullptr;
}

void AAIREDayNightVisualController::SetTargetLighting(
	const float CurrentHour)
{
	if (SunLight == nullptr && MoonLight == nullptr && SkyLight == nullptr)
	{
		return;
	}

	const float DayAlpha = AIREWorldTime::CalculateDayAlpha(CurrentHour);
	TargetSunRotation = FRotator(
		AIREWorldTime::CalculateSunPitch(
			CurrentHour,
			SunrisePitch,
			NoonPitch,
			SunsetPitch,
			MidnightPitch),
		SunYaw,
		0.0f);
	TargetMoonRotation = FRotator(
		AIREWorldTime::CalculateMoonPitch(
			CurrentHour,
			MoonHorizonPitch,
			MoonMidnightPitch,
			MidnightPitch),
		MoonYaw,
		0.0f);
	TargetSunIntensity = FMath::Lerp(
		NightSunIntensity,
		DaySunIntensity,
		DayAlpha);
	TargetMoonIntensity = FMath::Lerp(
		NightMoonIntensity,
		DayMoonIntensity,
		DayAlpha);
	TargetSkyLightIntensity = FMath::Lerp(
		NightSkyLightIntensity,
		DaySkyLightIntensity,
		DayAlpha);
	TargetSunColor = FLinearColor::LerpUsingHSV(
		NightSunColor,
		DaySunColor,
		DayAlpha);
	TargetMoonColor = FLinearColor::LerpUsingHSV(
		NightMoonColor,
		DayMoonColor,
		DayAlpha);
	TargetSkyLightColor = FLinearColor::LerpUsingHSV(
		NightSkyLightColor,
		DaySkyLightColor,
		DayAlpha);
	bHasTargetLighting = true;

	if (bSnapOnFirstUpdate && !bHasAppliedInitialLighting)
	{
		ApplyLightingInstantly();
		bHasAppliedInitialLighting = true;
	}
}

void AAIREDayNightVisualController::ApplyLightingInstantly()
{
	if (SunLight != nullptr)
	{
		SunLight->SetActorRotation(TargetSunRotation);
		if (ULightComponent* SunComponent = SunLight->GetLightComponent())
		{
			SunComponent->SetIntensity(TargetSunIntensity);
			SunComponent->SetLightColor(TargetSunColor);
		}
	}

	if (MoonLight != nullptr)
	{
		MoonLight->SetActorRotation(TargetMoonRotation);
		if (ULightComponent* MoonComponent = MoonLight->GetLightComponent())
		{
			MoonComponent->SetIntensity(TargetMoonIntensity);
			MoonComponent->SetLightColor(TargetMoonColor);
		}
	}

	if (SkyLight != nullptr)
	{
		if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
		{
			CurrentSkyLightIntensity = TargetSkyLightIntensity;
			CurrentSkyLightColor = TargetSkyLightColor;
			SkyComponent->SetIntensity(TargetSkyLightIntensity);
			SkyComponent->SetLightColor(TargetSkyLightColor);
		}
	}

}

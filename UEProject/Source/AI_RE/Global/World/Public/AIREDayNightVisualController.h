#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIREDayNightVisualController.generated.h"

class ADirectionalLight;
class ASkyLight;
class UAIREWorldTimeSubsystem;
class USceneComponent;

UCLASS()
class AI_RE_API AAIREDayNightVisualController : public AActor
{
	GENERATED_BODY()

public:
	AAIREDayNightVisualController();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AIRE|Day Night")
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AIRE|Day Night")
	TObjectPtr<ADirectionalLight> MoonLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AIRE|Day Night")
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	float SunrisePitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	float NoonPitch = -90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	float SunsetPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	float MidnightPitch = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	float SunYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	float MoonYaw = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	float MoonHorizonPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	float MoonMidnightPitch = -35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night", meta = (ClampMin = "0.0"))
	float DaySunIntensity = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night", meta = (ClampMin = "0.0"))
	float NightSunIntensity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night", meta = (ClampMin = "0.0"))
	float DayMoonIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night", meta = (ClampMin = "0.0"))
	float NightMoonIntensity = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night", meta = (ClampMin = "0.0"))
	float DaySkyLightIntensity = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night", meta = (ClampMin = "0.0"))
	float NightSkyLightIntensity = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	FLinearColor DaySunColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	FLinearColor NightSunColor = FLinearColor(0.35f, 0.45f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	FLinearColor DayMoonColor = FLinearColor(0.3f, 0.35f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	FLinearColor NightMoonColor = FLinearColor(0.55f, 0.65f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	FLinearColor DaySkyLightColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	FLinearColor NightSkyLightColor = FLinearColor(0.08f, 0.12f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night", meta = (ClampMin = "0.1"))
	float RotationInterpolationSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night", meta = (ClampMin = "0.1"))
	float LightInterpolationSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AIRE|Day Night")
	bool bSnapOnFirstUpdate = true;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAIREWorldTimeSubsystem> CachedWorldTime;

	FRotator TargetSunRotation = FRotator::ZeroRotator;
	FRotator TargetMoonRotation = FRotator::ZeroRotator;
	float TargetSunIntensity = 0.0f;
	float TargetMoonIntensity = 0.0f;
	float TargetSkyLightIntensity = 0.0f;
	float CurrentSkyLightIntensity = 0.0f;
	FLinearColor TargetSunColor = FLinearColor::White;
	FLinearColor TargetMoonColor = FLinearColor::White;
	FLinearColor TargetSkyLightColor = FLinearColor::White;
	FLinearColor CurrentSkyLightColor = FLinearColor::White;
	bool bHasTargetLighting = false;
	bool bHasAppliedInitialLighting = false;

	UFUNCTION()
	void HandleWorldTimeChanged(int32 CurrentDay, float CurrentHour);

	void BindWorldTime();
	void UnbindWorldTime();
	void SetTargetLighting(float CurrentHour);
	void ApplyLightingInstantly();
};

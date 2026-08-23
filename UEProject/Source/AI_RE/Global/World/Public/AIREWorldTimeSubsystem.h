#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIREWorldTimeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAIREWorldTimeChanged,
	int32,
	CurrentDay,
	float,
	CurrentHour);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIREWorldDayNightChanged,
	bool,
	bIsNight);

UCLASS()
class AI_RE_API UAIREWorldTimeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "AIRE|World Time")
	int32 GetCurrentDay() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|World Time")
	float GetCurrentTimeOfDay() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|World Time")
	bool IsDaytime() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|World Time")
	bool IsNight() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|World Time|Testing")
	bool IsTimePaused() const;

	UFUNCTION(BlueprintCallable, Category = "AIRE|World Time|Testing")
	void ToggleDayNight();

	UFUNCTION(BlueprintCallable, Category = "AIRE|World Time|Testing")
	void SetTimeOfDay(float NewHour);

	UFUNCTION(BlueprintCallable, Category = "AIRE|World Time|Testing")
	void ToggleTimePause();

	UPROPERTY(BlueprintAssignable, Category = "AIRE|World Time")
	FAIREWorldTimeChanged OnWorldTimeChanged;

	UPROPERTY(BlueprintAssignable, Category = "AIRE|World Time")
	FAIREWorldDayNightChanged OnWorldDayNightChanged;

private:
	FTimerHandle TimeUpdateTimerHandle;
	float CurrentTimeOfDay = 9.0f;
	int32 CurrentDay = 1;
	bool bTimePaused = false;

	void UpdateWorldTime();
	void AdvanceTime(float RealDeltaSeconds);
	void BroadcastTimeChange(bool bPreviousIsNight);
};

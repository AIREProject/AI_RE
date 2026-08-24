#include "AIREWorldTimeSubsystem.h"

#include "AIREWorldTimeMath.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIREWorldTime, Log, All);

namespace
{
	bool IsDecimalDigits(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}

		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			if (!FChar::IsDigit(Value[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool TryParse24HourClock(const FString& ClockText, float& OutHour)
	{
		TArray<FString> Parts;
		if (ClockText.ParseIntoArray(Parts, TEXT(":"), false) != 2)
		{
			return false;
		}

		if (!IsDecimalDigits(Parts[0]) || !IsDecimalDigits(Parts[1]))
		{
			return false;
		}

		const int32 Hour = FCString::Atoi(*Parts[0]);
		const int32 Minute = FCString::Atoi(*Parts[1]);
		if (Hour < 0
			|| Hour > 23
			|| Minute < 0
			|| Minute > 59)
		{
			return false;
		}

		OutHour = static_cast<float>(Hour)
			+ static_cast<float>(Minute) / 60.0f;
		return true;
	}

	void AIREToggleDayNightConsoleCommand(
		const TArray<FString>& Arguments,
		UWorld* World)
	{
		(void)Arguments;
		if (World == nullptr)
		{
			return;
		}

		if (UAIREWorldTimeSubsystem* WorldTime =
			World->GetSubsystem<UAIREWorldTimeSubsystem>())
		{
			WorldTime->ToggleDayNight();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GToggleDayNightCommand(
		TEXT("aire.Time.ToggleDayNight"),
		TEXT("Toggles AIRE world time between 12:00 and 00:00."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&AIREToggleDayNightConsoleCommand));

	void AIRESetHourConsoleCommand(
		const TArray<FString>& Arguments,
		UWorld* World)
	{
		if (World == nullptr
			|| Arguments.Num() != 1
			|| !Arguments[0].IsNumeric())
		{
			UE_LOG(
				LogAIREWorldTime,
				Warning,
				TEXT("Usage: aire.Time.SetHour <hour>"));
			return;
		}
		const float RequestedHour = FCString::Atof(*Arguments[0]);

		if (UAIREWorldTimeSubsystem* WorldTime =
			World->GetSubsystem<UAIREWorldTimeSubsystem>())
		{
			WorldTime->SetTimeOfDay(RequestedHour);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GSetHourCommand(
		TEXT("aire.Time.SetHour"),
		TEXT("Sets AIRE world time. Example: aire.Time.SetHour 5.5"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&AIRESetHourConsoleCommand));

	void AIRESetTimeConsoleCommand(
		const TArray<FString>& Arguments,
		UWorld* World)
	{
		float RequestedHour = 0.0f;
		if (World == nullptr
			|| Arguments.Num() != 1
			|| !TryParse24HourClock(Arguments[0], RequestedHour))
		{
			UE_LOG(
				LogAIREWorldTime,
				Warning,
				TEXT("Usage: aire.Time.SetTime HH:MM (00:00-23:59)"));
			return;
		}

		if (UAIREWorldTimeSubsystem* WorldTime =
			World->GetSubsystem<UAIREWorldTimeSubsystem>())
		{
			WorldTime->SetTimeOfDay(RequestedHour);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GSetTimeCommand(
		TEXT("aire.Time.SetTime"),
		TEXT("Sets AIRE world time in 24-hour HH:MM format."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&AIRESetTimeConsoleCommand));

	void AIRETogglePauseConsoleCommand(
		const TArray<FString>& Arguments,
		UWorld* World)
	{
		(void)Arguments;
		if (World == nullptr)
		{
			return;
		}

		if (UAIREWorldTimeSubsystem* WorldTime =
			World->GetSubsystem<UAIREWorldTimeSubsystem>())
		{
			WorldTime->ToggleTimePause();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GToggleTimePauseCommand(
		TEXT("aire.Time.TogglePause"),
		TEXT("Pauses or resumes AIRE world-time progression."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&AIRETogglePauseConsoleCommand));
}

void UAIREWorldTimeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	CurrentDay = AIREWorldTime::InitialDay;
	CurrentTimeOfDay = AIREWorldTime::InitialHour;
	bTimePaused = false;
	InWorld.GetTimerManager().SetTimer(
		TimeUpdateTimerHandle,
		this,
		&UAIREWorldTimeSubsystem::UpdateWorldTime,
		AIREWorldTime::TimeUpdateIntervalSeconds,
		true);
	OnWorldTimeChanged.Broadcast(CurrentDay, CurrentTimeOfDay);
}

void UAIREWorldTimeSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeUpdateTimerHandle);
	}

	Super::Deinitialize();
}

int32 UAIREWorldTimeSubsystem::GetCurrentDay() const
{
	return CurrentDay;
}

float UAIREWorldTimeSubsystem::GetCurrentTimeOfDay() const
{
	return CurrentTimeOfDay;
}

bool UAIREWorldTimeSubsystem::IsDaytime() const
{
	return !IsNight();
}

bool UAIREWorldTimeSubsystem::IsNight() const
{
	return AIREWorldTime::IsNightAtHour(CurrentTimeOfDay);
}

bool UAIREWorldTimeSubsystem::IsTimePaused() const
{
	return bTimePaused;
}

void UAIREWorldTimeSubsystem::ToggleDayNight()
{
	const bool bPreviousIsNight = IsNight();
	CurrentTimeOfDay = bPreviousIsNight ? 12.0f : 0.0f;
	BroadcastTimeChange(bPreviousIsNight);

	UE_LOG(
		LogAIREWorldTime,
		Display,
		TEXT("Toggled day/night. Day=%d Hour=%.2f"),
		CurrentDay,
		CurrentTimeOfDay);
}

void UAIREWorldTimeSubsystem::SetTimeOfDay(const float NewHour)
{
	const bool bPreviousIsNight = IsNight();
	CurrentTimeOfDay = AIREWorldTime::NormalizeHour(NewHour);
	BroadcastTimeChange(bPreviousIsNight);
	const int32 TotalMinutes = FMath::RoundToInt(CurrentTimeOfDay * 60.0f)
		% (24 * 60);

	UE_LOG(
		LogAIREWorldTime,
		Display,
		TEXT("Set world time. Day=%d Time=%02d:%02d"),
		CurrentDay,
		TotalMinutes / 60,
		TotalMinutes % 60);
}

void UAIREWorldTimeSubsystem::ToggleTimePause()
{
	bTimePaused = !bTimePaused;
	UE_LOG(
		LogAIREWorldTime,
		Display,
		TEXT("World time progression %s."),
		bTimePaused ? TEXT("paused") : TEXT("resumed"));
}

void UAIREWorldTimeSubsystem::UpdateWorldTime()
{
	if (bTimePaused)
	{
		return;
	}
	AdvanceTime(AIREWorldTime::TimeUpdateIntervalSeconds);
}

void UAIREWorldTimeSubsystem::AdvanceTime(const float RealDeltaSeconds)
{
	const bool bPreviousIsNight = IsNight();
	AIREWorldTime::Advance(
		CurrentDay,
		CurrentTimeOfDay,
		RealDeltaSeconds,
		AIREWorldTime::RealSecondsPerGameDay);
	BroadcastTimeChange(bPreviousIsNight);
}

void UAIREWorldTimeSubsystem::BroadcastTimeChange(
	const bool bPreviousIsNight)
{
	OnWorldTimeChanged.Broadcast(CurrentDay, CurrentTimeOfDay);
	const bool bCurrentIsNight = IsNight();
	if (bCurrentIsNight != bPreviousIsNight)
	{
		OnWorldDayNightChanged.Broadcast(bCurrentIsNight);
	}
}

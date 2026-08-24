#pragma once

#include "CoreMinimal.h"

namespace AIREWorldTime
{
	constexpr float RealSecondsPerGameDay = 1200.0f;
	constexpr float TimeUpdateIntervalSeconds = 1.0f;
	constexpr float InitialHour = 9.0f;
	constexpr int32 InitialDay = 1;
	constexpr float DayStartHour = 5.0f;
	constexpr float NightStartHour = 23.0f;

	inline float NormalizeHour(const float Hour)
	{
		float NormalizedHour = FMath::Fmod(Hour, 24.0f);
		if (NormalizedHour < 0.0f)
		{
			NormalizedHour += 24.0f;
		}
		return NormalizedHour;
	}

	inline bool IsNightAtHour(const float Hour)
	{
		const float NormalizedHour = NormalizeHour(Hour);
		return NormalizedHour >= NightStartHour
			|| NormalizedHour < DayStartHour;
	}

	inline void Advance(
		int32& InOutDay,
		float& InOutHour,
		const float RealDeltaSeconds,
		const float CycleLengthSeconds)
	{
		const float HoursPerRealSecond =
			24.0f / FMath::Max(CycleLengthSeconds, 1.0f);
		InOutHour += FMath::Max(RealDeltaSeconds, 0.0f)
			* HoursPerRealSecond;
		while (InOutHour >= 24.0f)
		{
			InOutHour -= 24.0f;
			++InOutDay;
		}
		InOutDay = FMath::Max(InitialDay, InOutDay);
	}

	inline float CalculateDayAlpha(const float Hour)
	{
		const float NormalizedHour = NormalizeHour(Hour);
		if (NormalizedHour < 4.0f)
		{
			return 0.0f;
		}
		if (NormalizedHour < 6.0f)
		{
			return (NormalizedHour - 4.0f) / 2.0f;
		}
		if (NormalizedHour < 22.0f)
		{
			return 1.0f;
		}
		return 1.0f - ((NormalizedHour - 22.0f) / 2.0f);
	}

	inline float CalculateSunPitch(
		const float Hour,
		const float SunrisePitch,
		const float NoonPitch,
		const float SunsetPitch,
		const float MidnightPitch)
	{
		const float NormalizedHour = NormalizeHour(Hour);
		if (NormalizedHour < 2.0f)
		{
			return FMath::Lerp(
				SunsetPitch,
				MidnightPitch,
				(NormalizedHour + 1.0f) / 3.0f);
		}
		if (NormalizedHour < 5.0f)
		{
			return FMath::Lerp(
				MidnightPitch,
				SunrisePitch,
				(NormalizedHour - 2.0f) / 3.0f);
		}
		if (NormalizedHour < 14.0f)
		{
			return FMath::Lerp(
				SunrisePitch,
				NoonPitch,
				(NormalizedHour - 5.0f) / 9.0f);
		}
		if (NormalizedHour < 23.0f)
		{
			return FMath::Lerp(
				NoonPitch,
				SunsetPitch,
				(NormalizedHour - 14.0f) / 9.0f);
		}
		return FMath::Lerp(
			SunsetPitch,
			MidnightPitch,
			(NormalizedHour - 23.0f) / 3.0f);
	}

	inline float CalculateMoonPitch(
		const float Hour,
		const float HorizonPitch,
		const float MidnightPitch,
		const float DayPitch)
	{
		const float NormalizedHour = NormalizeHour(Hour);
		if (NormalizedHour < 2.0f)
		{
			return FMath::Lerp(
				HorizonPitch,
				MidnightPitch,
				(NormalizedHour + 1.0f) / 3.0f);
		}
		if (NormalizedHour < 5.0f)
		{
			return FMath::Lerp(
				MidnightPitch,
				HorizonPitch,
				(NormalizedHour - 2.0f) / 3.0f);
		}
		if (NormalizedHour >= 23.0f)
		{
			return FMath::Lerp(
				HorizonPitch,
				MidnightPitch,
				(NormalizedHour - 23.0f) / 3.0f);
		}
		return DayPitch;
	}
}

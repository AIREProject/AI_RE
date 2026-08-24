#if WITH_DEV_AUTOMATION_TESTS

#include "AIREWorldTimeMath.h"
#include "AIREWorldTimeSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREWorldTimeProgressionTest,
	"AIRE.World.Time.Progression",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIREWorldTimeProgressionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	int32 Day = AIREWorldTime::InitialDay;
	float Hour = AIREWorldTime::InitialHour;
	AIREWorldTime::Advance(
		Day,
		Hour,
		60.0f,
		AIREWorldTime::RealSecondsPerGameDay);
	TestEqual(TEXT("Sixty real seconds preserve the current day"), Day, 1);
	TestTrue(
		TEXT("Sixty real seconds advance 1.2 game hours"),
		FMath::IsNearlyEqual(Hour, 10.2f));

	Day = 1;
	Hour = 23.9f;
	AIREWorldTime::Advance(
		Day,
		Hour,
		10.0f,
		AIREWorldTime::RealSecondsPerGameDay);
	TestEqual(TEXT("Crossing 24:00 advances the day"), Day, 2);
	TestTrue(
		TEXT("Crossing 24:00 wraps the hour"),
		FMath::IsNearlyEqual(Hour, 0.1f));

	TestTrue(TEXT("04:59 is night"), AIREWorldTime::IsNightAtHour(4.99f));
	TestFalse(TEXT("05:00 starts daytime"), AIREWorldTime::IsNightAtHour(5.0f));
	TestFalse(TEXT("22:59 is daytime"), AIREWorldTime::IsNightAtHour(22.99f));
	TestTrue(TEXT("23:00 starts night"), AIREWorldTime::IsNightAtHour(23.0f));
	TestTrue(
		TEXT("Dawn midpoint uses half daylight"),
		FMath::IsNearlyEqual(AIREWorldTime::CalculateDayAlpha(5.0f), 0.5f));
	TestTrue(
		TEXT("Dusk midpoint uses half daylight"),
		FMath::IsNearlyEqual(AIREWorldTime::CalculateDayAlpha(23.0f), 0.5f));
	TestTrue(
		TEXT("Sun reaches the sunrise horizon at 05:00"),
		FMath::IsNearlyEqual(AIREWorldTime::CalculateSunPitch(
			5.0f,
			0.0f,
			-90.0f,
			0.0f,
			90.0f), 0.0f));
	TestTrue(
		TEXT("Sun reaches its noon pitch at 14:00"),
		FMath::IsNearlyEqual(AIREWorldTime::CalculateSunPitch(
			14.0f,
			0.0f,
			-90.0f,
			0.0f,
			90.0f), -90.0f));
	TestTrue(
		TEXT("Sun reaches the sunset horizon at 23:00"),
		FMath::IsNearlyEqual(AIREWorldTime::CalculateSunPitch(
			23.0f,
			0.0f,
			-90.0f,
			0.0f,
			90.0f), 0.0f));
	TestTrue(
		TEXT("Moon reaches its night pitch at 02:00"),
		FMath::IsNearlyEqual(AIREWorldTime::CalculateMoonPitch(
			2.0f,
			0.0f,
			-35.0f,
			90.0f), -35.0f));

	if (!TestNotNull(TEXT("Engine is available"), GEngine))
	{
		return false;
	}

	const FName WorldName = MakeUniqueObjectName(
		nullptr,
		UWorld::StaticClass(),
		NAME_None,
		EUniqueObjectNameOptions::GloballyUnique);
	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		WorldName,
		GetTransientPackage());
	if (!TestNotNull(TEXT("Transient world is created"), TestWorld))
	{
		return false;
	}
	FWorldContext& WorldContext =
		GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		TestWorld->EndPlay(EEndPlayReason::Quit);
		GEngine->ShutdownWorldNetDriver(TestWorld);
		TestWorld->DestroyWorld(true);
		TestWorld->SetPhysicsScene(nullptr);
		GEngine->DestroyWorldContext(TestWorld);
	};
	TestWorld->BeginPlay();

	UAIREWorldTimeSubsystem* WorldTime =
		TestWorld->GetSubsystem<UAIREWorldTimeSubsystem>();
	if (!TestNotNull(TEXT("World time subsystem is available"), WorldTime))
	{
		return false;
	}
	TestEqual(TEXT("World starts on day one"), WorldTime->GetCurrentDay(), 1);
	TestTrue(
		TEXT("World starts at 09:00"),
		FMath::IsNearlyEqual(WorldTime->GetCurrentTimeOfDay(), 9.0f));
	TestTrue(TEXT("09:00 is daytime"), WorldTime->IsDaytime());

	WorldTime->ToggleDayNight();
	TestTrue(
		TEXT("Day toggle moves to 00:00"),
		FMath::IsNearlyEqual(WorldTime->GetCurrentTimeOfDay(), 0.0f));
	TestTrue(TEXT("00:00 is night"), WorldTime->IsNight());
	WorldTime->ToggleDayNight();
	TestTrue(
		TEXT("Night toggle moves to 12:00"),
		FMath::IsNearlyEqual(WorldTime->GetCurrentTimeOfDay(), 12.0f));
	WorldTime->SetTimeOfDay(5.5f);
	TestTrue(
		TEXT("Set time accepts a fractional hour"),
		FMath::IsNearlyEqual(WorldTime->GetCurrentTimeOfDay(), 5.5f));
	WorldTime->SetTimeOfDay(25.25f);
	TestTrue(
		TEXT("Set time normalizes hours above one day"),
		FMath::IsNearlyEqual(WorldTime->GetCurrentTimeOfDay(), 1.25f));
	TestFalse(TEXT("World time starts unpaused"), WorldTime->IsTimePaused());
	WorldTime->ToggleTimePause();
	TestTrue(TEXT("Time pause toggle pauses progression"), WorldTime->IsTimePaused());
	WorldTime->ToggleTimePause();
	TestFalse(TEXT("Second pause toggle resumes progression"), WorldTime->IsTimePaused());

	return true;
}

#endif

#include "Command/AIRELocalGatherFallback.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRELocalGatherFallbackTest,
	"AIRE.Companion.Command.LocalGatherFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIRELocalGatherFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	EAIREGatherResourceKind Resource = EAIREGatherResourceKind::None;
	TestTrue(
		TEXT("Explicit wood request is accepted"),
		FAIRELocalGatherFallback::TryParseResource(TEXT("나무 캐줘"), Resource));
	TestEqual(TEXT("Wood is selected"), Resource, EAIREGatherResourceKind::Wood);
	TestTrue(
		TEXT("Explicit stone request is accepted"),
		FAIRELocalGatherFallback::TryParseResource(TEXT("돌 캐줘"), Resource));
	TestEqual(TEXT("Stone is selected"), Resource, EAIREGatherResourceKind::Stone);
	TestTrue(
		TEXT("Explicit iron request is accepted"),
		FAIRELocalGatherFallback::TryParseResource(TEXT("철광석 가져와"), Resource));
	TestEqual(TEXT("Iron is selected"), Resource, EAIREGatherResourceKind::IronOre);

	const FString Rejected[] =
	{
		TEXT("돌이 예쁘다"),
		TEXT("철광석은 어디 있어?"),
		TEXT("돌이랑 철광석 캐줘"),
		TEXT("나무랑 돌 캐줘"),
		TEXT("돌 캐지 마"),
		TEXT("철광석 말고 나무 캐줘"),
		TEXT("채집해줘"),
		TEXT(""),
	};
	for (const FString& Message : Rejected)
	{
		TestFalse(
			TEXT("Ambiguous or unsafe request is rejected"),
			FAIRELocalGatherFallback::TryParseResource(Message, Resource));
	}
	return true;
}

#endif

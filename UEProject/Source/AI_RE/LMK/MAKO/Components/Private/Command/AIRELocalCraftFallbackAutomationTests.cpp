#include "Command/AIRELocalCraftFallback.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRELocalCraftFallbackTest,
	"AIRE.Companion.Command.LocalCraftFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIRELocalCraftFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString RecipeId;
	const TPair<FString, FString> Accepted[] =
	{
		{ TEXT("붕대 만들어줘"), TEXT("recipe-1") },
		{ TEXT("붕대 제작"), TEXT("recipe-1") },
		{ TEXT("엉붕 만들어줘"), TEXT("recipe-1") },
		{ TEXT("철 주괴 제련해줘"), TEXT("recipe-9") },
		{ TEXT("나무 손잡이 제작해"), TEXT("recipe-14") },
		{ TEXT("철검 만들어줘"), TEXT("recipe-11") },
	};
	for (const TPair<FString, FString>& Case : Accepted)
	{
		TestTrue(
			TEXT("Explicit single craft request is accepted"),
			FAIRELocalCraftFallback::TryParseRecipeId(Case.Key, RecipeId));
		TestEqual(TEXT("Recipe id matches"), RecipeId, Case.Value);
	}

	const FString Rejected[] =
	{
		TEXT("붕대 레시피 알려줘"),
		TEXT("붕대 만들지마"),
		TEXT("철괴 2개 제련해줘"),
		TEXT("나무 손잡이 하나 만들어줘"),
		TEXT("붕대랑 철검 같이 만들어줘"),
		TEXT("재료가 뭐가 필요해?"),
		TEXT("붕대가 필요해"),
		TEXT(""),
	};
	for (const FString& Message : Rejected)
	{
		TestFalse(
			TEXT("Question, negation, quantity, or ambiguity is rejected"),
			FAIRELocalCraftFallback::TryParseRecipeId(Message, RecipeId));
	}
	return true;
}

#endif

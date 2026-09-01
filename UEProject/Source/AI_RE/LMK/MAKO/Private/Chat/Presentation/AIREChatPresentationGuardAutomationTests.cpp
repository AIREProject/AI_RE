#include "Chat/Presentation/AIREChatPresentationGuard.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIREChatPresentationGuardTest,
	"AIRE.Companion.Chat.Presentation.VerbatimEchoGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIREChatPresentationGuardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TPair<FString, FString> EchoCases[] =
	{
		{TEXT("안녕"), TEXT("안녕")},
		{TEXT("오늘 어때?"), TEXT("오늘 어때")},
		{TEXT("난 카카오가 싫어"), TEXT(" 난 카카오가 싫어. ")},
		{TEXT("너도 그렇게 생각하지?"), TEXT("너도 그렇게 생각하지!")},
		{TEXT("배고파"), TEXT("배고파!")},
		{TEXT("돌 캐줘"), TEXT("돌 캐줘.")},
		{TEXT("철광석을 캐 줘"), TEXT("철광석을 캐 줘!")},
		{TEXT("나무를 가져와"), TEXT("나무를 가져와")},
		{TEXT("여기서 기다려"), TEXT("여기서 기다려.")},
		{TEXT("나를 따라와"), TEXT("나를 따라와!")},
		{TEXT("오늘 힘들었어"), TEXT("오늘 힘들었어")},
		{TEXT("비 오는 날이 좋아"), TEXT("비 오는 날이 좋아.")},
		{TEXT("내 이름은 하나야"), TEXT("내 이름은 하나야")},
		{TEXT("민트초코를 좋아해"), TEXT("민트초코를 좋아해!")},
		{TEXT("다음에도 기억해줘"), TEXT("다음에도 기억해줘.")},
		{TEXT("마코 고마워"), TEXT("마코 고마워!")},
		{TEXT("마을로 돌아가자"), TEXT("마을로 돌아가자")},
		{TEXT("지금 몇 시야?"), TEXT("지금 몇 시야")},
		{TEXT("보스가 무서워"), TEXT("보스가 무서워.")},
		{TEXT("오늘은 쉬고 싶어"), TEXT("오늘은 쉬고 싶어!")},
	};
	for (const TPair<FString, FString>& EchoCase : EchoCases)
	{
		bool bReplaced = false;
		const FString Guarded = FAIREChatPresentationGuard::GuardDisplayText(
			EchoCase.Value,
			EchoCase.Key,
			bReplaced);
		TestTrue(TEXT("Verbatim echo is replaced"), bReplaced);
		TestNotEqual(TEXT("Replacement differs from player text"), Guarded, EchoCase.Key);
	}
	const FString WrappedEchoes[] =
	{
		TEXT("사용자: 나는 민트초코를 좋아해. 다음에도 기억해줘."),
		TEXT("네 말: 나는 민트초코를 좋아해. 다음에도 기억해줘."),
		TEXT("대답: 나는 민트초코를 좋아해. 다음에도 기억해줘."),
		TEXT("이렇게 말했지? 나는 민트초코를 좋아해. 다음에도 기억해줘."),
		TEXT("&quot;나는 민트초코를 좋아해. 다음에도 기억해줘.&quot;"),
	};
	for (const FString& WrappedEcho : WrappedEchoes)
	{
		bool bWrappedReplaced = false;
		FAIREChatPresentationGuard::GuardDisplayText(
			WrappedEcho,
			TEXT("나는 민트초코를 좋아해. 다음에도 기억해줘."),
			bWrappedReplaced);
		TestTrue(TEXT("Short role and quote wrappers are replaced"), bWrappedReplaced);
	}

	bool bReplaced = false;
	const FString Contextual = TEXT("민트초코를 좋아한다고 했었지. 어떤 점이 좋아?");
	TestEqual(
		TEXT("Contextual memory use remains visible"),
		FAIREChatPresentationGuard::GuardDisplayText(
			Contextual,
			TEXT("민트초코를 좋아해"),
			bReplaced),
		Contextual);
	TestFalse(TEXT("Contextual response is not replaced"), bReplaced);
	const FString ShortAssentResponse = TEXT("응, 그래. 같이 가자.");
	TestEqual(
		TEXT("Substantive response to a one-letter assent remains visible"),
		FAIREChatPresentationGuard::GuardDisplayText(
			ShortAssentResponse,
			TEXT("응"),
			bReplaced),
		ShortAssentResponse);
	TestFalse(TEXT("One-letter assent does not over-block a response"), bReplaced);

	const FString MechanicalResponses[] =
	{
		TEXT("물론입니다. 요청을 처리하겠습니다."),
		TEXT("좋은 질문입니다. 자세히 설명해 드리겠습니다."),
		TEXT("도움이 되었기를 바랍니다."),
		TEXT("저는 AI 언어 모델로서 감정을 느끼지 않습니다."),
	};
	for (const FString& MechanicalResponse : MechanicalResponses)
	{
		bool bMechanicalReplaced = false;
		FAIREChatPresentationGuard::GuardDisplayText(
			MechanicalResponse,
			TEXT("안녕"),
			bMechanicalReplaced);
		TestTrue(TEXT("Mechanical AI boilerplate is replaced"), bMechanicalReplaced);
	}
	return true;
}

#endif

#include "Chat/Presentation/AIREChatPresentationGuard.h"

namespace
{
	FString NormalizeEchoText(const FString& Value)
	{
		FString Normalized;
		Normalized.Reserve(Value.Len());
		for (const TCHAR Character : Value.TrimStartAndEnd())
		{
			if (!FChar::IsWhitespace(Character)
				&& !FChar::IsPunct(Character))
			{
				Normalized.AppendChar(FChar::ToLower(Character));
			}
		}
		return Normalized;
	}

	bool IsMechanicalAiCliche(const FString& Response)
	{
		const FString Normalized = NormalizeEchoText(Response);
		const TCHAR* const Cliches[] =
		{
			TEXT("물론입니다"),
			TEXT("좋은질문입니다"),
			TEXT("좋은질문이에요"),
			TEXT("도움이되었기를바랍니다"),
			TEXT("추가로궁금한점이있으면"),
			TEXT("추가로궁금한사항이있으면"),
			TEXT("무엇을도와드릴까요"),
			TEXT("저는ai"),
			TEXT("저는인공지능"),
			TEXT("ai언어모델로서"),
			TEXT("요청을처리하겠습니다"),
		};
		for (const TCHAR* Cliche : Cliches)
		{
			if (Normalized.Contains(Cliche))
			{
				return true;
			}
		}
		return false;
	}

	FString SelectSafeFallback(const FString& UserMessage)
	{
		const TCHAR* const Fallbacks[] =
		{
			TEXT("응, 그 얘기 조금만 더 들려줘. 네 생각을 제대로 알고 싶어."),
			TEXT("그래, 어디부터 같이 얘기해 볼까?"),
			TEXT("마코는 여기 있어. 편하게 이어서 말해 줘."),
		};
		for (const TCHAR* Fallback : Fallbacks)
		{
			if (!FAIREChatPresentationGuard::IsVerbatimPlayerEcho(Fallback, UserMessage))
			{
				return Fallback;
			}
		}

		FString Fallback = TEXT("잠깐만, 네 얘기를 다시 생각해 볼게.");
		while (FAIREChatPresentationGuard::IsVerbatimPlayerEcho(Fallback, UserMessage))
		{
			Fallback.Append(TEXT(" 조금만 더 들려줘."));
		}
		return Fallback;
	}
}

bool FAIREChatPresentationGuard::IsVerbatimPlayerEcho(
	const FString& Response,
	const FString& UserMessage)
{
	const FString NormalizedUserMessage = NormalizeEchoText(UserMessage);
	if (NormalizedUserMessage.IsEmpty())
	{
		return false;
	}

	const FString NormalizedResponse = NormalizeEchoText(Response);
	return NormalizedResponse == NormalizedUserMessage
		|| (NormalizedUserMessage.Len() >= 2
			&& NormalizedResponse.Contains(NormalizedUserMessage)
			&& NormalizedResponse.Len() - NormalizedUserMessage.Len() <= 12);
}

FString FAIREChatPresentationGuard::GuardDisplayText(
	const FString& Response,
	const FString& UserMessage,
	bool& bOutReplaced)
{
	bOutReplaced = IsVerbatimPlayerEcho(Response, UserMessage)
		|| IsMechanicalAiCliche(Response);
	return bOutReplaced
		? SelectSafeFallback(UserMessage)
		: Response;
}

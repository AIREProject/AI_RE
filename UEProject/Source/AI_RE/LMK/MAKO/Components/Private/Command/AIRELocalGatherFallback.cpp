#include "Command/AIRELocalGatherFallback.h"

#include <initializer_list>

namespace
{
	bool ContainsAny(
		const FString& Value,
		const std::initializer_list<const TCHAR*> Candidates)
	{
		for (const TCHAR* Candidate : Candidates)
		{
			if (Value.Contains(Candidate, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

bool FAIRELocalGatherFallback::TryParseResource(
	const FString& UserMessage,
	EAIREGatherResourceKind& OutResource)
{
	OutResource = EAIREGatherResourceKind::None;
	const FString Message = UserMessage.TrimStartAndEnd();
	if (Message.IsEmpty()
		|| ContainsAny(
			Message,
			{TEXT("하지 마"), TEXT("하지마"), TEXT("캐지 마"),
				TEXT("캐지마"), TEXT("말고"), TEXT("싫어")}))
	{
		return false;
	}
	const bool bHasAction = ContainsAny(
		Message,
		{TEXT("캐"), TEXT("채집"), TEXT("가져와"), TEXT("구해")});
	const bool bHasStone = ContainsAny(
		Message,
		{TEXT("돌"), TEXT("바위"), TEXT("석재")});
	const bool bHasIron = ContainsAny(
		Message,
		{TEXT("철광석"), TEXT("철 광석"), TEXT("철광"), TEXT("철 원석"),
			TEXT("철 캐"), TEXT("철을 캐"), TEXT("철 채집"), TEXT("철을 채집"),
			TEXT("철 가져와"), TEXT("철을 가져와"), TEXT("철 구해"), TEXT("철을 구해")});
	if (!bHasAction || bHasStone == bHasIron)
	{
		return false;
	}
	OutResource = bHasStone
		? EAIREGatherResourceKind::Stone
		: EAIREGatherResourceKind::IronOre;
	return true;
}

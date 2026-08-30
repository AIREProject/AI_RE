#include "Command/AIRELocalGatherFallback.h"

#include <initializer_list>

namespace
{
	bool ContainsAnyGatherToken(
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
		|| ContainsAnyGatherToken(
			Message,
			{TEXT("하지 마"), TEXT("하지마"), TEXT("캐지 마"),
				TEXT("캐지마"), TEXT("말고"), TEXT("싫어")}))
	{
		return false;
	}
	const bool bHasAction = ContainsAnyGatherToken(
		Message,
		{TEXT("캐"), TEXT("채집"), TEXT("가져와"), TEXT("구해")});
	const bool bHasWood = ContainsAnyGatherToken(
		Message,
		{TEXT("나무"), TEXT("목재"), TEXT("장작"), TEXT("통나무"), TEXT("나뭇가지")});
	const bool bHasStone = ContainsAnyGatherToken(
		Message,
		{TEXT("돌"), TEXT("바위"), TEXT("석재")});
	const bool bHasIron = ContainsAnyGatherToken(
		Message,
		{TEXT("철광석"), TEXT("철 광석"), TEXT("철광"), TEXT("철 원석"),
			TEXT("철 캐"), TEXT("철을 캐"), TEXT("철 채집"), TEXT("철을 채집"),
			TEXT("철 가져와"), TEXT("철을 가져와"), TEXT("철 구해"), TEXT("철을 구해")});
	const int32 ResourceCount = static_cast<int32>(bHasWood)
		+ static_cast<int32>(bHasStone)
		+ static_cast<int32>(bHasIron);
	if (!bHasAction || ResourceCount != 1)
	{
		return false;
	}
	OutResource = bHasWood
		? EAIREGatherResourceKind::Wood
		: bHasStone
			? EAIREGatherResourceKind::Stone
			: EAIREGatherResourceKind::IronOre;
	return true;
}

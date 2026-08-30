#include "Command/AIRELocalCraftFallback.h"

#include <initializer_list>

namespace
{
	bool ContainsAny(
		const FString& Value,
		const std::initializer_list<const TCHAR*> Values)
	{
		for (const TCHAR* Candidate : Values)
		{
			if (Value.Contains(Candidate, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	bool ContainsExplicitQuantity(const FString& Value)
	{
		for (const TCHAR Character : Value)
		{
			if (FChar::IsDigit(Character))
			{
				return true;
			}
		}
		return ContainsAny(
			Value,
			{ TEXT("하나"), TEXT("한 개"), TEXT("한개"), TEXT("두 개"),
				TEXT("두개"), TEXT("세 개"), TEXT("세개"), TEXT("여러") });
	}
}

bool FAIRELocalCraftFallback::TryParseRecipeId(
	const FString& UserMessage,
	FString& OutRecipeId)
{
	OutRecipeId.Reset();
	const FString Message = UserMessage.TrimStartAndEnd();
	if (Message.IsEmpty()
		|| ContainsAny(Message, { TEXT("하지 마"), TEXT("하지마"),
			TEXT("만들지 마"), TEXT("만들지마"), TEXT("안 만들"),
			TEXT("안만들"), TEXT("제작하지"), TEXT("제련하지"),
			TEXT("필요 없어"), TEXT("말고") })
		|| ContainsAny(Message, { TEXT("레시피"), TEXT("제작법"),
			TEXT("재료"), TEXT("어떻게"), TEXT("뭐가 필요") })
		|| ContainsExplicitQuantity(Message)
		|| ContainsAny(Message, { TEXT("전부"), TEXT("같이") }))
	{
		return false;
	}

	const bool bHasAction = ContainsAny(
		Message,
		{ TEXT("만들"), TEXT("제작"), TEXT("제련") });
	const bool bBandage = ContainsAny(
		Message,
		{ TEXT("엉성한 붕대"), TEXT("엉성한붕대"), TEXT("엉붕"),
			TEXT("붕대") });
	const bool bIngot = ContainsAny(
		Message, { TEXT("철괴"), TEXT("철 주괴"), TEXT("철주괴"),
			TEXT("철 잉곳"), TEXT("철잉곳") });
	const bool bHandle = ContainsAny(
		Message, { TEXT("나무 손잡이"), TEXT("나무손잡이"),
			TEXT("목재 손잡이"), TEXT("목재손잡이") });
	const bool bSword = ContainsAny(
		Message, { TEXT("철검"), TEXT("철 검"), TEXT("쇠검") });
	const int32 MatchCount = static_cast<int32>(bBandage)
		+ static_cast<int32>(bIngot)
		+ static_cast<int32>(bHandle)
		+ static_cast<int32>(bSword);
	if (!bHasAction || MatchCount != 1)
	{
		return false;
	}

	OutRecipeId = bBandage ? TEXT("recipe-1")
		: bIngot ? TEXT("recipe-9")
		: bHandle ? TEXT("recipe-14")
		: TEXT("recipe-11");
	return true;
}

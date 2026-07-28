#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIREChatHistorySubsystem.generated.h"

UENUM(BlueprintType)
enum class EAIREChatMessageAuthor : uint8
{
	Player,
	Companion
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIREChatLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	EAIREChatMessageAuthor Author = EAIREChatMessageAuthor::Player;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|Companion|Chat")
	FDateTime Timestamp;
};

UCLASS()
class AI_RE_API UAIREChatHistorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	FAIREChatLogEntry AddEntry(
		EAIREChatMessageAuthor Author,
		const FString& Text);

	const TArray<FAIREChatLogEntry>& GetEntries() const;

private:
	static constexpr int32 MaxEntryCount = 200;

	UPROPERTY(Transient)
	TArray<FAIREChatLogEntry> Entries;

	int64 NextSequence = 1;
};

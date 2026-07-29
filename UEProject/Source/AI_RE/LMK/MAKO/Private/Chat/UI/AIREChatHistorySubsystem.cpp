#include "Chat/UI/AIREChatHistorySubsystem.h"

FAIREChatLogEntry UAIREChatHistorySubsystem::AddEntry(
	const EAIREChatMessageAuthor Author,
	const FString& Text)
{
	FAIREChatLogEntry Entry;
	Entry.Sequence = NextSequence++;
	Entry.Author = Author;
	Entry.Text = Text;
	Entry.Timestamp = FDateTime::Now();

	Entries.Add(Entry);
	if (Entries.Num() > MaxEntryCount)
	{
		Entries.RemoveAt(0, Entries.Num() - MaxEntryCount);
	}

	return Entry;
}

const TArray<FAIREChatLogEntry>&
UAIREChatHistorySubsystem::GetEntries() const
{
	return Entries;
}

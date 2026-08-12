#pragma once

#include "CoreMinimal.h"

namespace AIREWorldContext
{
	inline constexpr int32 SchemaVersion = 1;
	inline constexpr int32 MaxStableIdLength = 128;
	inline constexpr int32 MaxThreatCount = 32;
	inline constexpr int32 MaxNearbyResourceTypes = 8;
	inline constexpr int32 MaxAvailableWorkstations = 8;
	inline constexpr int32 MaxInventoryItemTypes = 16;
	inline constexpr int32 MaxContextUtf8Bytes = 8 * 1024;
	inline constexpr TCHAR DefaultLocationId[] = TEXT("forest_camp");
}

enum class EAIREWorldContextWorkType : uint8
{
	None,
	Crafting,
	Harvesting,
	StorageTransfer
};

enum class EAIREWorldContextWorkState : uint8
{
	None,
	Requested,
	Moving,
	Working,
	PausedByCombat
};

struct AI_RE_API FAIREWorldContextThreat
{
	bool bPresent = false;
	int32 Count = 0;
	FString NearestKind;
};

struct AI_RE_API FAIREWorldContextNearbyResource
{
	FString Kind;
	int32 Count = 0;
};

struct AI_RE_API FAIREWorldContextCurrentWork
{
	EAIREWorldContextWorkType Type = EAIREWorldContextWorkType::None;
	EAIREWorldContextWorkState State = EAIREWorldContextWorkState::None;

	bool IsSet() const
	{
		return Type != EAIREWorldContextWorkType::None
			&& State != EAIREWorldContextWorkState::None;
	}
};

struct AI_RE_API FAIREWorldContextInventoryItemTotal
{
	FString ItemId;
	int32 Count = 0;
};

struct AI_RE_API FAIREWorldContextInventory
{
	FString ContainerId;
	int32 FreeSlots = 0;
	TArray<FAIREWorldContextInventoryItemTotal> ItemTotals;
	bool bTruncated = false;
};

struct AI_RE_API FAIREWorldContextV1
{
	FString LocationId;
	FAIREWorldContextThreat Threat;
	TArray<FAIREWorldContextNearbyResource> NearbyResources;
	TArray<FString> AvailableWorkstations;
	FAIREWorldContextCurrentWork CurrentWork;
	TArray<FAIREWorldContextInventory> Inventories;
};

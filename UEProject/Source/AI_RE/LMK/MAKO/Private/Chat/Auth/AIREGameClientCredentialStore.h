#pragma once

#include "CoreMinimal.h"

class FAIREGameClientCredentialStore
{
public:
	static bool Load(const FString& Target, FString& OutToken);
	static bool Save(const FString& Target, const FString& Token);
	static bool Remove(const FString& Target);
};


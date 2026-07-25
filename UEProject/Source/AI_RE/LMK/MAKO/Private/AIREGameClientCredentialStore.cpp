#include "AIREGameClientCredentialStore.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincred.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

bool FAIREGameClientCredentialStore::Load(const FString& Target, FString& OutToken)
{
	OutToken.Reset();
#if PLATFORM_WINDOWS
	PCREDENTIALW Credential = nullptr;
	if (!CredReadW(*Target, CRED_TYPE_GENERIC, 0, &Credential) || Credential == nullptr)
	{
		return false;
	}

	const int32 CharacterCount = static_cast<int32>(Credential->CredentialBlobSize / sizeof(WCHAR));
	if (CharacterCount > 0 && Credential->CredentialBlob != nullptr)
	{
		OutToken = FString(
			CharacterCount,
			reinterpret_cast<const WCHAR*>(Credential->CredentialBlob));
	}
	CredFree(Credential);
	return !OutToken.IsEmpty();
#else
	return false;
#endif
}

bool FAIREGameClientCredentialStore::Save(const FString& Target, const FString& Token)
{
#if PLATFORM_WINDOWS
	if (Target.IsEmpty() || Token.IsEmpty())
	{
		return false;
	}

	CREDENTIALW Credential{};
	Credential.Type = CRED_TYPE_GENERIC;
	Credential.TargetName = const_cast<LPWSTR>(*Target);
	Credential.CredentialBlobSize = static_cast<DWORD>(Token.Len() * sizeof(WCHAR));
	Credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<WCHAR*>(*Token));
	Credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	Credential.UserName = const_cast<LPWSTR>(L"AI_RE GameClient");
	return CredWriteW(&Credential, 0) != 0;
#else
	return false;
#endif
}

bool FAIREGameClientCredentialStore::Remove(const FString& Target)
{
#if PLATFORM_WINDOWS
	if (Target.IsEmpty())
	{
		return false;
	}

	if (CredDeleteW(*Target, CRED_TYPE_GENERIC, 0) != 0)
	{
		return true;
	}
	return GetLastError() == ERROR_NOT_FOUND;
#else
	return false;
#endif
}

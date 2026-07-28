#include "AIREStateTreeMCPToolsetModule.h"

#include "AIREAnimationMCPToolset.h"
#include "AIRECompanionTestMCPToolset.h"
#include "AIREStateTreeMCPToolset.h"
#include "AIREUMGMCPToolset.h"
#include "Misc/CoreDelegates.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

void FAIREStateTreeMCPToolsetModule::StartupModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(
		this,
		&FAIREStateTreeMCPToolsetModule::RegisterToolset);
	FCoreDelegates::OnPreExit.AddRaw(this, &FAIREStateTreeMCPToolsetModule::UnregisterToolset);
}

void FAIREStateTreeMCPToolsetModule::ShutdownModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.RemoveAll(this);
	FCoreDelegates::OnPreExit.RemoveAll(this);
}

void FAIREStateTreeMCPToolsetModule::RegisterToolset()
{
	UToolsetRegistry::RegisterToolsetClass(UAIREStateTreeMCPToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UAIRECompanionTestMCPToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UAIREUMGMCPToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UAIREAnimationMCPToolset::StaticClass());
}

void FAIREStateTreeMCPToolsetModule::UnregisterToolset()
{
	UToolsetRegistry::UnregisterToolsetClass(UAIREAnimationMCPToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UAIREUMGMCPToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UAIRECompanionTestMCPToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UAIREStateTreeMCPToolset::StaticClass());
}

IMPLEMENT_MODULE(FAIREStateTreeMCPToolsetModule, AIREStateTreeMCPToolset)

#pragma once

#include "Modules/ModuleManager.h"

class FAIREStateTreeMCPToolsetModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterToolset();
	void UnregisterToolset();
};

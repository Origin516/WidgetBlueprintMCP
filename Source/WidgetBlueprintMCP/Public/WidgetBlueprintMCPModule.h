#pragma once

#include "Modules/ModuleManager.h"

class FWBMCPHttpServer;

DECLARE_LOG_CATEGORY_EXTERN(LogWidgetBlueprintMCP, Log, All);

class FWidgetBlueprintMCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FWBMCPHttpServer> HttpServer;
};

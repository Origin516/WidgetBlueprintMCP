#include "WidgetBlueprintMCPModule.h"
#include "WBMCPHttpServer.h"
#include "WBMCPSettings.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "WidgetBlueprintMCP"

DEFINE_LOG_CATEGORY(LogWidgetBlueprintMCP);

IMPLEMENT_MODULE(FWidgetBlueprintMCPModule, WidgetBlueprintMCP)

void FWidgetBlueprintMCPModule::StartupModule()
{
	HttpServer = MakeShared<FWBMCPHttpServer>();
	HttpServer->Start((uint32)UWBMCPSettings::Get()->Port);
	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("WidgetBlueprintMCP started on port %d"), UWBMCPSettings::Get()->Port);
}

void FWidgetBlueprintMCPModule::ShutdownModule()
{
	if (HttpServer.IsValid())
	{
		HttpServer->Stop();
		HttpServer.Reset();
	}
	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("WidgetBlueprintMCP stopped"));
}

#undef LOCTEXT_NAMESPACE

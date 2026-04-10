#include "WidgetBlueprintMCPModule.h"
#include "WBMCPHttpServer.h"
#include "WBMCPSseServer.h"
#include "WBMCPSettings.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "WidgetBlueprintMCP"

DEFINE_LOG_CATEGORY(LogWidgetBlueprintMCP);

IMPLEMENT_MODULE(FWidgetBlueprintMCPModule, WidgetBlueprintMCP)

void FWidgetBlueprintMCPModule::StartupModule()
{
	const UWBMCPSettings* Settings = UWBMCPSettings::Get();

	HttpServer = MakeShared<FWBMCPHttpServer>();
	HttpServer->Start((uint32)Settings->Port);

	if (Settings->bEnableSse)
	{
		SseServer = MakeShared<FWBMCPSseServer>();
		SseServer->Start((uint32)Settings->SsePort);
	}

	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("WidgetBlueprintMCP started (HTTP:%d, SSE:%s)"),
		Settings->Port,
		Settings->bEnableSse ? *FString::FromInt(Settings->SsePort) : TEXT("disabled"));
}

void FWidgetBlueprintMCPModule::ShutdownModule()
{
	if (SseServer.IsValid())
	{
		SseServer->Stop();
		SseServer.Reset();
	}
	if (HttpServer.IsValid())
	{
		HttpServer->Stop();
		HttpServer.Reset();
	}
	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("WidgetBlueprintMCP stopped"));
}

#undef LOCTEXT_NAMESPACE

#include "WBMCPHttpServer.h"
#include "WBMCPToolHandlers.h"
#include "WidgetBlueprintMCPModule.h"

#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FWBMCPHttpServer::~FWBMCPHttpServer()
{
	Stop();
}

void FWBMCPHttpServer::Start(uint32 Port)
{
	ListenPort = Port;

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	Router = HttpServerModule.GetHttpRouter(ListenPort);

	if (!Router.IsValid())
	{
		UE_LOG(LogWidgetBlueprintMCP, Error, TEXT("Failed to get HTTP router for port %d"), ListenPort);
		return;
	}

	RouteHandle = Router->BindRoute(
		FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_POST,
		[this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
		{
			return HandleMcpRequest(Request, OnComplete);
		}
	);

	HttpServerModule.StartAllListeners();
	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("MCP HTTP server listening on http://localhost:%d/mcp"), ListenPort);
}

void FWBMCPHttpServer::Stop()
{
	if (Router.IsValid() && RouteHandle.IsValid())
	{
		Router->UnbindRoute(RouteHandle);
		RouteHandle.Reset();
	}
	Router.Reset();
}

bool FWBMCPHttpServer::HandleMcpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Parse request body as JSON-RPC 2.0
	if (Request.Body.Num() == 0)
	{
		OnComplete(MakeErrorResponse(-32700, TEXT("Empty request body"), TSharedPtr<FJsonValue>()));
		return true;
	}
	// Add null terminator before converting — Request.Body is not null-terminated
	TArray<uint8> BodyData = Request.Body;
	BodyData.Add(0);
	const FString Body = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(BodyData.GetData())));

	TSharedPtr<FJsonObject> RequestJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);

	if (!FJsonSerializer::Deserialize(Reader, RequestJson) || !RequestJson.IsValid())
	{
		OnComplete(MakeErrorResponse(-32700, TEXT("Parse error"), MakeShared<FJsonValueNull>()));
		return true;
	}

	TSharedPtr<FJsonValue> IdValue = RequestJson->TryGetField(TEXT("id"));
	FString Method;
	RequestJson->TryGetStringField(TEXT("method"), Method);

	const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
	TSharedPtr<FJsonObject> Params;
	if (RequestJson->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
	{
		Params = *ParamsPtr;
	}

	TSharedPtr<FJsonObject> Result;

	if (Method == TEXT("initialize"))
	{
		Result = HandleInitialize(Params);
	}
	else if (Method == TEXT("tools/list"))
	{
		Result = HandleToolsList(Params);
	}
	else if (Method == TEXT("tools/call"))
	{
		Result = HandleToolsCall(Params);
	}
	else if (Method == TEXT("notifications/initialized"))
	{
		// Notification — no response body required
		OnComplete(MakeJsonResponse(TEXT("{}")));
		return true;
	}
	else
	{
		OnComplete(MakeErrorResponse(-32601, FString::Printf(TEXT("Method not found: %s"), *Method), IdValue));
		return true;
	}

	// Build JSON-RPC 2.0 response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Response->SetField(TEXT("id"), IdValue.IsValid() ? IdValue : MakeShared<FJsonValueNull>());
	Response->SetObjectField(TEXT("result"), Result);

	OnComplete(MakeJsonResponse(SerializeJson(Response)));
	return true;
}

TSharedPtr<FJsonObject> FWBMCPHttpServer::HandleInitialize(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), TEXT("ue4-widget-blueprint-mcp"));
	ServerInfo->SetStringField(TEXT("version"), TEXT("1.0.0"));

	TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> ToolsCapability = MakeShared<FJsonObject>();
	Capabilities->SetObjectField(TEXT("tools"), ToolsCapability);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), TEXT("2024-11-05"));
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);
	Result->SetObjectField(TEXT("capabilities"), Capabilities);

	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("MCP initialize handshake completed"));
	return Result;
}

TSharedPtr<FJsonObject> FWBMCPHttpServer::HandleToolsList(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), BuildToolList());
	return Result;
}

TSharedPtr<FJsonObject> FWBMCPHttpServer::HandleToolsCall(const TSharedPtr<FJsonObject>& Params)
{
	FString ToolName;
	const TSharedPtr<FJsonObject>* ArgumentsPtr = nullptr;
	TSharedPtr<FJsonObject> Arguments;

	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("name"), ToolName);
		if (Params->TryGetObjectField(TEXT("arguments"), ArgumentsPtr) && ArgumentsPtr)
		{
			Arguments = *ArgumentsPtr;
		}
	}

	return FWBMCPToolHandlers::Dispatch(ToolName, Arguments);
}

TArray<TSharedPtr<FJsonValue>> FWBMCPHttpServer::BuildToolList()
{
	TArray<TSharedPtr<FJsonValue>> Tools;

	auto MakeTool = [](const FString& Name, const FString& Description) -> TSharedPtr<FJsonValue>
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));
		InputSchema->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());

		TSharedPtr<FJsonObject> Tool = MakeShared<FJsonObject>();
		Tool->SetStringField(TEXT("name"), Name);
		Tool->SetStringField(TEXT("description"), Description);
		Tool->SetObjectField(TEXT("inputSchema"), InputSchema);
		return MakeShared<FJsonValueObject>(Tool);
	};

	Tools.Add(MakeTool(TEXT("list_widget_blueprints"),  TEXT("List all Widget Blueprints in the project")));
	Tools.Add(MakeTool(TEXT("get_widget_blueprint"),    TEXT("Get the widget tree of a Widget Blueprint as JSON")));
	Tools.Add(MakeTool(TEXT("modify_widget_property"),  TEXT("Modify a property on a widget")));
	Tools.Add(MakeTool(TEXT("add_widget"),              TEXT("Add a new widget to the widget tree")));
	Tools.Add(MakeTool(TEXT("remove_widget"),           TEXT("Remove a widget from the widget tree")));
	Tools.Add(MakeTool(TEXT("list_functions"),          TEXT("List all functions and events in a Widget Blueprint")));
	Tools.Add(MakeTool(TEXT("get_function_graph"),      TEXT("Get the node graph of a function as JSON")));
	Tools.Add(MakeTool(TEXT("modify_node_pin"),         TEXT("Modify a pin's default value on a blueprint node")));
	Tools.Add(MakeTool(TEXT("add_node"),                TEXT("Add a new node to a function graph")));
	Tools.Add(MakeTool(TEXT("connect_pins"),            TEXT("Connect two pins in a function graph")));
	Tools.Add(MakeTool(TEXT("disconnect_pins"),         TEXT("Disconnect two pins in a function graph")));
	Tools.Add(MakeTool(TEXT("remove_node"),             TEXT("Remove a node from a function graph")));
	Tools.Add(MakeTool(TEXT("save_widget_blueprint"),   TEXT("Save and compile a Widget Blueprint")));
	Tools.Add(MakeTool(TEXT("execute_batch"), TEXT("Execute multiple MCP tool commands in sequence. Supports $id.node_id substitution from previous add_node results.")));

	return Tools;
}

TUniquePtr<FHttpServerResponse> FWBMCPHttpServer::MakeJsonResponse(const FString& Body, int32 StatusCode)
{
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Body, TEXT("application/json"));
	Response->Code = static_cast<EHttpServerResponseCodes>(StatusCode);
	return Response;
}

TUniquePtr<FHttpServerResponse> FWBMCPHttpServer::MakeErrorResponse(int32 Code, const FString& Message, const TSharedPtr<FJsonValue>& Id)
{
	TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
	Error->SetNumberField(TEXT("code"), Code);
	Error->SetStringField(TEXT("message"), Message);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Response->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
	Response->SetObjectField(TEXT("error"), Error);

	return MakeJsonResponse(SerializeJson(Response));
}

FString FWBMCPHttpServer::SerializeJson(const TSharedPtr<FJsonObject>& Object)
{
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	return Output;
}

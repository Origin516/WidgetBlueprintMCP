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
	if (Request.Body.Num() == 0)
	{
		OnComplete(MakeErrorResponse(-32700, TEXT("Empty request body"), TSharedPtr<FJsonValue>()));
		return true;
	}
	TArray<uint8> BodyData = Request.Body;
	BodyData.Add(0);
	const FString Body = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(BodyData.GetData())));

	OnComplete(MakeJsonResponse(ProcessJsonRpcBody(Body)));
	return true;
}

FString FWBMCPHttpServer::ProcessJsonRpcBody(const FString& Body)
{
	TSharedPtr<FJsonObject> RequestJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);

	if (!FJsonSerializer::Deserialize(Reader, RequestJson) || !RequestJson.IsValid())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetNumberField(TEXT("code"), -32700);
		Err->SetStringField(TEXT("message"), TEXT("Parse error"));
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Resp->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
		Resp->SetObjectField(TEXT("error"), Err);
		return SerializeJson(Resp);
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

	if (Method == TEXT("notifications/initialized"))
	{
		return TEXT("{}");
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
	else
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetNumberField(TEXT("code"), -32601);
		Err->SetStringField(TEXT("message"), FString::Printf(TEXT("Method not found: %s"), *Method));
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Resp->SetField(TEXT("id"), IdValue.IsValid() ? IdValue : MakeShared<FJsonValueNull>());
		Resp->SetObjectField(TEXT("error"), Err);
		return SerializeJson(Resp);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Response->SetField(TEXT("id"), IdValue.IsValid() ? IdValue : MakeShared<FJsonValueNull>());
	Response->SetObjectField(TEXT("result"), Result);
	return SerializeJson(Response);
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

	// MSVC-safe: use local struct + std::initializer_list instead of TPair brace-init
	struct FProp { const TCHAR* Name; const TCHAR* Type; };

	auto MakeTool = [](const TCHAR* Name, const TCHAR* Description,
		std::initializer_list<FProp> Props,
		std::initializer_list<const TCHAR*> Required) -> TSharedPtr<FJsonValue>
	{
		TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
		for (const FProp& P : Props)
		{
			TSharedPtr<FJsonObject> PropDef = MakeShared<FJsonObject>();
			PropDef->SetStringField(TEXT("type"), P.Type);
			PropsObj->SetObjectField(P.Name, PropDef);
		}

		TArray<TSharedPtr<FJsonValue>> RequiredArr;
		for (const TCHAR* R : Required) RequiredArr.Add(MakeShared<FJsonValueString>(R));

		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));
		InputSchema->SetObjectField(TEXT("properties"), PropsObj);
		InputSchema->SetArrayField(TEXT("required"), RequiredArr);

		TSharedPtr<FJsonObject> Tool = MakeShared<FJsonObject>();
		Tool->SetStringField(TEXT("name"), Name);
		Tool->SetStringField(TEXT("description"), Description);
		Tool->SetObjectField(TEXT("inputSchema"), InputSchema);
		return MakeShared<FJsonValueObject>(Tool);
	};

	Tools.Add(MakeTool(TEXT("list_widget_blueprints"),
		TEXT("List all Widget Blueprint asset paths in the project."),
		{ {TEXT("path_filter"), TEXT("string")} }, {}));

	Tools.Add(MakeTool(TEXT("get_widget_blueprint"),
		TEXT("Get widget tree, variables, animations and graph names of a Widget Blueprint."),
		{ {TEXT("asset_path"), TEXT("string")} },
		{ TEXT("asset_path") }));

	Tools.Add(MakeTool(TEXT("get_animation"),
		TEXT("Get detailed animation data (possessables, tracks, keyframes) for a named animation in a Widget Blueprint."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("animation_name"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("animation_name") }));

	Tools.Add(MakeTool(TEXT("list_functions"),
		TEXT("List all event graphs, functions and macros in a Widget Blueprint."),
		{ {TEXT("asset_path"), TEXT("string")} },
		{ TEXT("asset_path") }));

	Tools.Add(MakeTool(TEXT("get_function_graph"),
		TEXT("Get the full node graph (nodes, pins, connections) of a function or event graph. Use node ids from here for modify/connect/remove operations."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("graph_name"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("graph_name") }));

	Tools.Add(MakeTool(TEXT("modify_widget_property"),
		TEXT("Modify a property value on a widget in the widget tree."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("widget_name"), TEXT("string")}, {TEXT("property_name"), TEXT("string")}, {TEXT("value"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("widget_name"), TEXT("property_name"), TEXT("value") }));

	Tools.Add(MakeTool(TEXT("modify_node_pin"),
		TEXT("Set the default value of a pin on a blueprint graph node."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("graph_name"), TEXT("string")}, {TEXT("node_id"), TEXT("string")}, {TEXT("pin_name"), TEXT("string")}, {TEXT("value"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("graph_name"), TEXT("node_id"), TEXT("pin_name"), TEXT("value") }));

	Tools.Add(MakeTool(TEXT("add_widget"),
		TEXT("Add a new widget to the widget tree."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("widget_class"), TEXT("string")}, {TEXT("widget_name"), TEXT("string")}, {TEXT("parent_widget"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("widget_class"), TEXT("widget_name") }));

	Tools.Add(MakeTool(TEXT("remove_widget"),
		TEXT("Remove a widget from the widget tree."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("widget_name"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("widget_name") }));

	Tools.Add(MakeTool(TEXT("add_node"),
		TEXT("Add a new node to a function graph. node_type: CallFunction|VariableGet|VariableSet|Branch|Sequence|CustomEvent|Cast|DoOnce. Returns node_id."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("graph_name"), TEXT("string")}, {TEXT("node_type"), TEXT("string")}, {TEXT("x"), TEXT("number")}, {TEXT("y"), TEXT("number")}, {TEXT("params"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("graph_name"), TEXT("node_type") }));

	Tools.Add(MakeTool(TEXT("connect_pins"),
		TEXT("Connect two pins in a function graph. Get node_id and pin names from get_function_graph."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("graph_name"), TEXT("string")}, {TEXT("source_node_id"), TEXT("string")}, {TEXT("source_pin"), TEXT("string")}, {TEXT("target_node_id"), TEXT("string")}, {TEXT("target_pin"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("graph_name"), TEXT("source_node_id"), TEXT("source_pin"), TEXT("target_node_id"), TEXT("target_pin") }));

	Tools.Add(MakeTool(TEXT("disconnect_pins"),
		TEXT("Disconnect two pins in a function graph."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("graph_name"), TEXT("string")}, {TEXT("source_node_id"), TEXT("string")}, {TEXT("source_pin"), TEXT("string")}, {TEXT("target_node_id"), TEXT("string")}, {TEXT("target_pin"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("graph_name"), TEXT("source_node_id"), TEXT("source_pin"), TEXT("target_node_id"), TEXT("target_pin") }));

	Tools.Add(MakeTool(TEXT("remove_node"),
		TEXT("Remove a node from a function graph."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("graph_name"), TEXT("string")}, {TEXT("node_id"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("graph_name"), TEXT("node_id") }));

	Tools.Add(MakeTool(TEXT("save_widget_blueprint"),
		TEXT("Save a Widget Blueprint asset to disk."),
		{ {TEXT("asset_path"), TEXT("string")} },
		{ TEXT("asset_path") }));

	Tools.Add(MakeTool(TEXT("compile_widget_blueprint"),
		TEXT("Compile a Widget Blueprint. Returns success, errors[], warnings[]."),
		{ {TEXT("asset_path"), TEXT("string")} },
		{ TEXT("asset_path") }));

	Tools.Add(MakeTool(TEXT("add_variable"),
		TEXT("Add a new variable to a Widget Blueprint. var_type: string|bool|int|float|text|name"),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("var_name"), TEXT("string")}, {TEXT("var_type"), TEXT("string")}, {TEXT("category"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("var_name"), TEXT("var_type") }));

	Tools.Add(MakeTool(TEXT("set_variable_default"),
		TEXT("Set the default value of a variable in a Widget Blueprint."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("var_name"), TEXT("string")}, {TEXT("default_value"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("var_name"), TEXT("default_value") }));

	Tools.Add(MakeTool(TEXT("modify_variable_flags"),
		TEXT("Modify variable flags: instance_editable and expose_on_spawn."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("var_name"), TEXT("string")}, {TEXT("instance_editable"), TEXT("boolean")}, {TEXT("expose_on_spawn"), TEXT("boolean")} },
		{ TEXT("asset_path"), TEXT("var_name") }));

	Tools.Add(MakeTool(TEXT("remove_variable"),
		TEXT("Remove a variable from a Widget Blueprint."),
		{ {TEXT("asset_path"), TEXT("string")}, {TEXT("var_name"), TEXT("string")} },
		{ TEXT("asset_path"), TEXT("var_name") }));

	Tools.Add(MakeTool(TEXT("execute_batch"),
		TEXT("Execute multiple MCP tool commands in sequence. Supports $id.node_id substitution from previous add_node results. stop_on_error defaults to true."),
		{ {TEXT("commands"), TEXT("array")}, {TEXT("stop_on_error"), TEXT("boolean")} },
		{ TEXT("commands") }));

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

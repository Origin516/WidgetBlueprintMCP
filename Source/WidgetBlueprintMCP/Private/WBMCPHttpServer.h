#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "IHttpRouter.h"

class FWBMCPHttpServer
{
public:
	FWBMCPHttpServer() = default;
	~FWBMCPHttpServer();

	void Start(uint32 Port);
	void Stop();

	// Route handler for POST /mcp
	bool HandleMcpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	
	// Shared JSON-RPC processing — called by SSE server too
	static FString ProcessJsonRpcBody(const FString& Body);
	
private:
	// JSON-RPC method dispatchers (static — no instance state)
	static TSharedPtr<FJsonObject> HandleInitialize(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonObject> HandleToolsList(const TSharedPtr<FJsonObject>& Params);
	static TSharedPtr<FJsonObject> HandleToolsCall(const TSharedPtr<FJsonObject>& Params);


	// Helpers
	static TUniquePtr<FHttpServerResponse> MakeJsonResponse(const FString& Body, int32 StatusCode = 200);
	static TUniquePtr<FHttpServerResponse> MakeErrorResponse(int32 Code, const FString& Message, const TSharedPtr<FJsonValue>& Id);
	static FString SerializeJson(const TSharedPtr<FJsonObject>& Object);
	static TArray<TSharedPtr<FJsonValue>> BuildToolList();

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle RouteHandle;
	uint32 ListenPort = 8765;
};

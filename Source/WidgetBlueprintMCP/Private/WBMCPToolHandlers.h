#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Handles MCP tool/call dispatching for Widget Blueprint operations.
 */
class FWBMCPToolHandlers
{
public:
	static TSharedPtr<FJsonObject> Dispatch(const FString& ToolName, const TSharedPtr<FJsonObject>& Args);

private:
	// Read tools
	static TSharedPtr<FJsonObject> HandleListWidgetBlueprints(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleGetWidgetBlueprint(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleListFunctions(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleGetFunctionGraph(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleGetAnimation(const TSharedPtr<FJsonObject>& Args);

	// Write tools
	static TSharedPtr<FJsonObject> HandleModifyWidgetProperty(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleModifyNodePin(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleSaveWidgetBlueprint(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleCompileWidgetBlueprint(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleAddWidget(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleRemoveWidget(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleAddNode(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleConnectPins(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleDisconnectPins(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleRemoveNode(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleAddVariable(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleSetVariableDefault(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleModifyVariableFlags(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleRemoveVariable(const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> HandleExecuteBatch(const TSharedPtr<FJsonObject>& Args);

	// Helpers
	static TSharedPtr<FJsonObject> MakeTextResult(const FString& Text, bool bIsError = false);
	static TSharedPtr<FJsonObject> MakeJsonResult(const TSharedPtr<FJsonObject>& Data);
	static void SaveJsonSnapshot(const FString& FileName, const FString& JsonString);
};

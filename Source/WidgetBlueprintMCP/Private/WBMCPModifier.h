#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * GameThread-safe widget blueprint modification operations.
 * Returns empty string on success, error message on failure.
 */
class FWBMCPModifier
{
public:
    static FString ModifyWidgetProperty(const FString& AssetPath, const FString& WidgetName, const FString& PropertyName, const FString& Value);
    static FString ModifyNodePin(const FString& AssetPath, const FString& GraphName, const FString& NodeId, const FString& PinName, const FString& Value);
    static FString SaveWidgetBlueprint(const FString& AssetPath);

    static FString AddWidget(const FString& AssetPath, const FString& ParentWidgetName, const FString& WidgetClassName, const FString& NewWidgetName);
    static FString RemoveWidget(const FString& AssetPath, const FString& WidgetName);
    static FString AddNode(const FString& AssetPath, const FString& GraphName, const FString& NodeType, int32 X, int32 Y, const FString& ParamsJson, FString& OutNodeId);
    static FString ConnectPins(const FString& AssetPath, const FString& GraphName, const FString& SourceNodeId, const FString& SourcePinName, const FString& TargetNodeId, const FString& TargetPinName);
    static FString DisconnectPins(const FString& AssetPath, const FString& GraphName, const FString& SourceNodeId, const FString& SourcePinName, const FString& TargetNodeId, const FString& TargetPinName);
    static FString RemoveNode(const FString& AssetPath, const FString& GraphName, const FString& NodeId);
    static FString CompileWidgetBlueprint(const FString& AssetPath, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

    static FString AddVariable(const FString& AssetPath, const FString& VarName, const FString& VarType, const FString& Category);
    static FString SetVariableDefault(const FString& AssetPath, const FString& VarName, const FString& DefaultValue);
    static FString ModifyVariableFlags(const FString& AssetPath, const FString& VarName, bool bInstanceEditable, bool bExposeOnSpawn);
    static FString RemoveVariable(const FString& AssetPath, const FString& VarName);

private:
    static void RunOnGameThread(TFunction<void()> Work);
    static UEdGraph* FindGraph(UWidgetBlueprint* Blueprint, const FString& GraphName);
    static UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& NodeId);
};

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UWidget;
class UWidgetBlueprint;
class UEdGraph;
class UEdGraphNode;
struct FEdGraphPinType;

/**
 * Serializes UWidgetBlueprint data into JSON for Claude to understand.
 * Read-only — modifications are applied via targeted delta operations.
 */
class FWBMCPSerializer
{
public:
	// Full blueprint overview: widget tree + animations + variables + bindings + graph names
	static TSharedPtr<FJsonObject> WidgetBlueprintToJson(UWidgetBlueprint* Blueprint);

	// List of all function/event graph names in the blueprint
	static TSharedPtr<FJsonObject> GetFunctionList(UWidgetBlueprint* Blueprint);

	// Serialize a single graph (event graph or function) by name
	static TSharedPtr<FJsonObject> FunctionGraphToJson(UWidgetBlueprint* Blueprint, const FString& GraphName);

	// Detailed animation data: possessables, tracks, keys
	static TSharedPtr<FJsonObject> GetAnimationDetail(UWidgetBlueprint* Blueprint, const FString& AnimationName);

private:
	// Widget tree
	static TSharedPtr<FJsonObject> WidgetToJson(UWidget* Widget);
	static TSharedPtr<FJsonObject> CollectProperties(UObject* Object);

	// Blueprint-level data
	static TArray<TSharedPtr<FJsonValue>> SerializeAnimations(UWidgetBlueprint* Blueprint);
	static TArray<TSharedPtr<FJsonValue>> SerializeVariables(UWidgetBlueprint* Blueprint);
	static TArray<TSharedPtr<FJsonValue>> SerializeBindings(UWidgetBlueprint* Blueprint);

	// Graph serialization
	static TSharedPtr<FJsonObject> GraphToJson(UEdGraph* Graph);
	static TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node);
	static FString PinTypeToString(const FEdGraphPinType& PinType);
};

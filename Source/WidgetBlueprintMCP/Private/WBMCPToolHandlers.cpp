#include "WBMCPToolHandlers.h"
#include "WBMCPBatchExecutor.h"
#include "WBMCPSerializer.h"
#include "WBMCPModifier.h"
#include "WidgetBlueprintMCPModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WidgetBlueprint.h"

TSharedPtr<FJsonObject> FWBMCPToolHandlers::Dispatch(const FString& ToolName, const TSharedPtr<FJsonObject>& Args)
{
	if (ToolName == TEXT("list_widget_blueprints"))  return HandleListWidgetBlueprints(Args);
	if (ToolName == TEXT("get_widget_blueprint"))    return HandleGetWidgetBlueprint(Args);
	if (ToolName == TEXT("get_animation"))           return HandleGetAnimation(Args);
	if (ToolName == TEXT("list_functions"))          return HandleListFunctions(Args);
	if (ToolName == TEXT("get_function_graph"))      return HandleGetFunctionGraph(Args);
	if (ToolName == TEXT("modify_widget_property"))  return HandleModifyWidgetProperty(Args);
	if (ToolName == TEXT("modify_node_pin"))         return HandleModifyNodePin(Args);
	if (ToolName == TEXT("save_widget_blueprint"))   return HandleSaveWidgetBlueprint(Args);
	if (ToolName == TEXT("add_widget"))              return HandleAddWidget(Args);
	if (ToolName == TEXT("remove_widget"))           return HandleRemoveWidget(Args);
	if (ToolName == TEXT("add_node"))                return HandleAddNode(Args);
	if (ToolName == TEXT("connect_pins"))            return HandleConnectPins(Args);
	if (ToolName == TEXT("disconnect_pins"))         return HandleDisconnectPins(Args);
	if (ToolName == TEXT("remove_node"))             return HandleRemoveNode(Args);
	if (ToolName == TEXT("compile_widget_blueprint")) return HandleCompileWidgetBlueprint(Args);
	if (ToolName == TEXT("add_variable"))          return HandleAddVariable(Args);
	if (ToolName == TEXT("set_variable_default"))  return HandleSetVariableDefault(Args);
	if (ToolName == TEXT("modify_variable_flags")) return HandleModifyVariableFlags(Args);
	if (ToolName == TEXT("remove_variable"))       return HandleRemoveVariable(Args);
	if (ToolName == TEXT("execute_batch"))          return HandleExecuteBatch(Args);

	return MakeTextResult(FString::Printf(TEXT("Tool '%s' is not yet implemented."), *ToolName), true);
}

// ---------------------------------------------------------------------------
// Read tools
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleListWidgetBlueprints(const TSharedPtr<FJsonObject>& Args)
{
	FString PathFilter = TEXT("/Game/");
	if (Args.IsValid())
	{
		Args->TryGetStringField(TEXT("path_filter"), PathFilter);
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter Filter;
	Filter.ClassNames.Add(UWidgetBlueprint::StaticClass()->GetFName());
	Filter.PackagePaths.Add(*PathFilter);
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FAssetData& Asset : Assets)
	{
		Items.Add(MakeShared<FJsonValueString>(Asset.PackageName.ToString()));
	}

	FString ItemsJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ItemsJson);
	FJsonSerializer::Serialize(Items, Writer);

	return MakeTextResult(ItemsJson);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleGetWidgetBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return MakeTextResult(TEXT("Missing required argument: asset_path"), true);
	}

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return MakeTextResult(FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath), true);
	}

	TSharedPtr<FJsonObject> Data = FWBMCPSerializer::WidgetBlueprintToJson(Blueprint);
	if (!Data.IsValid())
	{
		return MakeTextResult(TEXT("Failed to serialize Widget Blueprint"), true);
	}

	// Save snapshot for human inspection
	FString SnapshotJson;
	TSharedRef<TJsonWriter<>> SnapWriter = TJsonWriterFactory<>::Create(&SnapshotJson);
	FJsonSerializer::Serialize(Data.ToSharedRef(), SnapWriter);
	FString SnapFileName = AssetPath.Replace(TEXT("/"), TEXT("_")) + TEXT(".json");
	SaveJsonSnapshot(SnapFileName, SnapshotJson);

	return MakeJsonResult(Data);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleListFunctions(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return MakeTextResult(TEXT("Missing required argument: asset_path"), true);
	}

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return MakeTextResult(FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath), true);
	}

	TSharedPtr<FJsonObject> Data = FWBMCPSerializer::GetFunctionList(Blueprint);
	return MakeJsonResult(Data);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleGetFunctionGraph(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	FString GraphName;

	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("graph_name"), GraphName))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, graph_name"), true);
	}

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return MakeTextResult(FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath), true);
	}

	TSharedPtr<FJsonObject> Data = FWBMCPSerializer::FunctionGraphToJson(Blueprint, GraphName);
	if (!Data.IsValid())
	{
		TArray<FString> Available;
		for (UEdGraph* G : Blueprint->UbergraphPages) { if (G) Available.Add(G->GetName()); }
		for (UEdGraph* G : Blueprint->FunctionGraphs)  { if (G) Available.Add(G->GetName()); }
		return MakeTextResult(FString::Printf(TEXT("Graph not found: '%s'. Available: %s"),
			*GraphName, *FString::Join(Available, TEXT(", "))), true);
	}

	// Save snapshot for human inspection
	FString SnapshotJson;
	TSharedRef<TJsonWriter<>> SnapWriter = TJsonWriterFactory<>::Create(&SnapshotJson);
	FJsonSerializer::Serialize(Data.ToSharedRef(), SnapWriter);
	FString SnapFileName = AssetPath.Replace(TEXT("/"), TEXT("_")) + TEXT("__") + GraphName + TEXT(".json");
	SaveJsonSnapshot(SnapFileName, SnapshotJson);

	return MakeJsonResult(Data);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleGetAnimation(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, AnimationName;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("animation_name"), AnimationName))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, animation_name"), true);
	}

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return MakeTextResult(FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath), true);
	}

	TSharedPtr<FJsonObject> Data = FWBMCPSerializer::GetAnimationDetail(Blueprint, AnimationName);
	if (!Data.IsValid())
	{
		TArray<FString> Available;
		for (UWidgetAnimation* Anim : Blueprint->Animations)
		{
			if (Anim) Available.Add(Anim->GetName());
		}
		return MakeTextResult(FString::Printf(TEXT("Animation not found: '%s'. Available: %s"),
			*AnimationName, *FString::Join(Available, TEXT(", "))), true);
	}

	// Save snapshot
	FString SnapshotJson;
	TSharedRef<TJsonWriter<>> SnapWriter = TJsonWriterFactory<>::Create(&SnapshotJson);
	FJsonSerializer::Serialize(Data.ToSharedRef(), SnapWriter);
	FString SnapFileName = AssetPath.Replace(TEXT("/"), TEXT("_")) + TEXT("__anim__") + AnimationName + TEXT(".json");
	SaveJsonSnapshot(SnapFileName, SnapshotJson);

	return MakeJsonResult(Data);
}

// ---------------------------------------------------------------------------
// Write tools
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleModifyWidgetProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, WidgetName, PropertyName, Value;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("widget_name"), WidgetName)
		|| !Args->TryGetStringField(TEXT("property_name"), PropertyName)
		|| !Args->TryGetStringField(TEXT("value"), Value))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, widget_name, property_name, value"), true);
	}

	FString Error = FWBMCPModifier::ModifyWidgetProperty(AssetPath, WidgetName, PropertyName, Value);
	if (!Error.IsEmpty())
	{
		return MakeTextResult(Error, true);
	}
	return MakeTextResult(TEXT("OK"));
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleModifyNodePin(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, GraphName, NodeId, PinName, Value;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("graph_name"), GraphName)
		|| !Args->TryGetStringField(TEXT("node_id"), NodeId)
		|| !Args->TryGetStringField(TEXT("pin_name"), PinName)
		|| !Args->TryGetStringField(TEXT("value"), Value))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, graph_name, node_id, pin_name, value"), true);
	}

	FString Error = FWBMCPModifier::ModifyNodePin(AssetPath, GraphName, NodeId, PinName, Value);
	if (!Error.IsEmpty())
	{
		return MakeTextResult(Error, true);
	}
	return MakeTextResult(TEXT("OK"));
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleSaveWidgetBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return MakeTextResult(TEXT("Missing required argument: asset_path"), true);
	}

	FString Error = FWBMCPModifier::SaveWidgetBlueprint(AssetPath);
	if (!Error.IsEmpty())
	{
		return MakeTextResult(Error, true);
	}
	return MakeTextResult(TEXT("OK"));
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleCompileWidgetBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return MakeTextResult(TEXT("Missing required argument: asset_path"), true);
	}

	TArray<FString> Errors, Warnings;
	FString Error = FWBMCPModifier::CompileWidgetBlueprint(AssetPath, Errors, Warnings);
	if (!Error.IsEmpty() && Error != TEXT("COMPILE_ERROR"))
	{
		return MakeTextResult(Error, true);
	}

	bool bSuccess = (Error != TEXT("COMPILE_ERROR"));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bSuccess);

	TArray<TSharedPtr<FJsonValue>> ErrorsArr, WarningsArr;
	for (const FString& E : Errors)   ErrorsArr.Add(MakeShared<FJsonValueString>(E));
	for (const FString& W : Warnings) WarningsArr.Add(MakeShared<FJsonValueString>(W));
	Result->SetArrayField(TEXT("errors"), ErrorsArr);
	Result->SetArrayField(TEXT("warnings"), WarningsArr);

	FString Summary = bSuccess
		? FString::Printf(TEXT("Compile OK. Warnings: %d"), Warnings.Num())
		: FString::Printf(TEXT("Compile FAILED. Errors: %d, Warnings: %d"), Errors.Num(), Warnings.Num());
	Result->SetStringField(TEXT("summary"), Summary);

	return MakeJsonResult(Result);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleAddWidget(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, ParentWidgetName, WidgetClassName, NewWidgetName;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("widget_class"), WidgetClassName)
		|| !Args->TryGetStringField(TEXT("widget_name"), NewWidgetName))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, widget_class, widget_name"), true);
	}
	Args->TryGetStringField(TEXT("parent_widget"), ParentWidgetName);

	FString Error = FWBMCPModifier::AddWidget(AssetPath, ParentWidgetName, WidgetClassName, NewWidgetName);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleRemoveWidget(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, WidgetName;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, widget_name"), true);
	}

	FString Error = FWBMCPModifier::RemoveWidget(AssetPath, WidgetName);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleAddNode(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, GraphName, NodeType, ParamsJson;
	double XDouble = 0.0, YDouble = 0.0;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("graph_name"), GraphName)
		|| !Args->TryGetStringField(TEXT("node_type"), NodeType))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, graph_name, node_type"), true);
	}
	Args->TryGetNumberField(TEXT("x"), XDouble);
	Args->TryGetNumberField(TEXT("y"), YDouble);
	Args->TryGetStringField(TEXT("params"), ParamsJson);

	FString OutNodeId;
	FString Error = FWBMCPModifier::AddNode(AssetPath, GraphName, NodeType, (int32)XDouble, (int32)YDouble, ParamsJson, OutNodeId);
	if (!Error.IsEmpty()) return MakeTextResult(Error, true);
	return MakeTextResult(FString::Printf(TEXT("OK node_id=%s"), *OutNodeId));
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleConnectPins(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, GraphName, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("graph_name"), GraphName)
		|| !Args->TryGetStringField(TEXT("source_node_id"), SourceNodeId)
		|| !Args->TryGetStringField(TEXT("source_pin"), SourcePinName)
		|| !Args->TryGetStringField(TEXT("target_node_id"), TargetNodeId)
		|| !Args->TryGetStringField(TEXT("target_pin"), TargetPinName))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, graph_name, source_node_id, source_pin, target_node_id, target_pin"), true);
	}

	FString Error = FWBMCPModifier::ConnectPins(AssetPath, GraphName, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleDisconnectPins(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, GraphName, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("graph_name"), GraphName)
		|| !Args->TryGetStringField(TEXT("source_node_id"), SourceNodeId)
		|| !Args->TryGetStringField(TEXT("source_pin"), SourcePinName)
		|| !Args->TryGetStringField(TEXT("target_node_id"), TargetNodeId)
		|| !Args->TryGetStringField(TEXT("target_pin"), TargetPinName))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, graph_name, source_node_id, source_pin, target_node_id, target_pin"), true);
	}

	FString Error = FWBMCPModifier::DisconnectPins(AssetPath, GraphName, SourceNodeId, SourcePinName, TargetNodeId, TargetPinName);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleRemoveNode(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, GraphName, NodeId;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("graph_name"), GraphName)
		|| !Args->TryGetStringField(TEXT("node_id"), NodeId))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, graph_name, node_id"), true);
	}

	FString Error = FWBMCPModifier::RemoveNode(AssetPath, GraphName, NodeId);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleAddVariable(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, VarName, VarType, Category;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("var_name"), VarName)
		|| !Args->TryGetStringField(TEXT("var_type"), VarType))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, var_name, var_type"), true);
	}
	Args->TryGetStringField(TEXT("category"), Category);

	FString Error = FWBMCPModifier::AddVariable(AssetPath, VarName, VarType, Category);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleSetVariableDefault(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, VarName, DefaultValue;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("var_name"), VarName)
		|| !Args->TryGetStringField(TEXT("default_value"), DefaultValue))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, var_name, default_value"), true);
	}

	FString Error = FWBMCPModifier::SetVariableDefault(AssetPath, VarName, DefaultValue);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleModifyVariableFlags(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, VarName;
	bool bInstanceEditable = true, bExposeOnSpawn = false;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("var_name"), VarName))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, var_name"), true);
	}
	Args->TryGetBoolField(TEXT("instance_editable"), bInstanceEditable);
	Args->TryGetBoolField(TEXT("expose_on_spawn"), bExposeOnSpawn);

	FString Error = FWBMCPModifier::ModifyVariableFlags(AssetPath, VarName, bInstanceEditable, bExposeOnSpawn);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleRemoveVariable(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, VarName;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("asset_path"), AssetPath)
		|| !Args->TryGetStringField(TEXT("var_name"), VarName))
	{
		return MakeTextResult(TEXT("Missing required arguments: asset_path, var_name"), true);
	}

	FString Error = FWBMCPModifier::RemoveVariable(AssetPath, VarName);
	return Error.IsEmpty() ? MakeTextResult(TEXT("OK")) : MakeTextResult(Error, true);
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::HandleExecuteBatch(const TSharedPtr<FJsonObject>& Args)
{
	return FWBMCPBatchExecutor::Execute(Args);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FWBMCPToolHandlers::MakeTextResult(const FString& Text, bool bIsError)
{
	TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
	Content->SetStringField(TEXT("type"), TEXT("text"));
	Content->SetStringField(TEXT("text"), Text);

	TArray<TSharedPtr<FJsonValue>> ContentArray;
	ContentArray.Add(MakeShared<FJsonValueObject>(Content));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("content"), ContentArray);
	Result->SetBoolField(TEXT("isError"), bIsError);
	return Result;
}

TSharedPtr<FJsonObject> FWBMCPToolHandlers::MakeJsonResult(const TSharedPtr<FJsonObject>& Data)
{
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Data.ToSharedRef(), Writer);
	return MakeTextResult(JsonString);
}

void FWBMCPToolHandlers::SaveJsonSnapshot(const FString& FileName, const FString& JsonString)
{
	FString Dir = FPaths::ProjectSavedDir() / TEXT("WidgetBlueprintMCP");
	IFileManager::Get().MakeDirectory(*Dir, true);
	FString FilePath = Dir / FileName;
	FFileHelper::SaveStringToFile(JsonString, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

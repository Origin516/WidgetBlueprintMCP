#include "WBMCPModifier.h"

#include "Async/Async.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Containers/Ticker.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

void FWBMCPModifier::RunOnGameThread(TFunction<void()> Work)
{
    if (IsInGameThread())
    {
        Work();
        return;
    }

    FEvent* DoneEvent = FPlatformProcess::CreateSynchEvent(true);
    AsyncTask(ENamedThreads::GameThread, [Work = MoveTemp(Work), DoneEvent]()
    {
        Work();
        DoneEvent->Trigger();
    });
    DoneEvent->Wait();
    delete DoneEvent;
}

FString FWBMCPModifier::ModifyWidgetProperty(
    const FString& AssetPath,
    const FString& WidgetName,
    const FString& PropertyName,
    const FString& Value)
{
    FString Result;
    RunOnGameThread([&]()
    {
        UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
        if (!Blueprint)
        {
            Result = FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath);
            return;
        }

        UWidget* TargetWidget = nullptr;
        Blueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            if (!TargetWidget && Widget->GetName() == WidgetName)
            {
                TargetWidget = Widget;
            }
        });

        if (!TargetWidget)
        {
            Result = FString::Printf(TEXT("Widget not found: %s"), *WidgetName);
            return;
        }

        FProperty* Prop = FindFProperty<FProperty>(TargetWidget->GetClass(), *PropertyName);
        if (!Prop)
        {
            Result = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
            return;
        }

        if (!Prop->HasAnyPropertyFlags(CPF_Edit))
        {
            Result = FString::Printf(TEXT("Property is not editable: %s"), *PropertyName);
            return;
        }

        void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(TargetWidget);
        if (!Prop->ImportText(*Value, ValuePtr, PPF_None, TargetWidget))
        {
            Result = FString::Printf(TEXT("Failed to import value for property: %s"), *PropertyName);
            return;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    });
    return Result;
}

FString FWBMCPModifier::ModifyNodePin(
    const FString& AssetPath,
    const FString& GraphName,
    const FString& NodeId,
    const FString& PinName,
    const FString& Value)
{
    FString Result;
    RunOnGameThread([&]()
    {
        UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
        if (!Blueprint)
        {
            Result = FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath);
            return;
        }

        UEdGraph* TargetGraph = FindGraph(Blueprint, GraphName);
        if (!TargetGraph)
        {
            Result = FString::Printf(TEXT("Graph not found: %s"), *GraphName);
            return;
        }

        UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, NodeId);
        if (!TargetNode)
        {
            Result = FString::Printf(TEXT("Node not found: %s"), *NodeId);
            return;
        }

        UEdGraphPin* TargetPin = nullptr;
        for (UEdGraphPin* Pin : TargetNode->Pins)
        {
            if (Pin && Pin->PinName.ToString() == PinName)
            {
                TargetPin = Pin;
                break;
            }
        }

        if (!TargetPin)
        {
            Result = FString::Printf(TEXT("Pin not found: %s"), *PinName);
            return;
        }

        TargetPin->DefaultValue = Value;

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    });
    return Result;
}

FString FWBMCPModifier::SaveWidgetBlueprint(const FString& AssetPath)
{
    // SavePackage must not block the HTTP handler tick (re-entrant deadlock).
    // Defer to next GameThread tick and return immediately.
    FString CapturedPath = AssetPath;
    RunOnGameThread([CapturedPath]()
    {
        FTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([CapturedPath](float) -> bool
            {
                UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *CapturedPath);
                if (Blueprint)
                {
                    UPackage* Pkg = Blueprint->GetOutermost();
                    FString Filename = FPackageName::LongPackageNameToFilename(
                        Pkg->GetName(),
                        FPackageName::GetAssetPackageExtension());
                    // Release memory-mapped file handle, force writable (no source control checkout)
                    ResetLoaders(Pkg);
                    FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, false);
                    UPackage::SavePackage(Pkg, Blueprint, RF_Public | RF_Standalone, *Filename,
                        nullptr, nullptr, false, true, SAVE_None, nullptr, FDateTime::MinValue(), false);
                }
                return false;
            }),
            0.0f);
    });
    return TEXT("");
}

UEdGraph* FWBMCPModifier::FindGraph(UWidgetBlueprint* Blueprint, const FString& GraphName)
{
	for (UEdGraph* G : Blueprint->UbergraphPages) { if (G && G->GetName() == GraphName) return G; }
	for (UEdGraph* G : Blueprint->FunctionGraphs)  { if (G && G->GetName() == GraphName) return G; }
	for (UEdGraph* G : Blueprint->MacroGraphs)     { if (G && G->GetName() == GraphName) return G; }
	return nullptr;
}

UEdGraphNode* FWBMCPModifier::FindNodeByGuid(UEdGraph* Graph, const FString& NodeId)
{
	FGuid Guid;
	if (!FGuid::Parse(NodeId, Guid)) return nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == Guid) return Node;
	}
	return nullptr;
}

FString FWBMCPModifier::AddWidget(
	const FString& AssetPath,
	const FString& ParentWidgetName,
	const FString& WidgetClassName,
	const FString& NewWidgetName)
{
	FString Result;
	RunOnGameThread([&]()
	{
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		if (!Blueprint) { Result = FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath); return; }

		UClass* WidgetClass = FindObject<UClass>(ANY_PACKAGE, *WidgetClassName);
		if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
		{
			Result = FString::Printf(TEXT("Widget class not found: %s"), *WidgetClassName);
			return;
		}

		UWidget* NewWidget = Blueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*NewWidgetName));
		if (!NewWidget) { Result = TEXT("Failed to construct widget"); return; }

		if (ParentWidgetName.IsEmpty())
		{
			Blueprint->WidgetTree->RootWidget = NewWidget;
		}
		else
		{
			UWidget* ParentWidget = nullptr;
			Blueprint->WidgetTree->ForEachWidget([&](UWidget* W)
			{
				if (!ParentWidget && W->GetName() == ParentWidgetName) ParentWidget = W;
			});
			if (!ParentWidget) { Result = FString::Printf(TEXT("Parent widget not found: %s"), *ParentWidgetName); return; }

			UPanelWidget* Panel = Cast<UPanelWidget>(ParentWidget);
			if (!Panel) { Result = FString::Printf(TEXT("Not a panel widget: %s"), *ParentWidgetName); return; }

			Panel->AddChild(NewWidget);
		}

		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->MarkPackageDirty();
	});
	return Result;
}

FString FWBMCPModifier::RemoveWidget(const FString& AssetPath, const FString& WidgetName)
{
	FString Result;
	RunOnGameThread([&]()
	{
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		if (!Blueprint) { Result = FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath); return; }

		UWidget* Target = nullptr;
		Blueprint->WidgetTree->ForEachWidget([&](UWidget* W)
		{
			if (!Target && W->GetName() == WidgetName) Target = W;
		});
		if (!Target) { Result = FString::Printf(TEXT("Widget not found: %s"), *WidgetName); return; }

		Blueprint->WidgetTree->RemoveWidget(Target);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->MarkPackageDirty();
	});
	return Result;
}

FString FWBMCPModifier::AddNode(
	const FString& AssetPath,
	const FString& GraphName,
	const FString& NodeType,
	int32 X,
	int32 Y,
	const FString& ParamsJson,
	FString& OutNodeId)
{
	FString Result;
	RunOnGameThread([&]()
	{
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		if (!Blueprint) { Result = FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath); return; }

		UEdGraph* TargetGraph = FindGraph(Blueprint, GraphName);
		if (!TargetGraph) { Result = FString::Printf(TEXT("Graph not found: %s"), *GraphName); return; }

		// Parse extra params
		TSharedPtr<FJsonObject> Params;
		if (!ParamsJson.IsEmpty())
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ParamsJson);
			FJsonSerializer::Deserialize(Reader, Params);
		}
		if (!Params.IsValid()) Params = MakeShared<FJsonObject>();

		UEdGraphNode* NewNode = nullptr;

		if (NodeType == TEXT("CallFunction"))
		{
			FString FunctionClassName, FunctionName;
			Params->TryGetStringField(TEXT("function_class"), FunctionClassName);
			Params->TryGetStringField(TEXT("function_name"), FunctionName);

			UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(TargetGraph);
			UClass* FuncClass = !FunctionClassName.IsEmpty() ? FindObject<UClass>(ANY_PACKAGE, *FunctionClassName) : nullptr;
			if (FuncClass)
			{
				UFunction* Func = FuncClass->FindFunctionByName(*FunctionName);
				if (Func) CallNode->SetFromFunction(Func);
			}
			NewNode = CallNode;
		}
		else if (NodeType == TEXT("VariableGet"))
		{
			FString VarName;
			Params->TryGetStringField(TEXT("variable_name"), VarName);

			UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(TargetGraph);
			FMemberReference Ref;
			Ref.SetSelfMember(FName(*VarName));
			VarNode->VariableReference = Ref;
			NewNode = VarNode;
		}
		else if (NodeType == TEXT("VariableSet"))
		{
			FString VarName;
			Params->TryGetStringField(TEXT("variable_name"), VarName);

			UK2Node_VariableSet* VarNode = NewObject<UK2Node_VariableSet>(TargetGraph);
			FMemberReference Ref;
			Ref.SetSelfMember(FName(*VarName));
			VarNode->VariableReference = Ref;
			NewNode = VarNode;
		}
		else
		{
			Result = FString::Printf(TEXT("Unsupported node type: %s"), *NodeType);
			return;
		}

		TargetGraph->AddNode(NewNode, false, false);
		NewNode->CreateNewGuid();
		NewNode->PostPlacedNewNode();
		NewNode->AllocateDefaultPins();
		NewNode->NodePosX = X;
		NewNode->NodePosY = Y;

		OutNodeId = NewNode->NodeGuid.ToString();

		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->MarkPackageDirty();
	});
	return Result;
}

FString FWBMCPModifier::ConnectPins(
	const FString& AssetPath,
	const FString& GraphName,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName)
{
	FString Result;
	RunOnGameThread([&]()
	{
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		if (!Blueprint) { Result = FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath); return; }

		UEdGraph* Graph = FindGraph(Blueprint, GraphName);
		if (!Graph) { Result = FString::Printf(TEXT("Graph not found: %s"), *GraphName); return; }

		UEdGraphNode* SourceNode = FindNodeByGuid(Graph, SourceNodeId);
		if (!SourceNode) { Result = FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId); return; }

		UEdGraphNode* TargetNode = FindNodeByGuid(Graph, TargetNodeId);
		if (!TargetNode) { Result = FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId); return; }

		UEdGraphPin* SourcePin = nullptr;
		UEdGraphPin* TargetPin = nullptr;
		for (UEdGraphPin* P : SourceNode->Pins) { if (P && P->PinName.ToString() == SourcePinName) { SourcePin = P; break; } }
		for (UEdGraphPin* P : TargetNode->Pins) { if (P && P->PinName.ToString() == TargetPinName) { TargetPin = P; break; } }

		if (!SourcePin) { Result = FString::Printf(TEXT("Source pin not found: %s"), *SourcePinName); return; }
		if (!TargetPin) { Result = FString::Printf(TEXT("Target pin not found: %s"), *TargetPinName); return; }

		const UEdGraphSchema* Schema = Graph->GetSchema();
		FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			Result = FString::Printf(TEXT("Cannot connect: %s"), *Response.Message.ToString());
			return;
		}

		Schema->TryCreateConnection(SourcePin, TargetPin);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->MarkPackageDirty();
	});
	return Result;
}

FString FWBMCPModifier::DisconnectPins(
	const FString& AssetPath,
	const FString& GraphName,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName)
{
	FString Result;
	RunOnGameThread([&]()
	{
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		if (!Blueprint) { Result = FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath); return; }

		UEdGraph* Graph = FindGraph(Blueprint, GraphName);
		if (!Graph) { Result = FString::Printf(TEXT("Graph not found: %s"), *GraphName); return; }

		UEdGraphNode* SourceNode = FindNodeByGuid(Graph, SourceNodeId);
		if (!SourceNode) { Result = FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId); return; }

		UEdGraphNode* TargetNode = FindNodeByGuid(Graph, TargetNodeId);
		if (!TargetNode) { Result = FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId); return; }

		UEdGraphPin* SourcePin = nullptr;
		UEdGraphPin* TargetPin = nullptr;
		for (UEdGraphPin* P : SourceNode->Pins) { if (P && P->PinName.ToString() == SourcePinName) { SourcePin = P; break; } }
		for (UEdGraphPin* P : TargetNode->Pins) { if (P && P->PinName.ToString() == TargetPinName) { TargetPin = P; break; } }

		if (!SourcePin) { Result = FString::Printf(TEXT("Source pin not found: %s"), *SourcePinName); return; }
		if (!TargetPin) { Result = FString::Printf(TEXT("Target pin not found: %s"), *TargetPinName); return; }

		const UEdGraphSchema* Schema = Graph->GetSchema();
		Schema->BreakSinglePinLink(SourcePin, TargetPin);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->MarkPackageDirty();
	});
	return Result;
}

FString FWBMCPModifier::RemoveNode(
	const FString& AssetPath,
	const FString& GraphName,
	const FString& NodeId)
{
	FString Result;
	RunOnGameThread([&]()
	{
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		if (!Blueprint) { Result = FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath); return; }

		UEdGraph* Graph = FindGraph(Blueprint, GraphName);
		if (!Graph) { Result = FString::Printf(TEXT("Graph not found: %s"), *GraphName); return; }

		UEdGraphNode* Node = FindNodeByGuid(Graph, NodeId);
		if (!Node) { Result = FString::Printf(TEXT("Node not found: %s"), *NodeId); return; }

		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		Blueprint->MarkPackageDirty();
	});
	return Result;
}

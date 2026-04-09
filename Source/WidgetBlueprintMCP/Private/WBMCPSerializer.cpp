#include "WBMCPSerializer.h"

#include "WidgetBlueprint.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "UObject/UnrealType.h"

// ---------------------------------------------------------------------------
// Public — full blueprint overview
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FWBMCPSerializer::WidgetBlueprintToJson(UWidgetBlueprint* Blueprint)
{
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());

	// Widget tree
	UWidget* RootWidget = Blueprint->WidgetTree->RootWidget;
	if (RootWidget)
	{
		Result->SetObjectField(TEXT("rootWidget"), WidgetToJson(RootWidget));
	}

	// Animations
	Result->SetArrayField(TEXT("animations"), SerializeAnimations(Blueprint));

	// Variables
	Result->SetArrayField(TEXT("variables"), SerializeVariables(Blueprint));

	// Property bindings
	Result->SetArrayField(TEXT("bindings"), SerializeBindings(Blueprint));

	// Graph name lists (full node data fetched separately via get_function_graph)
	TArray<TSharedPtr<FJsonValue>> EventGraphNames;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph)
		{
			EventGraphNames.Add(MakeShared<FJsonValueString>(Graph->GetName()));
		}
	}
	Result->SetArrayField(TEXT("eventGraphs"), EventGraphNames);

	TArray<TSharedPtr<FJsonValue>> FunctionNames;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			FunctionNames.Add(MakeShared<FJsonValueString>(Graph->GetName()));
		}
	}
	Result->SetArrayField(TEXT("functions"), FunctionNames);

	return Result;
}

// ---------------------------------------------------------------------------
// Public — function list
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FWBMCPSerializer::GetFunctionList(UWidgetBlueprint* Blueprint)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	if (!Blueprint)
	{
		return Result;
	}

	TArray<TSharedPtr<FJsonValue>> EventGraphs;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}
		TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
		Info->SetStringField(TEXT("name"), Graph->GetName());
		Info->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
		EventGraphs.Add(MakeShared<FJsonValueObject>(Info));
	}
	Result->SetArrayField(TEXT("eventGraphs"), EventGraphs);

	TArray<TSharedPtr<FJsonValue>> Functions;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}
		TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
		Info->SetStringField(TEXT("name"), Graph->GetName());
		Info->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
		Functions.Add(MakeShared<FJsonValueObject>(Info));
	}
	Result->SetArrayField(TEXT("functions"), Functions);

	TArray<TSharedPtr<FJsonValue>> Macros;
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (!Graph)
		{
			continue;
		}
		TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
		Info->SetStringField(TEXT("name"), Graph->GetName());
		Info->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
		Macros.Add(MakeShared<FJsonValueObject>(Info));
	}
	Result->SetArrayField(TEXT("macros"), Macros);

	return Result;
}

// ---------------------------------------------------------------------------
// Public — single graph serialization
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FWBMCPSerializer::FunctionGraphToJson(UWidgetBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// Search event graphs first, then function graphs, then macros
	auto FindGraph = [&GraphName](const TArray<UEdGraph*>& Graphs) -> UEdGraph*
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				return Graph;
			}
		}
		return nullptr;
	};

	UEdGraph* TargetGraph = FindGraph(Blueprint->UbergraphPages);
	if (!TargetGraph)
	{
		TargetGraph = FindGraph(Blueprint->FunctionGraphs);
	}
	if (!TargetGraph)
	{
		TargetGraph = FindGraph(Blueprint->MacroGraphs);
	}

	if (!TargetGraph)
	{
		return nullptr;
	}

	return GraphToJson(TargetGraph);
}

// ---------------------------------------------------------------------------
// Private — widget tree
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FWBMCPSerializer::WidgetToJson(UWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), Widget->GetName());
	Result->SetStringField(TEXT("type"), Widget->GetClass()->GetName());
	Result->SetObjectField(TEXT("properties"), CollectProperties(Widget));

	if (Widget->Slot)
	{
		TSharedPtr<FJsonObject> SlotJson = MakeShared<FJsonObject>();
		SlotJson->SetStringField(TEXT("type"), Widget->Slot->GetClass()->GetName());
		SlotJson->SetObjectField(TEXT("properties"), CollectProperties(Widget->Slot));
		Result->SetObjectField(TEXT("slot"), SlotJson);
	}

	UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
	if (Panel)
	{
		TArray<TSharedPtr<FJsonValue>> Children;
		int32 ChildCount = Panel->GetChildrenCount();
		for (int32 i = 0; i < ChildCount; ++i)
		{
			UWidget* Child = Panel->GetChildAt(i);
			if (Child)
			{
				Children.Add(MakeShared<FJsonValueObject>(WidgetToJson(Child)));
			}
		}
		Result->SetArrayField(TEXT("children"), Children);
	}

	return Result;
}

TSharedPtr<FJsonObject> FWBMCPSerializer::CollectProperties(UObject* Object)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	if (!Object)
	{
		return Result;
	}

	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		FString ValueString;
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		Property->ExportTextItem(ValueString, ValuePtr, nullptr, Object, PPF_None);

		if (ValueString.IsEmpty())
		{
			continue;
		}

		Result->SetStringField(Property->GetName(), ValueString);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Private — blueprint-level data
// ---------------------------------------------------------------------------

TArray<TSharedPtr<FJsonValue>> FWBMCPSerializer::SerializeAnimations(UWidgetBlueprint* Blueprint)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (UWidgetAnimation* Anim : Blueprint->Animations)
	{
		if (!Anim)
		{
			continue;
		}
		TSharedPtr<FJsonObject> AnimJson = MakeShared<FJsonObject>();
		AnimJson->SetStringField(TEXT("name"), Anim->GetName());

		if (Anim->MovieScene)
		{
			TRange<FFrameNumber> Range = Anim->MovieScene->GetPlaybackRange();
			if (Range.HasLowerBound() && Range.HasUpperBound())
			{
				FFrameNumber DurationFrames = Range.GetUpperBoundValue() - Range.GetLowerBoundValue();
				double TickResolution = Anim->MovieScene->GetTickResolution().AsDecimal();
				float DurationSeconds = TickResolution > 0.0 ? (float)(DurationFrames.Value / TickResolution) : 0.f;
				AnimJson->SetNumberField(TEXT("durationSeconds"), DurationSeconds);
			}
			AnimJson->SetNumberField(TEXT("trackCount"), Anim->MovieScene->GetMasterTracks().Num() + Anim->MovieScene->GetPossessableCount());
		}

		Result.Add(MakeShared<FJsonValueObject>(AnimJson));
	}
	return Result;
}

TArray<TSharedPtr<FJsonValue>> FWBMCPSerializer::SerializeVariables(UWidgetBlueprint* Blueprint)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarJson = MakeShared<FJsonObject>();
		VarJson->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarJson->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		VarJson->SetStringField(TEXT("category"), Var.Category.ToString());

		if (!Var.DefaultValue.IsEmpty())
		{
			VarJson->SetStringField(TEXT("defaultValue"), Var.DefaultValue);
		}

		bool bIsExposed = (Var.PropertyFlags & CPF_ExposeOnSpawn) != 0;
		bool bIsInstanceEditable = (Var.PropertyFlags & CPF_DisableEditOnInstance) == 0;
		VarJson->SetBoolField(TEXT("isExposed"), bIsExposed);
		VarJson->SetBoolField(TEXT("isInstanceEditable"), bIsInstanceEditable);

		Result.Add(MakeShared<FJsonValueObject>(VarJson));
	}
	return Result;
}

TArray<TSharedPtr<FJsonValue>> FWBMCPSerializer::SerializeBindings(UWidgetBlueprint* Blueprint)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FDelegateEditorBinding& Binding : Blueprint->Bindings)
	{
		TSharedPtr<FJsonObject> BindJson = MakeShared<FJsonObject>();
		BindJson->SetStringField(TEXT("widget"), Binding.ObjectName);
		BindJson->SetStringField(TEXT("property"), Binding.PropertyName.ToString());
		BindJson->SetStringField(TEXT("function"), Binding.FunctionName.ToString());
		Result.Add(MakeShared<FJsonValueObject>(BindJson));
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Private — graph serialization
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FWBMCPSerializer::GraphToJson(UEdGraph* Graph)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), Graph->GetName());

	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node)
		{
			Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
		}
	}
	Result->SetArrayField(TEXT("nodes"), Nodes);

	return Result;
}

TSharedPtr<FJsonObject> FWBMCPSerializer::NodeToJson(UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
	Result->SetStringField(TEXT("type"), Node->GetClass()->GetName());
	Result->SetNumberField(TEXT("x"), Node->NodePosX);
	Result->SetNumberField(TEXT("y"), Node->NodePosY);

	if (!Node->NodeComment.IsEmpty())
	{
		Result->SetStringField(TEXT("comment"), Node->NodeComment);
	}

	// Node title (human-readable)
	Result->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

	// Additional info for common node types
	if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
	{
		Result->SetStringField(TEXT("functionName"), CallNode->FunctionReference.GetMemberName().ToString());
		UClass* FuncClass = CallNode->FunctionReference.GetMemberParentClass();
		if (FuncClass)
		{
			Result->SetStringField(TEXT("functionClass"), FuncClass->GetName());
		}
	}
	else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		Result->SetStringField(TEXT("eventName"), EventNode->EventReference.GetMemberName().ToString());
	}
	else if (UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(Node))
	{
		Result->SetStringField(TEXT("variableName"), VarGet->VariableReference.GetMemberName().ToString());
	}
	else if (UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(Node))
	{
		Result->SetStringField(TEXT("variableName"), VarSet->VariableReference.GetMemberName().ToString());
	}

	// Pins
	TArray<TSharedPtr<FJsonValue>> Pins;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
		PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
		PinJson->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
		PinJson->SetStringField(TEXT("type"), PinTypeToString(Pin->PinType));

		if (!Pin->DefaultValue.IsEmpty())
		{
			PinJson->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
		}
		if (!Pin->DefaultTextValue.IsEmpty())
		{
			PinJson->SetStringField(TEXT("defaultTextValue"), Pin->DefaultTextValue.ToString());
		}

		// Connections: "nodeGuid.pinName"
		if (Pin->LinkedTo.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Links;
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->GetOwningNode())
				{
					FString LinkId = FString::Printf(TEXT("%s.%s"),
						*LinkedPin->GetOwningNode()->NodeGuid.ToString(),
						*LinkedPin->PinName.ToString());
					Links.Add(MakeShared<FJsonValueString>(LinkId));
				}
			}
			PinJson->SetArrayField(TEXT("linkedTo"), Links);
		}

		Pins.Add(MakeShared<FJsonValueObject>(PinJson));
	}
	Result->SetArrayField(TEXT("pins"), Pins);

	return Result;
}

FString FWBMCPSerializer::PinTypeToString(const FEdGraphPinType& PinType)
{
	FString TypeStr = PinType.PinCategory.ToString();
	if (!PinType.PinSubCategory.IsNone())
	{
		TypeStr += TEXT("/") + PinType.PinSubCategory.ToString();
	}
	if (PinType.PinSubCategoryObject.IsValid())
	{
		TypeStr += TEXT("(") + PinType.PinSubCategoryObject->GetName() + TEXT(")");
	}
	return TypeStr;
}

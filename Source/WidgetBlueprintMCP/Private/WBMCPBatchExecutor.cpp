#include "WBMCPBatchExecutor.h"
#include "WBMCPToolHandlers.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TSharedPtr<FJsonObject> FWBMCPBatchExecutor::Execute(const TSharedPtr<FJsonObject>& Args)
{
	// Parse commands array
	const TArray<TSharedPtr<FJsonValue>>* CommandsArr = nullptr;
	if (!Args.IsValid() || !Args->TryGetArrayField(TEXT("commands"), CommandsArr) || !CommandsArr)
	{
		TSharedPtr<FJsonObject> ErrResult = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> ContentArr;
		TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
		Content->SetStringField(TEXT("type"), TEXT("text"));
		Content->SetStringField(TEXT("text"), TEXT("Missing required argument: commands"));
		ContentArr.Add(MakeShared<FJsonValueObject>(Content));
		ErrResult->SetArrayField(TEXT("content"), ContentArr);
		ErrResult->SetBoolField(TEXT("isError"), true);
		return ErrResult;
	}

	bool bStopOnError = true;
	Args->TryGetBoolField(TEXT("stop_on_error"), bStopOnError);

	TMap<FString, TSharedPtr<FJsonObject>> NamedResults; // id -> {node_id, ...}
	TArray<TSharedPtr<FJsonValue>> Results;

	for (const TSharedPtr<FJsonValue>& CmdValue : *CommandsArr)
	{
		const TSharedPtr<FJsonObject>* CmdObjPtr = nullptr;
		if (!CmdValue.IsValid() || !CmdValue->TryGetObject(CmdObjPtr) || !CmdObjPtr)
		{
			continue;
		}
		TSharedPtr<FJsonObject> Cmd = *CmdObjPtr;

		FString ToolName;
		FString CmdId;
		Cmd->TryGetStringField(TEXT("name"), ToolName);
		Cmd->TryGetStringField(TEXT("id"), CmdId);

		// Guard against recursive batch
		if (ToolName == TEXT("execute_batch"))
		{
			TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
			EntryObj->SetStringField(TEXT("name"), ToolName);
			if (!CmdId.IsEmpty()) EntryObj->SetStringField(TEXT("id"), CmdId);
			EntryObj->SetBoolField(TEXT("success"), false);
			EntryObj->SetStringField(TEXT("result"), TEXT("Recursive execute_batch is not allowed"));
			Results.Add(MakeShared<FJsonValueObject>(EntryObj));
			if (bStopOnError) break;
			continue;
		}

		// Extract and clone arguments
		const TSharedPtr<FJsonObject>* ArgsObjPtr = nullptr;
		TSharedPtr<FJsonObject> CmdArgs = MakeShared<FJsonObject>();
		if (Cmd->TryGetObjectField(TEXT("arguments"), ArgsObjPtr) && ArgsObjPtr)
		{
			// Deep copy by serializing + deserializing
			FString ArgsJson;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArgsJson);
			FJsonSerializer::Serialize((*ArgsObjPtr).ToSharedRef(), Writer);
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ArgsJson);
			FJsonSerializer::Deserialize(Reader, CmdArgs);
		}

		// Substitute $id.field references
		SubstituteReferences(CmdArgs, NamedResults);

		// Execute
		TSharedPtr<FJsonObject> ToolResult = FWBMCPToolHandlers::Dispatch(ToolName, CmdArgs);

		// Extract result text and isError
		bool bIsError = false;
		FString ResultText;
		ToolResult->TryGetBoolField(TEXT("isError"), bIsError);
		const TArray<TSharedPtr<FJsonValue>>* ContentArrPtr = nullptr;
		if (ToolResult->TryGetArrayField(TEXT("content"), ContentArrPtr) && ContentArrPtr && ContentArrPtr->Num() > 0)
		{
			const TSharedPtr<FJsonObject>* ContentObjPtr = nullptr;
			if ((*ContentArrPtr)[0]->TryGetObject(ContentObjPtr) && ContentObjPtr)
			{
				(*ContentObjPtr)->TryGetStringField(TEXT("text"), ResultText);
			}
		}

		// Build result entry
		TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
		EntryObj->SetStringField(TEXT("name"), ToolName);
		if (!CmdId.IsEmpty()) EntryObj->SetStringField(TEXT("id"), CmdId);
		EntryObj->SetBoolField(TEXT("success"), !bIsError);
		EntryObj->SetStringField(TEXT("result"), ResultText);

		// Extract node_id from add_node result
		if (!bIsError && ToolName == TEXT("add_node"))
		{
			FString NodeId = ExtractNodeId(ResultText);
			if (!NodeId.IsEmpty())
			{
				EntryObj->SetStringField(TEXT("node_id"), NodeId);

				// Register in named results for later substitution
				if (!CmdId.IsEmpty())
				{
					TSharedPtr<FJsonObject> NamedEntry = MakeShared<FJsonObject>();
					NamedEntry->SetStringField(TEXT("node_id"), NodeId);
					NamedResults.Add(CmdId, NamedEntry);
				}
			}
		}

		Results.Add(MakeShared<FJsonValueObject>(EntryObj));

		if (bIsError && bStopOnError) break;
	}

	// Serialize results array
	FString ResultsJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultsJson);
	FJsonSerializer::Serialize(Results, Writer);

	TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
	Content->SetStringField(TEXT("type"), TEXT("text"));
	Content->SetStringField(TEXT("text"), ResultsJson);

	TArray<TSharedPtr<FJsonValue>> ContentArr;
	ContentArr.Add(MakeShared<FJsonValueObject>(Content));

	TSharedPtr<FJsonObject> FinalResult = MakeShared<FJsonObject>();
	FinalResult->SetArrayField(TEXT("content"), ContentArr);
	FinalResult->SetBoolField(TEXT("isError"), false);
	return FinalResult;
}

void FWBMCPBatchExecutor::SubstituteReferences(TSharedPtr<FJsonObject>& Arguments, const TMap<FString, TSharedPtr<FJsonObject>>& NamedResults)
{
	if (!Arguments.IsValid()) return;

	TArray<FString> Keys;
	Arguments->Values.GetKeys(Keys);

	for (const FString& Key : Keys)
	{
		TSharedPtr<FJsonValue> Val = Arguments->Values[Key];
		if (!Val.IsValid()) continue;

		FString StrVal;
		if (Val->TryGetString(StrVal) && StrVal.StartsWith(TEXT("$")))
		{
			// Parse "$id.field"
			FString WithoutDollar = StrVal.Mid(1);
			FString IdPart, FieldPart;
			if (WithoutDollar.Split(TEXT("."), &IdPart, &FieldPart))
			{
				const TSharedPtr<FJsonObject>* NamedPtr = NamedResults.Find(IdPart);
				if (NamedPtr && NamedPtr->IsValid())
				{
					FString FieldVal;
					if ((*NamedPtr)->TryGetStringField(*FieldPart, FieldVal))
					{
						Arguments->SetStringField(Key, FieldVal);
					}
				}
			}
		}
	}
}

FString FWBMCPBatchExecutor::ExtractNodeId(const FString& ResultText)
{
	// Format: "OK node_id=GUID"
	const FString Prefix = TEXT("node_id=");
	int32 Idx = ResultText.Find(Prefix);
	if (Idx == INDEX_NONE) return FString();
	return ResultText.Mid(Idx + Prefix.Len()).TrimEnd();
}

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Executes a batch of MCP tool commands sequentially.
 * Supports $id.node_id substitution from previous add_node results.
 */
class FWBMCPBatchExecutor
{
public:
	static TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Args);

private:
	static void SubstituteReferences(TSharedPtr<FJsonObject>& Arguments, const TMap<FString, TSharedPtr<FJsonObject>>& NamedResults);
	static FString ExtractNodeId(const FString& ResultText);
};

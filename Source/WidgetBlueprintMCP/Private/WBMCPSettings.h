#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WBMCPSettings.generated.h"

/**
 * Widget Blueprint MCP server settings.
 * Editor Preferences > Plugins > Widget Blueprint MCP
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig)
class UWBMCPSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWBMCPSettings();

	static const UWBMCPSettings* Get();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override  { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override   { return TEXT("WidgetBlueprintMCP"); }

	UPROPERTY(config, EditAnywhere, Category = "WidgetBlueprintMCP", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 Port = 8765;
};

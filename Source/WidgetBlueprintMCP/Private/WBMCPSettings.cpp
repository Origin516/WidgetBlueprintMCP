#include "WBMCPSettings.h"

UWBMCPSettings::UWBMCPSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName  = TEXT("WidgetBlueprintMCP");
}

const UWBMCPSettings* UWBMCPSettings::Get()
{
	return GetDefault<UWBMCPSettings>();
}

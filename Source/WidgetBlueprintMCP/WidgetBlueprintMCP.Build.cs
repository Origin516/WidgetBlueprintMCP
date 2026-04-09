using UnrealBuildTool;

public class WidgetBlueprintMCP : ModuleRules
{
	public WidgetBlueprintMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp17;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"UMG",
			"UMGEditor",
			"Kismet",
			"BlueprintGraph",
			"Json",
			"JsonUtilities",
			"HTTPServer",
			"AssetRegistry",
			"EditorSubsystem",
			"DeveloperSettings",
			"MovieScene",
			"Slate",
			"SlateCore",
		});
	}
}

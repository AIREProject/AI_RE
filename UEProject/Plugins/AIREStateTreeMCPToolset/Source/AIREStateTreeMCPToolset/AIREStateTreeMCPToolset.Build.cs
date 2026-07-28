using UnrealBuildTool;

public class AIREStateTreeMCPToolset : ModuleRules
{
	public AIREStateTreeMCPToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AI_RE",
				"StateTreeModule",
				"ToolsetRegistry"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
				"AnimationBlueprintLibrary",
				"InputCore",
				"MovieScene",
				"MovieSceneTracks",
				"PropertyBindingUtils",
				"StateTreeEditorModule",
				"UMG",
				"UMGEditor",
				"UnrealEd"
			}
		);
	}
}

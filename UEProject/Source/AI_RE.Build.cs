// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AI_RE : ModuleRules
{
	public AI_RE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"HTTP",
			"Json",
			"UMG",
			"Slate",
			"WebSockets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PrivateIncludePaths.AddRange(new string[] {
			"AI_RE/LMK/MAKO/Private"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("Advapi32.lib");
		}

		PublicIncludePaths.AddRange(new string[] {
			"AI_RE/OBI/Player",
			"AI_RE/OBI/Component/Public",
			"AI_RE/OBI/Abilities/Public",
			"AI_RE/OBI/Animation/Public",
			"AI_RE/OBI/UI/Public",
			"AI_RE/Global/Animation/Public",
			"AI_RE/Global/Components/Public",
			"AI_RE/Global/Inventory/Public",
			"AI_RE/Global/Sync/Public",
			"AI_RE/Global/Abilities/Set/Public",
			"AI_RE/Global/Actors/Public",
			"AI_RE/Global/Interfaces/Public",
			"AI_RE/Global/Combat/Public",
			"AI_RE/Global/AI/Public",
			"AI_RE/Global/Abilities/Attributes/Public",
			"AI_RE/Global/Characters/Public",
			"AI_RE/Global/Tags/Public",
			"AI_RE/Global/Data",
			"AI_RE/LMK/MAKO/Public",
			"AI_RE/LMK/MAKO/Components/Public"

		});

		// Uncomment if you are using Slate UI
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features 
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

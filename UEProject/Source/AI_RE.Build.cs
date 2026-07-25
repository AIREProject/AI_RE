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
			"AI_RE/OBI/ThirdPerson",
			"AI_RE/OBI/ThirdPerson/Variant_Combat",
			"AI_RE/OBI/ThirdPerson/Variant_Combat/AI",
			"AI_RE/OBI/ThirdPerson/Variant_Combat/Animation",
			"AI_RE/OBI/ThirdPerson/Variant_Combat/Gameplay",
			"AI_RE/OBI/ThirdPerson/Variant_Combat/Interfaces",
			"AI_RE/OBI/Component/Public",
			"AI_RE/OBI/UI/Public",
			"AI_RE/Global/Components/Public",
			"AI_RE/Global/Actors/Public",
			"AI_RE/Global/Interfaces/Public",
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

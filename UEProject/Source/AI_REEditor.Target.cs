// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AI_REEditorTarget : TargetRules
{
	public AI_REEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("AI_RE");
	}
}

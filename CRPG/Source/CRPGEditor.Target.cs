// Copyright. © 2024. Spxcebxr Games.

using UnrealBuildTool;
using System.Collections.Generic;

public class CRPGEditorTarget : TargetRules
{
	public CRPGEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		ExtraModuleNames.AddRange( new string[] { "CRPG" } );
	}
}

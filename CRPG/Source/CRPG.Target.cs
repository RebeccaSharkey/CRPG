// Copyright. © 2024. Spxcebxr Games.

using UnrealBuildTool;
using System.Collections.Generic;

public class CRPGTarget : TargetRules
{
	public CRPGTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		ExtraModuleNames.AddRange( new string[] { "CRPG" } );
	}
}

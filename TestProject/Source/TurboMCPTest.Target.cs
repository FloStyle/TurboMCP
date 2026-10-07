// Copyright FloStyle. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/// Game target for the test project.
///
/// The target is a game target because a plugin must be buildable from a normal
/// project, but TurboMCP itself is Editor-only, so it only contributes to the
/// editor target below.
public class TurboMCPTestTarget : TargetRules
{
	public TurboMCPTestTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("TurboMCPTest");
	}
}

// Copyright FloStyle. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/// Editor target for the test project.
///
/// This is the target to build when testing the plugin: TurboMCP and the engine
/// plugins it depends on are all Editor-only, so they exist only here.
public class TurboMCPTestEditorTarget : TargetRules
{
	public TurboMCPTestEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("TurboMCPTest");
	}
}

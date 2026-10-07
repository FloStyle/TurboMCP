// Copyright FloStyle. All Rights Reserved.

using UnrealBuildTool;

/// The test project's own module.
///
/// It is deliberately empty of logic. Its only job is to make the project a
/// C++ project, so the engine builds it and the TurboMCP plugin alongside it.
/// Every tool under test lives in the plugin, never here, so the plugin is
/// tested the way a user would load it.
public class TurboMCPTest : ModuleRules
{
	public TurboMCPTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			});
	}
}

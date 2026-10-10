// Copyright (c) 2026 Florian ESCHALIER (FloStyle)

using UnrealBuildTool;

/// The test project's own module.
///
/// It is deliberately empty of LOGIC. Its only job is to make the project a C++ project, so the engine
/// builds it and the TurboMCP plugin alongside it. Every tool under test lives in the plugin, never here,
/// so the plugin is tested the way a user would load it.
///
/// AND IT NOW CARRIES ONE ACTOR, WHICH IS THE SINGLE DELIBERATE EXCEPTION. `ATurboMCPTestLogActor` raises one
/// Warning and one Error when a level starts, so `R-PLAY-CHECK` finally has something real to count. It is
/// here rather than in the plugin ON PURPOSE: a `UE_LOG` added to the plugin's own production code so that
/// the plugin's own suite could pass would make the suite's strength depend on the code it is judging, which
/// would prove nothing. A TEST FIXTURE belongs in the test project.
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

		// `UnrealEd` IS ADDED FOR ONE REASON AND IT IS WORTH STATING. The plugin's commandlet that builds the
		// test map needs `UWorld::CreateWorld`, `UPackage::SavePackage` and the asset registry, and it reads
		// `ATurboMCPTestLogActor` to spawn it - so the dependency runs FROM the plugin TO this module, and
		// this entry is what makes that edge legal in the build graph rather than an unresolved include.
		//
		// It is a PRIVATE dependency because no public header of this module exposes an `UnrealEd` type: the
		// actor's header includes only `GameFramework/Actor.h`.
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
			});
	}
}

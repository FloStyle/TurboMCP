// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

using UnrealBuildTool;

// Disposable gameplay/UI and compiler fixtures live here, outside production tools.
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

        // Editor/compiler fixtures and runtime CommonUI input scenarios.
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"Kismet",
                "UMG", "Slate", "SlateCore", "InputCore", "EnhancedInput", "CommonUI", "CommonInput", "GameplayTags",
			});
	}
}

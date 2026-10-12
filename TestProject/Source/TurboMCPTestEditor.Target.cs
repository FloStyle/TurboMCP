// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

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

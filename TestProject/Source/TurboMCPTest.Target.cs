// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

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

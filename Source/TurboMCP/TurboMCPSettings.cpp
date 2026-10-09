// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPSettings.h"

UTurboMCPSettings::UTurboMCPSettings()
{
	// The defaults are declared on the members, so this only exists to give the class a body the
	// engine can construct from the config.
}

UTurboMCPSettings* UTurboMCPSettings::GetMutable()
{
	// `GetMutableDefault` is what `UDeveloperSettings` expects: the engine creates one instance
	// from the config and keeps it, so every reader sees the same state and the panel's checkbox
	// cannot drift from what `SaveConfig` writes.
	return GetMutableDefault<UTurboMCPSettings>();
}

const UTurboMCPSettings* UTurboMCPSettings::Get()
{
	return GetDefault<UTurboMCPSettings>();
}

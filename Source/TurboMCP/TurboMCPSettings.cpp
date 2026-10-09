// Copyright FloStyle. All Rights Reserved.

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

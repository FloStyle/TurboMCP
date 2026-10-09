// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPModule.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#include "ToolsetRegistry/UToolsetRegistry.h"

#include "Logging.h"
#include "TurboMCPAnalyticsProvider.h"
#include "Panel/STurboMCPPanel.h"
#include "Tools/TurboMCPToolsets.h"
#define LOCTEXT_NAMESPACE "FTurboMCPModule"

void FTurboMCPModule::StartupModule()
{
	// The registry is not ready yet, so wait for every loading phase to finish.
	// Registering in StartupModule would leave the class unregistered with no
	// error, which is the worst kind of failure because the editor looks fine.
	LoadingPhasesCompleteHandle = FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(
		this, &FTurboMCPModule::RegisterToolsets);

	// The panel only needs Slate, so it can be offered straight away. A developer
	// opening it before the toolsets exist sees the honest "MISSING" lines.
	RegisterTurboMCPPanel();

	UE_LOG(LogTurboMCP, Log, TEXT("TurboMCP module started"));
}

void FTurboMCPModule::ShutdownModule()
{
	if (LoadingPhasesCompleteHandle.IsValid())
	{
		FCoreDelegates::OnAllModuleLoadingPhasesComplete.Remove(LoadingPhasesCompleteHandle);
		LoadingPhasesCompleteHandle.Reset();
	}

	UnregisterToolsets();
	RemoveAnalyticsProvider();
	UnregisterTurboMCPPanel();

	UE_LOG(LogTurboMCP, Log, TEXT("TurboMCP module shut down"));
}

void FTurboMCPModule::RegisterToolsets()
{
	// A toolset can only be registered once the registry exists. IsAvailable is
	// the engine's own check for that, and it returns false when there is no
	// editor (for example in a commandlet), which is not an error here.
	if (!UToolsetRegistry::IsAvailable())
	{
		UE_LOG(LogTurboMCP, Warning,
			TEXT("TurboMCP toolsets not registered: the toolset registry is not available. ")
			TEXT("Check that the ToolsetRegistry engine plugin is enabled."));
		return;
	}

	RegisterTurboMCPToolsets();

	InstallAnalyticsProvider();

	UE_LOG(LogTurboMCP, Log, TEXT("TurboMCP toolsets registered"));
}

void FTurboMCPModule::UnregisterToolsets()
{
	if (!UToolsetRegistry::IsAvailable())
	{
		return;
	}

	UnregisterTurboMCPToolsets();
}

void FTurboMCPModule::InstallAnalyticsProvider()
{
	// The phase 0 stub said the attribute names had to be read from source first, "so that a
	// wrong guess cannot corrupt engine telemetry". They were, and they are:
	//
	//     SessionId, ToolsetNameHash, ToolNameHash, Duration, ResultStatus
	//
	// on an event named `<namespace>.ToolCall`, from `ModelContextProtocolAnalytics.cpp`. The
	// names are BLAKE3 hashes and there are no arguments, which is why the plugin's own tools
	// write their own full-detail lines as well.
	//
	// `Install` KEEPS whatever provider was already there and forwards every event to it, so
	// installing this cannot lose an event the engine's own telemetry wanted. That was the risk
	// the phase 0 note was guarding against, and it is handled in the provider rather than by
	// not installing one.
	FTurboMCPAnalyticsProvider::Install();

	bInstalledAnalyticsProvider = FTurboMCPAnalyticsProvider::IsInstalled();

	UE_LOG(LogTurboMCP, Log,
		TEXT("TurboMCP analytics provider installed: %s"),
		bInstalledAnalyticsProvider ? TEXT("yes") : TEXT("no"));
}

void FTurboMCPModule::RemoveAnalyticsProvider()
{
	// Unconditional rather than guarded by `bInstalledAnalyticsProvider`: the provider restores
	// the one it wrapped only when it is still the one installed, so calling this when it was
	// never installed, or when something else has since replaced it, is safe and does nothing.
	FTurboMCPAnalyticsProvider::Uninstall();

	if (!bInstalledAnalyticsProvider)
	{
		return;
	}

	bInstalledAnalyticsProvider = false;
	PreviousAnalyticsProvider.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTurboMCPModule, TurboMCP)

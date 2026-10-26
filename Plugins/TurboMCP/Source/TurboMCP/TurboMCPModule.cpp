// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPModule.h"
#include "TurboMCPSettings.h"
#include "TurboMCPPlayInput.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/ToolsetRegistry.h"

#include "Logging.h"
#include "TurboMCPAnalyticsProvider.h"
#include "TurboMCPFocusGuard.h"
#include "Panel/STurboMCPPanel.h"
#include "Tools/TurboMCPToolsets.h"
#define LOCTEXT_NAMESPACE "FTurboMCPModule"

void FTurboMCPModule::StartupModule()
{
    const UTurboMCPSettings* Settings = UTurboMCPSettings::Get();
    bEnabledForSession = Settings && Settings->bEnableTurboMCP;
    if (!bEnabledForSession)
    {
        UE_LOG(LogTurboMCP, Log, TEXT("TurboMCP disabled by this user's Editor Preferences. No tools, panel, recording hooks or native-tool filters installed. Enable TurboMCP in Editor Preferences > Plugins > TurboMCP and restart the editor to activate it. Unreal MCP is unchanged."));
        return;
    }

	// The registry is not ready yet, so wait for every loading phase to finish.
	// Registering in StartupModule would leave the class unregistered with no
	// error, which is the worst kind of failure because the editor looks fine.
	LoadingPhasesCompleteHandle = FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(
		this, &FTurboMCPModule::RegisterToolsets);

	// The panel only needs Slate, so it can be offered straight away. A developer
	// opening it before the toolsets exist sees the honest "MISSING" lines.
	RegisterTurboMCPPanel();

	// AND THE FOCUS GUARD IS INSTALLED AT THE SAME MOMENT, FOR THE SAME REASON: it needs Slate and nothing else.
	// IT IS THE FIX FOR THE OWNER'S REPORT THAT A TEST RUN KEEPS TAKING THEIR FOCUS, and it lives here because
	// FOUR ATTEMPTS FROM OUTSIDE THE PROCESS WERE MEASURED AND EACH KILLED THE EDITOR - a process that manages its
	// own message loop treats an externally hidden or minimised main window as a shutdown condition. From inside,
	// the editor activates itself normally and this hands the foreground back. It does nothing unless the run is
	// unattended, so a human launching the editor to work in it is unaffected.
	FTurboMCPFocusGuard::Install();

	UE_LOG(LogTurboMCP, Log, TEXT("TurboMCP module started"));
}

void FTurboMCPModule::ShutdownModule()
{
    if (!bEnabledForSession) return;
    TurboMCPPlayInput::Shutdown();
	if (LoadingPhasesCompleteHandle.IsValid())
	{
		FCoreDelegates::OnAllModuleLoadingPhasesComplete.Remove(LoadingPhasesCompleteHandle);
		LoadingPhasesCompleteHandle.Reset();
	}

	UnregisterToolsets();
	RemoveAnalyticsProvider();
	UnregisterTurboMCPPanel();

	// Removed after the panel, because the guard needs Slate to unhook itself.
	FTurboMCPFocusGuard::Remove();

	UE_LOG(LogTurboMCP, Log, TEXT("TurboMCP module shut down"));
}

void FTurboMCPModule::RegisterToolsets()
{
    if (!bEnabledForSession) return;
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
    // Stock-engine containment: these native paths bypass the plugin's write
    // and rollback safeguards. Exact suffixes avoid blocking unrelated tools.
    auto RegistryResult = UToolsetRegistrySubsystem::Get();
    if (!RegistryResult.HasError())
    {
        auto& Registry = RegistryResult.GetValue()->ToolsetRegistry;
        const TArray<FString> Patterns = {
            TEXT("/(^|\\.)DataTableTools\\.(add_rows|set_rows|rename_rows)$/"),
            TEXT("/(^|\\.)BlueprintTools\\.(set_pin_value|compile_blueprint)$/"),
            TEXT("/(^|\\.)ProgrammaticToolset\\.execute_tool_script$/")
        };
        for (const FString& Pattern : Patterns)
        {
            if (!Registry.GetBlockedNames().Contains(Pattern))
            {
                Registry.AddBlockedName(Pattern);
                InstalledSafetyFilters.Add(Pattern);
            }
        }
        UE_LOG(LogTurboMCP, Warning, TEXT("Native DataTable JSON writes, pin setters, compile and script batches are filtered. Use TurboMCP DataTableRows, PinDefault, Compile/Save and bounded BatchEdit. Direct Python/editor code is not intercepted."));
    }


	InstallAnalyticsProvider();

	UE_LOG(LogTurboMCP, Log, TEXT("TurboMCP toolsets registered"));
}

void FTurboMCPModule::UnregisterToolsets()
{
	if (!UToolsetRegistry::IsAvailable())
	{
		return;
	}

    auto RegistryResult = UToolsetRegistrySubsystem::Get();
    if (!RegistryResult.HasError())
    {
        auto& Registry = RegistryResult.GetValue()->ToolsetRegistry;
        for (const FString& Pattern : InstalledSafetyFilters)
        {
            Registry.RemoveBlockedName(Pattern);
        }
    }
    InstalledSafetyFilters.Reset();
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
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTurboMCPModule, TurboMCP)

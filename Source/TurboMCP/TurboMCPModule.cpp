// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPModule.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#include "ToolsetRegistry/UToolsetRegistry.h"

#include "Logging.h"
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
	// Deliberately empty in phase 0. The call log arrives in phase 5
	// (`08_ROADMAP.md`) and needs the engine's analytics attribute names read
	// from source first, so that a wrong guess cannot corrupt engine telemetry.
}

void FTurboMCPModule::RemoveAnalyticsProvider()
{
	if (!bInstalledAnalyticsProvider)
	{
		return;
	}

	bInstalledAnalyticsProvider = false;
	PreviousAnalyticsProvider.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTurboMCPModule, TurboMCP)

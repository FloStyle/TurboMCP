// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

class IAnalyticsProviderET;

/**
 * The single editor module of the TurboMCP plugin.
 *
 * Its only jobs are: register this plugin's toolsets with the engine's
 * toolset registry, and install the call log's analytics hook. Every tool
 * lives in a toolset class under Tools/, so this file stays small
 * (`00_START_HERE.md` section 4: one file, one job).
 */
class FTurboMCPModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	/**
	 * Registers every TurboMCP toolset with `UToolsetRegistry`.
	 *
	 * Runs from `FCoreDelegates::OnAllModuleLoadingPhasesComplete` rather than
	 * from StartupModule. `07_ENGINE_NOTES.md` section 2.1 says the registry may
	 * not exist during StartupModule, and a toolset registered too early is
	 * silently never served to an agent.
	 */
	void RegisterToolsets();

	/** Undoes RegisterToolsets so a hot reload leaves no stale registration. */
	void UnregisterToolsets();

private:
	/**
	 * Installs the call log's analytics provider, keeping the provider that was
	 * already installed so engine tool calls can still be forwarded.
	 *
	 * `07_ENGINE_NOTES.md` section 2.4: reading the engine's own tool calls
	 * needs a provider of our own that records the call and forwards it on.
	 */
	void InstallAnalyticsProvider();

	/** Removes our provider and puts the previous one back. */
	void RemoveAnalyticsProvider();

	/** Handle for the module-loading-complete delegate, so it can be removed. */
	FDelegateHandle LoadingPhasesCompleteHandle;

	/** The analytics provider installed before this plugin loaded, if any. */
	TSharedPtr<IAnalyticsProviderET> PreviousAnalyticsProvider;

	/** True once our analytics provider is the installed one. */
	bool bInstalledAnalyticsProvider = false;
};

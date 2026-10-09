// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TurboMCPSettings.generated.h"

/**
 * The plugin's persisted choices.
 *
 * `03_TOOLS.md` requires the call log to be "turned on by a checkbox in the panel. It starts
 * unticked, and the choice persists in editor settings." `UDeveloperSettings` is what the editor
 * has for exactly that: it persists to a config file, it is readable and writable without an
 * editor window, and it appears under Project Settings so a user can find the switch even when the
 * TurboMCP panel is closed.
 *
 * THE DEFAULT IS OFF, deliberately: a log that switched itself on would make every user pay for it,
 * and the spec says it starts unticked. The panel's checkbox and this object are the SAME state, so
 * they cannot disagree.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "TurboMCP"))
class TURBOMCP_API UTurboMCPSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTurboMCPSettings();

	/** True when every tool call is written to the call log. Starts off, per the spec. */
	UPROPERTY(EditAnywhere, config, Category = "Call log",
		meta = (DisplayName = "Record every tool call"))
	bool bEnableCallLog = false;

	/** The settings object, or null when it could not be created. */
	static UTurboMCPSettings* GetMutable();

	/** The settings, or null. */
	static const UTurboMCPSettings* Get();

	/** The config section name, so `SaveConfig` writes where the loader reads. */
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

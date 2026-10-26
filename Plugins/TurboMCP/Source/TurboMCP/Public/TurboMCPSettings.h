// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TurboMCPSettings.generated.h"

/** Personal preferences for this user and project. Never written to shared default config.
 * Enablement is sampled once at module startup; restart the editor to apply a change.
 * Call recording remains independently configurable from the panel or these preferences.
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "TurboMCP"))
class TURBOMCP_API UTurboMCPSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTurboMCPSettings();

    /** Enable TurboMCP tools, panel and hooks for this user. Restart the editor after changing this preference. Unreal MCP and other toolsets are unaffected. */
    UPROPERTY(EditAnywhere, config, Category = "Activation", meta = (DisplayName = "Enable TurboMCP", ConfigRestartRequired = true))
    bool bEnableTurboMCP = true;


	/** Record tool calls by default for field diagnostics. Saved user preferences override this default. */
	UPROPERTY(EditAnywhere, config, Category = "Call log",
		meta = (DisplayName = "Record every tool call"))
	bool bEnableCallLog = true;

	/** The settings object, or null when it could not be created. */
	static UTurboMCPSettings* GetMutable();

	/** The settings, or null. */
	static const UTurboMCPSettings* Get();

	/** The config section name, so `SaveConfig` writes where the loader reads. */
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

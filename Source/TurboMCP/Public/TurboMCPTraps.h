// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"

/**
 * Known traps and real parameter names for tools, loaded from a data file.
 *
 * `D-07` in `05_DECISIONS.md` requires this to be data rather than code, because
 * traps change with every engine update and every agent report, and must be
 * correctable without a rebuild. So the entries live in a JSON file next to the
 * plugin, and this class only reads them.
 *
 * Two files are consulted, in this order, and the later one wins:
 *
 *   1. `<Plugin>/Content/Traps/EngineToolTraps.json` - shipped with the plugin
 *   2. `<Project>/Config/TurboMCP/Traps.json`        - the project's own notes
 *
 * The second exists so a project can record a trap this plugin does not know
 * yet, without editing the plugin or recompiling.
 *
 * One shared instance is used, because loading and parsing the file on every
 * tool call would be wasteful and because a tool description that changes
 * between calls would be worse than one that is merely stale.
 */
class FTurboMCPTraps
{
public:
	/** What is known about one tool. */
	struct FToolInfo
	{
		/** Real parameter names, keyed by the name an agent might guess. */
		TMap<FString, FString> Parameters;

		/** One plain sentence per trap. No log text, no code. */
		TArray<FString> Traps;
	};

	/** The shared instance. Loads on first use. */
	static const FTurboMCPTraps& Get();

	/**
	 * Notes for one tool, or null when nothing is recorded for it.
	 *
	 * A null result is normal and not an error: most engine tools have no known
	 * traps. The caller says "none recorded" rather than an empty answer (`P1`).
	 */
	const FToolInfo* Find(const FString& InToolName) const;

	/** Every tool name that has notes. Used by the help tool to show coverage. */
	const TMap<FString, FToolInfo>& GetAll() const { return Tools; }

	/**
	 * Why the notes are empty, when they are.
	 *
	 * Empty when a file was read. Otherwise one line naming the paths tried, so
	 * a missing data file is visible instead of looking like "no traps exist"
	 * (`P1`).
	 */
	const FString& GetLoadError() const { return LoadError; }

	/** Where the notes were read from, for the help tool to print. */
	const FString& GetLoadedFrom() const { return LoadedFrom; }

private:
	FTurboMCPTraps();

	/** Reads one file into Tools. Returns false and leaves Tools alone on failure. */
	bool LoadFile(const FString& InPath, FString& OutError);

	TMap<FString, FToolInfo> Tools;
	FString LoadError;
	FString LoadedFrom;
};

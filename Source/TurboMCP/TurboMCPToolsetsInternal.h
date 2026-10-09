// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SubclassOf.h"
#include "UObject/NameTypes.h"

class UClass;
class UToolsetDefinition;

/**
 * Helpers shared by the files that build and describe the TurboMCP tool surface.
 *
 * Why this is not in `Public/Tools/TurboMCPToolsets.h`: that header is processed
 * by UnrealHeaderTool, and a change to it makes UHT regenerate the reflection
 * and makes UBT re-plan the whole target. These are plain C++ helpers with no
 * reflection, so they live here where changing them costs one file recompile.
 *
 * Declarations only. The definitions are in `TurboMCPToolsets.cpp`, so the four
 * toolset classes and the rule for naming them stay in one place.
 */
namespace TurboMCPToolsets
{
	/** Every toolset class this plugin owns, in kind order: Read, Look, Preview, Edit. */
	const TArray<TSubclassOf<UToolsetDefinition>>& GetToolsetClasses();

	/**
	 * The `<Module>.<ClassName>` name the engine registers a class under.
	 *
	 * The leading `U` is stripped, verified against the live server: `list_toolsets`
	 * returns `TurboMCP.TurboMCPReadToolset`, not `...UTurboMCPReadToolset`.
	 */
	FString GetToolsetName(const UClass* InClass);

	/** One line saying whether the MCP server is running, and where. */
	FString DescribeServerState();

	/** One line per toolset, saying whether the registry has it. */
	FString DescribeRegisteredToolsets();
}

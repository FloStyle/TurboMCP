// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Logging/LogMacros.h"

/**
 * Log category for the plugin's messages in the Output Log.
	 *
	 * *** ONE MODULE KEEPS A CATEGORY OF ITS OWN, AND THIS COMMENT SAID "every message" WITHOUT NAMING IT. ***
	 * TurboMCPTrace.cpp:24 defines LogTurboMCPTrace and its three messages (:111, :165, :178) use it.
	 * *** MEASURED: LogTurboMCP IS USED 102 TIMES AND LogTurboMCPTrace 3 - SO EVERY MESSAGE WAS FALSE BY THREE,
	 * AND THE SECOND CATEGORY IS DELIBERATE RATHER THAN AN OVERSIGHT: the trace module exists so a live run can be
	 * diagnosed, and a category of its own is what lets a developer FILTER THAT ALONE. THE PROMISE IS AMENDED RATHER
	 * THAN THE CATEGORY RENAMED, BECAUSE RENAMING THREE CALL SITES WOULD DESTROY A DISTINCTION SOMEONE MADE ON PURPOSE. ***
 *
 * Why a category of its own: `P6` in `02_PRINCIPLES.md` forbids engine log text
 * in tool results. When a tool has to swallow engine log spam, it counts the
 * warnings per category instead. A named category is what makes that count
 * possible, and it lets a developer filter the Output Log to TurboMCP alone.
 */
TURBOMCP_API DECLARE_LOG_CATEGORY_EXTERN(LogTurboMCP, Log, All);

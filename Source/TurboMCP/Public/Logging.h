// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Logging/LogMacros.h"

/**
 * Log category for every message TurboMCP writes to the Output Log.
 *
 * Why a category of its own: `P6` in `02_PRINCIPLES.md` forbids engine log text
 * in tool results. When a tool has to swallow engine log spam, it counts the
 * warnings per category instead. A named category is what makes that count
 * possible, and it lets a developer filter the Output Log to TurboMCP alone.
 */
TURBOMCP_API DECLARE_LOG_CATEGORY_EXTERN(LogTurboMCP, Log, All);

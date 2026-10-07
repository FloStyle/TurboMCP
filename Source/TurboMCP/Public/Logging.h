// Copyright FloStyle. All Rights Reserved.

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

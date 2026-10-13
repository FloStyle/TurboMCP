// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

/**
 * Gives every one of this plugin's tools a `start` record before it runs and an `end` record after it,
 * ON WHICHEVER PATH THE CALL ARRIVED - including an agent's.
 *
 * WHY THIS EXISTS, AND WHAT IT REPLACES
 * -------------------------------------
 * `T02`'s `done when` 1 asks for a start line with the tool name and the arguments before a tool runs.
 * The plugin's own tools had no such line: nothing in the plugin called `FScopedCall`, so every record
 * in the log was `source=engine` — the engine's analytics event, which carries a duration AFTER the
 * call and nothing before it. `TurboMCPCallLog.h:164-177` records the consequence in the timeline: an
 * agent's call that crashes leaves NO LINE AT ALL.
 *
 * *** THE MEASURED PRICE OF DOING IT INSIDE THE TOOLS WAS 68 INDIVIDUALLY-ANCHORED EDITS: 68 tool
 * bodies, whose first statements come in 59 distinct shapes. THAT MEASUREMENT IS CORRECT AND IT IS
 * MEASURING THE WRONG PLACE. Every agent tool call enters through ONE engine call -
 * `UE::ToolsetRegistry::FToolsetRegistry::ExecuteTool(Descriptor, Json)`, reached from the MCP
 * server's own adapter (`ModelContextProtocolToolsetRegistryAdapter.cpp:66` and `:258`) - and the
 * registry holds one handler per toolset. The plugin owns FOUR toolsets, so the start record costs
 * FOUR substitutions rather than sixty-eight insertions. ***
 *
 * HOW THE SUBSTITUTION IS DONE, AND WHY IT NEEDS NO ENGINE PATCH
 * -------------------------------------------------------------
 * `FToolset::ExecuteTool` is public and NOT virtual (`Toolset/Toolset.h:28`), and it calls the virtual
 * `ExecuteToolInternal` (`:72`), so a handler can be WRAPPED rather than replaced: the wrapper writes
 * the start record, forwards to the handler the engine built, and writes the end record from the
 * future's own continuation. `FToolsetRegistry::UnregisterToolset` + `RegisterToolset`
 * (`ToolsetRegistry.cpp:122-160`) is the public pair that swaps it, and the registry re-broadcasts
 * `OnToolsetRegistered`, so the MCP server re-reads the schemas by itself.
 *
 * WHAT IS RECORDED, AND IN WHICH SPELLING
 * ---------------------------------------
 * The toolset is written as the BARE name (the last dotted segment of the engine's registration name)
 * and the tool as the bare tool name. That spelling is the one `TurboMCPCallCost.cpp:111-113` already
 * documents for `FScopedCall`, and it is what keeps the doubled path it records as a defect out of the
 * log: the engine's own event writes `TurboMCP.TurboMCPReadToolset` and a fully-qualified tool name,
 * so the two writers remain tellable apart by their `source` field, and neither invents a name.
 *
 * THE CALL IS STILL REPORTED TWICE ON THE ENGINE'S SIDE, AND THAT IS NOT HIDDEN
 * ---------------------------------------------------------------------------
 * The engine's analytics event for a plugin tool still arrives and is still written, because it is the
 * engine's own measurement of the same call and `TurboMCPCallLog.h:25-43` keeps both writers on
 * purpose. An agent run therefore shows the plugin's start/end pair AND the engine's thinner end
 * record. The `source` field is what distinguishes them, and `ReadTurboMCPCallLog` prints it.
 *
 * Safe to call more than once: a toolset already wrapped is left alone. Called from
 * `RegisterTurboMCPToolsets()`, so a toolset is registered and recorded by one code path.
 */
void InstallTurboMCPCallRecording();

/**
 * Drops this plugin's references to the wrappers it installed.
 *
 * NOT A SECOND UNREGISTRATION: `UnregisterTurboMCPToolsets` has already taken the toolsets out of the
 * engine's registry by then, and this only releases the shared pointers this file holds. It exists
 * because a static array keeping engine handlers alive across a module unload outlives the thing it
 * is holding.
 */
void RemoveTurboMCPCallRecording();

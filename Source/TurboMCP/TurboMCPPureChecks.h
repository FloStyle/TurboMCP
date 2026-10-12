// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

/**
 * Reaches `FTurboMCPCallCost`'s TWO PURE FUNCTIONS from a suite, with LITERAL inputs.
 *
 * *** WHY THIS EXISTS, AND IT IS A MEASURED GAP RATHER THAN A PREFERENCE. ***
 *
 * `NormaliseToolset` and `AssetFromArguments` are the two functions the per-tool and per-asset totals
 * are built from, and ONE OF THEM IS UNREACHABLE FROM ANY SUITE THAT READS THE LOG. Every entry in
 * `TestProject/Saved/Logs/TurboMCPCalls.jsonl` is `source=engine`, and an engine event carries NO
 * `arguments` field at all - measured on 2026-10-11 over 252 lines, not one of which had arguments. So
 * no suite can hand `AssetFromArguments` a non-empty input, and the only thing watching it is the
 * sentence the tool prints when the per-asset section cannot fill.
 *
 * THE NORMALISER'S HALF IS REACHABLE, AND THAT IS MEASURED TOO BECAUSE IT CORRECTS AN EARLIER RECORD.
 * A break test reporting that the suite still passed with the normaliser reduced to `return Trimmed;`
 * IS NOT REPRODUCIBLE: against a build that demonstrably compiled that break
 * (`[1/5] Compile [x64] TurboMCPCallCost.cpp`, DLL relinked, `Result: Succeeded`), `test-call-cost`
 * reports `RESULT: FAIL (1) -> no tool row contains the engine's qualification path`, naming rows such
 * as `TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus`. The log DOES carry the qualified spelling -
 * 116 of its 252 lines do - because the engine's own writer resolves names fully qualified.
 *
 * THE LIKELY CAUSE OF THAT EARLIER PASS IS THE STALE-BINARY TRAP THIS PROJECT HAS RECORDED SEVEN
 * TIMES, AND THE CHRONOLOGY IS WHY RATHER THAN A PREFERENCE: the registration of the qualified form
 * landed on 2026-10-09 (`08dda6e`), and the caller's spelling constant has only ever held the
 * qualified `TurboMCP.TurboMCPReadToolset` (`2b643ad`, same day) - so the log's shape two days later,
 * at the break test of 2026-10-11, was the shape it has now. I cannot observe that run and do not
 * claim to. What I CAN state is that the measurement does not survive a build that demonstrably
 * compiled the break.
 *
 * SO WHAT THIS TOOL ADDS IS NOT "A CHECK THAT CANNOT OTHERWISE FAIL"; IT IS A CHECK THAT CANNOT GO
 * VACUOUS. The log-dependent check bites only while the log happens to carry a QUALIFIED name, which
 * is a fact about the engine's hash resolver rather than about this code: an engine that resolved the
 * bare name, or a log holding only `(top-level)` rows, would leave that check green with the
 * normaliser broken again. A LITERAL INPUT CANNOT DRIFT - it needs no log, it is the same assertion
 * in every session, and it is the only shape that can ask for the doubly-qualified path a join applied
 * twice produces.
 *
 * THE COMMANDLET THAT WOULD HAVE DONE THIS WITHOUT A SUITE CANNOT RUN, and that is the ENGINE's
 * constraint rather than this project's: `TurboMCP.Build.cs:61` depends on `Kismet`, so the process
 * reaches `FBlueprintEditorModule::StartupModule()` and dies on its FIRST statement, `check(GEditor);`
 * (`Editor/Kismet/Private/BlueprintEditorModule.cpp:214`) - before any commandlet's `Main` is reached,
 * and with none of that commandlet's own output in the log. `TurboMCPCallCostCommandlet` is kept as
 * the evidence of that, and it is not modified.
 *
 * SO THE ASSERTIONS HAVE TO RUN WHERE `GEditor` EXISTS, WHICH IS HOW EVERY OTHER CHECK IN THIS
 * PROJECT RUNS: a suite, over MCP, calling a tool. This class is that tool's implementation, and it
 * is deliberately the smallest one that can work: it takes the literal inputs, calls the two
 * functions, and PRINTS WHAT THEY RETURNED. It decides nothing, it judges nothing, and it grades
 * nothing - a tool that both ran a check and declared it passed would be a check with no second
 * witness.
 *
 * IT COMPUTES STRINGS AND WRITES NOTHING. It opens no log, loads no asset, touches no editor tab,
 * and toggles no shared setting, so unlike the fixture-driven suites this one cannot race another
 * suite and has nothing to restore.
 *
 * WHY IT IS SAFE FOR THIS TO LIVE IN THE READ TOOLSET. The two functions it calls are `static`
 * members of `FTurboMCPCallCost` (`Public/TurboMCPCallCost.h:107` and `:117`) and are NOT inside the
 * anonymous namespace of `TurboMCPCallCost.cpp:13-82`, so they are already reachable from another
 * translation unit. The helper that IS anonymous - `ValueOfKey` - stays private, and this file calls
 * the public function rather than reaching past it.
 */
class FTurboMCPPureChecks
{
public:
	/**
	 * Calls both pure functions on the caller's literal inputs and reports what they returned.
	 *
	 * `InToolsets` is one or more toolset SPELLINGS separated by commas or pipes. Two are expected,
	 * because the log has TWO WRITERS and they spell one toolset two ways: the analytics provider
	 * resolves a hash against a registry of fully qualified names
	 * (`TurboMCPAnalyticsProvider.cpp:55`), so an engine entry records
	 * `TurboMCP.TurboMCPReadToolset`, while this plugin's own `FScopedCall` records the bare
	 * `TurboMCPReadToolset`. Collapsing them is what keeps one toolset from being reported as two
	 * rows with a denominator each.
	 *
	 * Each spelling is echoed with the key `NormaliseToolset` gave it, and the number of DISTINCT
	 * keys is printed as a NUMBER, so a caller reads "one key" off a count rather than out of a
	 * sentence. A spelling list that is empty says so rather than reporting a count of zero as if
	 * zero keys had been computed (`P1`).
	 *
	 * `InArguments` is an `arguments` blob exactly as the log records one - free text, not a parsed
	 * schema. The value `AssetFromArguments` returned is printed with its LENGTH as a NUMBER, so
	 * "names no asset" is a measured zero that a caller parses, never a sentence a caller matches
	 * beside a number. An engine event carries NO ARGUMENTS AT ALL, which is why the empty answer
	 * must be distinguishable from a real one rather than read as a zero cost.
	 *
	 * Line breaks and tabs in the echoed arguments are written as spaces, because the report is a
	 * line-oriented shape a caller parses. The COMPUTATION uses the string exactly as given; only
	 * the echo is folded, and the fold is stated here so a reader of the output is not misled.
	 */
	static FString Report(const FString& InToolsets, const FString& InArguments);
};

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPPureChecks.h"

/**
 * `R-PURE-CHECKS`: call the two PURE functions behind `R-CALL-COST` with LITERAL inputs.
 *
 * *** THIS TOOL EXISTS FOR ONE MEASURED REASON, AND THE MEASUREMENT IS NARROWER THAN THE BRIEF THAT
 * ASKED FOR IT - STATED HERE RATHER THAN QUIETLY WIDENED. *** `R-CALL-COST` totals calls by
 * `FTurboMCPCallCost::NormaliseToolset` and attributes them by `FTurboMCPCallCost::AssetFromArguments`.
 *
 * WHAT IS GENUINELY UNREACHABLE: `AssetFromArguments`. Every entry in the real log is `source=engine`,
 * and an engine event carries NO `arguments` field at all - measured on 2026-10-11 over 252 lines, not
 * one of which had arguments. No suite can hand that function a non-empty input, so nothing watches it
 * except the sentence printed when the per-asset section cannot fill.
 *
 * WHAT IS REACHABLE, MEASURED, AND REPORTED BECAUSE IT CORRECTS THE RECORD: the normaliser's collapse.
 * On a build that demonstrably compiled `return Trimmed;` in its body (`[1/5] Compile [x64]
 * TurboMCPCallCost.cpp`, DLL relinked), `test-call-cost` reports `RESULT: FAIL (1) -> no tool row
 * contains the engine's qualification path`. A break test that reported it still passing is NOT
 * reproducible, and the most likely cause is the stale-binary trap this project has recorded seven
 * times: the registration of the qualified form landed on 2026-10-09 (`08dda6e`) and the caller has
 * passed the qualified spelling since the same day, so the log's shape at that break test was the
 * shape it has now. I cannot observe that run; I can state only that the measurement does not survive
 * a build whose compilation of the broken file is in its log.
 *
 * WHAT THIS TOOL THEREFORE ADDS IS NOT "A CHECK THAT CANNOT OTHERWISE FAIL" BUT "A CHECK THAT CANNOT
 * GO VACUOUS": the log-dependent check bites only while the log carries a QUALIFIED name, which is a
 * fact about the engine's resolver rather than about this code, and a literal input cannot drift.
 *
 * THE COMMANDLET THAT WOULD HAVE DONE THIS WITHOUT A SUITE CANNOT RUN, and that is the ENGINE's
 * constraint rather than this project's: `TurboMCP.Build.cs:61` depends on `Kismet`, so the process
 * reaches `FBlueprintEditorModule::StartupModule()` and dies on its first statement, `check(GEditor);`
 * (`Editor/Kismet/Private/BlueprintEditorModule.cpp:214`), before any commandlet's `Main` runs.
 * `TurboMCPCallCostCommandlet` is kept as the evidence of that.
 *
 * SO THE ASSERTIONS RUN WHERE `GEditor` EXISTS, WHICH IS WHERE EVERY OTHER CHECK IN THIS PROJECT
 * RUNS: a suite, over MCP, calling a tool. This is that tool's entry point and it is deliberately
 * DUMB: `FTurboMCPPureChecks::Report` calls the two functions and prints what they returned. Nothing
 * here judges, grades or asserts, because a tool that ran a check and declared it passed would be a
 * check with no second witness - the suite is the witness, and it compares the printed values
 * against literals of its own.
 *
 * BOTH PARAMETERS ARE STRINGS BECAUSE THE ENGINE'S TOOL REGISTRY MARKS AN `FString` PARAMETER
 * REQUIRED whatever its default, which is the note `R-TOOL-SCHEMA` already carries. So "no
 * arguments at all" - the shape an engine event really has - is spelled as an EMPTY STRING and not
 * by omitting the field.
 *
 * IT READS NO LOG AND WRITES NOTHING, so it needs no fixture and has no state to restore. That is
 * not a small thing here: a fixture rebuild stops the editor, and this capability's whole point is
 * to be checkable without one.
 */
FString UTurboMCPReadToolset::ReadTurboMCPPureChecks(
	const FString& Toolsets,
	const FString& Arguments)
{
	// NO `error:` PREFIX GUARD HERE, AND THE REASON IS STRUCTURAL RATHER THAN AN OMISSION. The
	// readers that carry that guard share their body with a writer, so two layers can each assume the
	// other added the prefix. `FTurboMCPPureChecks::Report` is this tool's ONLY producer and it has
	// no refusal path at all, so a guard would be a branch that cannot be false - and a branch that
	// cannot be false is the shape this project has recorded eleven times as a check that cannot
	// fail. The prefix is written in exactly one place, which is `Report`'s first line.
	return FTurboMCPPureChecks::Report(Toolsets, Arguments);
}

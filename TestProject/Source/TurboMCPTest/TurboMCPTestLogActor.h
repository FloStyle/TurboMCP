// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TurboMCPTestLogActor.generated.h"

/**
 * An actor that raises one Warning and one Error when the level starts, so a play check has something real to
 * count AND the Blueprint VM has a node that runs.
 *
 * WHY THIS EXISTS, AND WHY IT IS IN THE TEST PROJECT RATHER THAN IN THE PLUGIN
 * ---------------------------------------------------------------------------
 * `R-PLAY-CHECK` reports the warnings and errors raised during a window. Its suite could not prove that the
 * COUNT IS MEASURED rather than merely shaped, because `TestProject/Content` contained **no `.umap` at all**:
 * Play-In-Editor ran the engine's empty `/Temp/UEDPIE_0_Untitled_1` world, nothing in it executed any code,
 * and a forged zero was indistinguishable from a true zero. That is `QUESTIONS.md` Q-17. The SAME missing
 * map is why the `E-TRACE` subscriber to `FBlueprintCoreDelegates::OnScriptException` was never observed to
 * fire - the VM never reaches an `EX_Tracepoint` in an empty world - which is Q-18. ONE FIXTURE, TWO HOLES.
 *
 * BLUEPRINT ALONE CANNOT RAISE A WARNING, AND THAT IS MEASURED RATHER THAN ASSUMED:
 * `UKismetSystemLibrary::PrintString` logs through `UE_LOGF(LogBlueprintUserMessages, Log, ...)`
 * UNCONDITIONALLY when `bPrintToLog` is set - severity `Log`, and its signature has NO parameter that can
 * raise it. So a C++ helper is the only route to a Warning, whatever fixture is built.
 *
 * IT BELONGS IN THE TEST PROJECT BECAUSE A TOOL MUST NOT TEST ITSELF. A `UE_LOG` added to the plugin's own
 * production code so that the plugin's own suite could pass would make the suite's strength depend on the
 * code it is judging, which would prove nothing.
 *
 * THE MESSAGES CARRY UNMISTAKABLE MARKERS - `TURBOMCP-TESTLOG-WARN` and `TURBOMCP-TESTLOG-ERROR` - so a suite
 * can find them in the engine's log and in the tool's report without matching prose that another part of the
 * engine might also print.
 *
 * THE CLASS IS FOUND AT RUNTIME BY THE COMMANDLET, NOT INCLUDED BY IT: a plugin module cannot depend on a
 * game module (measured - the include failed with "No such file or directory"), so the commandlet searches
 * the class registry for this name.
 */
UCLASS()
class TURBOMCPTEST_API ATurboMCPTestLogActor : public AActor
{
	GENERATED_BODY()

public:
	ATurboMCPTestLogActor();

	/**
	 * Raises the warning, then the error, ONCE EACH by default.
	 *
	 * THE ORDER IS FIXED AND THE COUNT IS EXACTLY ONE EACH, so a caller knows what a correct window contains.
	 * A test that asserted "at least one" would pass on a report that invented extras.
	 */
	virtual void BeginPlay() override;

	/**
	 * Log ONE MORE PAIR PER SECOND of play, off by default.
	 *
	 * This is what lets a test show that the window's LENGTH matters: a report that ignored the budget would
	 * produce the same count for a 2-second and a 6-second window, and nothing else in the fixture would
	 * reveal that.
	 */
	UPROPERTY(EditAnywhere, Category = "TurboMCP Test")
	bool bRepeatEverySecond = false;

	virtual void Tick(float DeltaSeconds) override;

private:
	/**
	 * Time carried since the last repeated pair.
	 *
	 * A MEMBER RATHER THAN A FUNCTION-STATIC: a static accumulator is shared by every instance and survives a
	 * level reload, so a second session would begin part-way through a second and the first pair would land
	 * at an unpredictable time. The whole value of this fixture is that its count is PREDICTABLE.
	 */
	double SecondsSinceLastPair = 0.0;

	/** How many repeated pairs have been logged, so each line is distinguishable in the log. */
	int32 RepeatedPairsLogged = 0;
};

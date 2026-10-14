// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Logging/LogVerbosity.h"
#include "Misc/OutputDevice.h"

#include "TurboMCPTestCompileFixture.generated.h"

/**
 * A Blueprint-callable fixture that makes the ENGINE log during a compile, which no Blueprint can do on its
 * own and no tool call can force.
 *
 * WHY THIS EXISTS
 * ---------------
 * `T04` (`Feedbacks/TurboMCP_TaskList_2026-10-13.md:87-92`) asks `E-COMPILE` to list each ensure and each
 * error the engine logged during a compile, and its verify is "compile a Blueprint that triggers a `queued
 * for compile while compiling` ensure". THE VERIFY HAS BEEN UNRUNNABLE SINCE THE CARD WAS WRITTEN, and the
 * reason is structural rather than a missing test:
 *
 *   - the ensure the card names is `BlueprintCompilationManager.cpp:331`, inside
 *     `FBlueprintCompilationManagerImpl::QueueForCompilation`, and its trigger is `GCompilingBlueprint`
 *     plus a second compile request arriving while the first one is still running;
 *   - THE TOOLS CANNOT BUILD THAT: `EditTurboMCPCompile` compiles sequentially, so the second compile
 *     starts after the first one's guard has been cleared;
 *   - AND NOTHING IN THIS PROJECT LOGS AT `Warning` OR `Error` DURING A COMPILE: every `UE_LOG` in
 *     `BlueprintCompilationManager.cpp` is `Display` (`:1971`, `:1974`, `:1976`, `:2184`, `:2185`), and a
 *     scan of every log under `TestProject/Saved/Logs` finds no `Ensure condition failed` line at all. So
 *     even with the editor running in front of us, a compile of an existing asset logs NOTHING for the
 *     capture to find and the report's engine branch cannot be exercised.
 *
 * WHAT THIS DOES INSTEAD, AND WHY IT IS NOT A FORGERY OF THE RESULT
 * ----------------------------------------------------------------
 * It does not log a warning and call it an ensure. IT RE-CREATES THE CARD'S OWN CONDITION and lets the
 * ENGINE raise its own message:
 *
 *   1. subscribe to `GEditor->OnBlueprintPreCompile()`, which the engine broadcasts from inside the compile
 *      window (`BlueprintCompilationManager.cpp:1365`, guarded at `:691`);
 *   2. compile the Blueprint the caller named;
 *   3. FROM INSIDE THAT CALLBACK, ask the engine to queue ANOTHER Blueprint for compilation.
 *
 * The engine then does what it did in session 2: it logs
 * `Attempting to enqueue <X> for compile while compiling: <Y>` at `Warning` or `Error` through
 * `ensureMsgf` (`AssertionMacros.cpp:683` / `:679`) - a message that never reaches `FCompilerResultsLog`,
 * which is the entire reason the old verdict could answer `clean, 0 warnings` while an ensure had fired.
 *
 * *** IT BELONGS IN THE TEST PROJECT AND NOT IN THE PLUGIN, WHICH IS THE SAME REASON
 * `ATurboMCPTestLogActor` LIVES HERE: a `UE_LOG` added to the plugin so that the plugin's own suite could
 * pass would make the suite's strength depend on the code it is judging. ***
 *
 * THE RUNTIME PAIR IS DELIBERATELY NOT REPEATED HERE. This fixture raises NOTHING of its own: if the engine
 * does not log, this reports a failure rather than manufacturing the line it was hoping to find, so the
 * suite can tell "the report read the engine log" apart from "the fixture wrote what the report wanted".
 */
UCLASS()
class TURBOMCPTEST_API UTurboMCPTestCompileFixture : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Compile `AssetPath` with the engine's pre-compile hook armed, queueing `AlsoQueuePath` from inside it.
	 *
	 * Returns a one-line description of what happened, because a fixture that answers nothing cannot be told
	 * apart from one that was never called. A caller then reads `E-COMPILE`'s own report for the asset
	 * afterwards - this function does NOT return the compile report, on purpose: the report under test must
	 * be the one the TOOL produced.
	 *
	 * @param AssetPath      the Blueprint to compile, for example `/Game/Tests/BP_NodeTest`
	 * @param AlsoQueuePath  a second Blueprint to queue while the first is compiling. Empty means "queue the
	 *                       same asset again", which is still a compile requested while compiling.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurboMCP Test",
		meta = (AICallable, WorldContext = "WorldContextObject"))
	static FString CompileWithQueuedRequest(const FString& AssetPath, const FString& AlsoQueuePath);

	/**
	 * How many engine log lines the fixture's own hook has seen, and whether it has fired at all.
	 *
	 * A check can then tell "the hook never ran" (the engine's compile path changed, and the suite is
	 * measuring nothing) apart from "the hook ran and the engine stayed silent". Those need different fixes
	 * and a single failing assertion cannot distinguish them.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurboMCP Test")
	static FString HookState();

	/**
	 * THE SEVERITY THE ENGINE ACTUALLY USED, MEASURED BY THE FIXTURE'S OWN OUTPUT DEVICE.
	 *
	 * THIS IS THE DECISIVE NUMBER FOR `T04` AND IT HAD TO BE TAKEN THIS WAY. The task's fix only works if the
	 * engine's assertion reaches the log at `Warning` or `Error`, because `FTurboMCPLogCapture` records those
	 * two severities and nothing else (`TurboMCPLogCapture.cpp:70-77`). THE EDITOR'S LOG FILE CANNOT ANSWER
	 * THAT QUESTION: MEASURED, the same ensure is written as `Error` when a suite reads it and as `Verbose`
	 * once `AddExpectedError` has been declared for it, because `FAutomationTestFramework` rewrites the
	 * verbosity of lines it consumes. So the level is taken from inside the process, on the current thread,
	 * by the fixture's own device - the same interface `E-COMPILE` uses - and reported as a word.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurboMCP Test")
	static FString EnsureSeverity();

	/**
	 * Answers ONE question with a measurement: does `GLog` hand a message to a device on the CALLING thread?
	 *
	 * WHY IT DECIDES A REPAIR RATHER THAN SATISFYING CURIOSITY. `FTurboMCPLogCapture` writes down the thread
	 * id it was created on and drops every message that arrives on another thread
	 * (`TurboMCPLogCapture.cpp:62-68`). If delivery follows the caller, that rule is sound and the fixture's
	 * `inGame=no` is a fact about the automation context only. IF DELIVERY NEVER FOLLOWS THE CALLER, THE RULE
	 * IS UNSOUND FOR EVERY TOOL THAT USES THE CAPTURE - `E-COMPILE` and `E-SAVE` alike - and every warning
	 * count they print would be a structural zero.
	 *
	 * The method attaches the same device `CompileWithQueuedRequest` uses, logs one `Warning` from the thread
	 * that calls it, and reports whether the device recorded it.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurboMCP Test")
	static bool ProbeWhetherDeliveryFollowsTheCaller();

	// *** PUBLIC BECAUSE THE OUTPUT DEVICE THAT WRITES THEM IS NOT A MEMBER AND NOT A FRIEND. *** A device
	// has to be a separate non-`UObject` type (see the note on `FTurboMCPTestCompileLogDevice`), so these
	// three counters are its record of what the engine logged; making them private would only mean adding a
	// friend declaration to hide something no caller can misuse.
	/** How many of the engine's log calls reached `Warning` or `Error` while the device was attached. */
	static int32 EnsureLogCount;

	/** The first such line, flattened to one line, so the report can name the text the engine wrote. */
	static FString EnsureLogText;

	/** Which severities arrived, as words, because the severity is `T04`'s whole subject. */
	static FString EnsureLogVerbs;

private:
	/** True while the hook is armed, so the fixture cannot fire on a compile it did not ask for. */
	static bool bHookArmed;

	/** How many pre-compile broadcasts the hook has handled since the last arm. */
	static int32 HookFireCount;

	/**
	 * How many engine ensures were raised while the fixture's compile ran.
	 *
	 * `T04`'s whole subject is an engine message that never reaches the compiler's own results log, and an
	 * ensure is the one such message this project can make the engine raise on demand. The fixture counts
	 * them so a caller can tell "the engine asserted and the report noticed" apart from "nothing happened".
	 */
	static int32 EnsureCount;

	/** The text of the last ensure, flattened to one line, for a report that has to name what was raised. */
	static FString LastEnsureMessage;

	/** The asset queued from inside the hook, kept out of the garbage collector's way while it is in use. */
	static TWeakObjectPtr<UBlueprint> QueuedTarget;
};

/**
 * The output device the fixture listens through, AND IT IS NOT A `UObject` - WHICH IS A MEASURED
 * REQUIREMENT RATHER THAN A PREFERENCE.
 *
 * The first version of this fixture made `UTurboMCPTestCompileFixture` itself inherit `FOutputDevice` and
 * created the device with `static UTurboMCPTestCompileFixture Device;`. THE EDITOR DIED ON THE FIRST RUN
 * WITH `Fatal error: FObjectInitializer::Get() can only be used inside of UObject-derived class
 * constructor` (`UObjectGlobals.cpp:4886`), because a `UObject` may only come into being through
 * `NewObject`, and a function-local static bypasses that. A device also has to outlive the calls that use
 * it, and a garbage-collected object cannot promise that.
 *
 * It is declared here rather than in the `.cpp` because the fixture's static counters are what it writes.
 */
struct FTurboMCPTestCompileLogDevice : public FOutputDevice
{
	//~ Begin FOutputDevice
	virtual void Serialize(const TCHAR* InMessage, ELogVerbosity::Type InVerbosity,
		const FName& InCategory) override;
	//~ End FOutputDevice

	/** The thread this device belongs to; only that thread's messages are recorded, as a capture does. */
	uint32 OwnerThreadId = 0;

	/** True while this device is already inside `Serialize`, so it can never re-enter itself. */
	bool bInsideSerialize = false;

	/**
	 * Every log call this device has seen, at any severity, before the severity filter.
	 *
	 * MEASURED BECAUSE `engineLog` CAME BACK ZERO WHILE THE EDITOR'S LOG SHOWED THE ENGINE'S LINE: a count
	 * that is zero and a count that is filtered to zero look identical from outside, and only a count taken
	 * BEFORE the filter tells them apart.
	 */
	int32 CallsSeen = 0;

	/** How many of those calls arrived on another thread and were therefore not attributed to the compile. */
	int32 OffThreadCalls = 0;

	/** The thread the FIRST log call arrived on, and whether the engine called it the game thread. */
	uint32 FirstCallOwnerId = 0;
	uint32 FirstCallThisId = 0;
	bool bFirstCallInGameThread = false;
};


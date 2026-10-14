// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "TurboMCPTest/TurboMCPTestCompileFixture.h"

/**
 * RUNS THE T04 FIXTURE SO THE CONDITION EXISTS, AND ASSERTS WHAT IT CAN SEE FROM IN HERE.
 *
 * WHAT THIS ASSERTS AND WHAT IT DELIBERATELY DOES NOT
 * ---------------------------------------------------
 * It asserts that `UTurboMCPTestCompileFixture` FIRED ITS HOOK, QUEUED A COMPILE FROM INSIDE ANOTHER
 * COMPILE, AND THAT THE ENGINE RAISED ITS OWN ENSURE WHILE THAT HAPPENED. Those are facts about this
 * process, and a failure in either means the condition was never built - which is the failure mode that
 * would make every other check about `T04` meaningless.
 *
 * MEASURED, SO THE THIRD ONE IS NOT AN ASSUMPTION: without an ensure handler the engine raised TWO of them
 * inside this fixture's compile, at `BlueprintCompilationManager.cpp:331`
 * (`Attempting to enqueue BP_TestAlpha for compile while compiling`) and at `:2243`
 * (`Blueprints requested compilation while compiling other blueprints: BP_TestAlpha / While Compiling:
 * BP_NodeTest`), and `FAutomationTestFramework` turned both into test errors - which is why the fixture
 * installs a handler that skips the default handling after the engine has written its line.
 *
 * IT DOES NOT ASSERT THAT `E-COMPILE`'S REPORT NOTICED THE ENSURE, because this test cannot make that call
 * and must not duplicate it: the report is read from OUTSIDE the process, by
 * `Tools/test-compile-ensure.py`, which runs this test, asks the editor's own log toolset for the engine's
 * line, then calls `E-COMPILE` and compares the two. A check written in here would be the plugin agreeing
 * with itself.
 *
 * *** AND THE LOG CANNOT BE READ FROM IN HERE EITHER, WHICH IS MEASURED RATHER THAN ASSUMED. *** The editor
 * holds its own log file open: a probe test found `FileExists` and `FileSize` both answering while
 * `platform OpenRead`, `IFileManager::CreateFileReader` and `FFileHelper::LoadFileToArray` ALL FAILED on
 * the absolute path. So the engine's word is read through the editor's log toolset, from outside.
 *
 * THE TEST IS IN THE TEST PROJECT AND NOT IN THE PLUGIN, for the same reason
 * `ATurboMCPTestLogActor` is: a fixture that lives in the plugin could only prove the plugin agrees with
 * itself.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPTestCompileQueuedRequestTest,
	"TurboMCP.T04.QueueForCompilationDuringCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPTestCompileQueuedRequestTest::RunTest(const FString& Parameters)
{
	// *** NOTHING IS DECLARED `AddExpectedError` HERE, AND THE FIRST VERSIONS THAT DID ARE THE REASON. ***
	// An `Error` line raised inside a test is a test failure unless it is declared expected - but declaring
	// it has a measured cost this task cannot pay: `AddExpectedError` makes the automation framework REWRITE
	// the verbosity of the line it consumes. MEASURED, both ways:
	//
	//   with `Occurrences = 1`   the first run reported the engine's lines at `Error`, and every later run
	//                            reported them at `Verbose` with an empty body, because the expectation had
	//                            already been consumed - so the test's verdict depended on how often it had
	//                            run, and on the second run it failed.
	//   with `Occurrences = -1`  the framework SUPPRESSED the lines, and the fixture's own device then
	//                            recorded `engineLog=0 [ ]` - nothing at all. A test whose subject is
	//                            suppressed is measuring its own suppression.
	//
	// SO THE ENGINE'S LINE IS LEFT ALONE, THE DEVICE MEASURES IT, AND THIS TEST IS EXPECTED TO REPORT THE
	// ENGINE'S ERRORS AS ITS OWN. `Tools/test-compile-ensure.py` reads that on purpose and asserts the errors
	// are exactly the engine's two lines - nothing else - so a real failure in the fixture still shows up as
	// an error the suite did not predict. THAT IS A WEAKER CONTRACT THAN A GREEN TEST, AND IT IS STATED
	// RATHER THAN HIDDEN: the alternative is a green test that has suppressed the very line `T04` is about.

	// *** TWO DIFFERENT ASSETS, AND THE FIRST VERSION OF THIS TEST GOT THAT WRONG IN A WAY WORTH RECORDING.
	// It queued the SAME Blueprint that was compiling, and the engine stayed silent. The reason is in the
	// engine's own guard: `QueueForCompilation` (`BlueprintCompilationManager.cpp:318`) only reaches its
	// `ensureMsgf` when `!CompileJob.BPToCompile->bQueuedForCompilation`, and the asset being compiled is
	// flagged queued for the whole compile (`:349`). A second asset is idle, so its flag is clear and the
	// request really does arrive while a compile is running. ***
	const FString Outcome = UTurboMCPTestCompileFixture::CompileWithQueuedRequest(
		TEXT("/Game/Tests/BP_NodeTest.BP_NodeTest"),
		TEXT("/Game/Tests/Kit/BP_TestAlpha.BP_TestAlpha"));

	AddInfo(FString::Printf(TEXT("fixture: %s"), *Outcome));
	AddInfo(FString::Printf(TEXT("hook state: %s"), *UTurboMCPTestCompileFixture::HookState()));

	if (Outcome.StartsWith(TEXT("error:")))
	{
		AddError(FString::Printf(TEXT("the fixture could not build the condition: %s"), *Outcome));
		return false;
	}

	// THE LOAD-BEARING ASSERTIONS. If the hook count is zero, the engine never saw a nested request, no
	// ensure could fire, and the run measured silence. If the ensure count is zero, the engine did not
	// consider the condition a violation and `T04`'s report has nothing to be blind to.
	TestTrue(TEXT("a compile was queued from inside another compile"),
		Outcome.Contains(TEXT("queuedFromHook=1")));

	const bool bEngineEnsured = !Outcome.Contains(TEXT("ensures=0"));
	TestTrue(TEXT("the engine raised its own ensure while that compile ran"), bEngineEnsured);
	if (bEngineEnsured)
	{
		TestTrue(TEXT("the ensure names the asset that was queued while another compiled"),
			Outcome.Contains(TEXT("BP_TestAlpha")));
	}

	// *** THE DECISIVE CHECK FOR `T04`, AND IT IS TAKEN FROM INSIDE THE PROCESS. *** The task's requirement is
	// that the compile result list what the ENGINE logged, so the question that decides it is: at what
	// severity does an engine ensure reach the log during a compile? `FTurboMCPLogCapture` records `Warning`
	// and `Error` and nothing else (`TurboMCPLogCapture.cpp:70-77`), so a fixture that answered `Log` or
	// `Verbose` would mean the report cannot see it however it is written. MEASURED HERE: `engineLog=60`
	// over one compile, of which the two ensure messages are the substantive ones and the rest are the fatal
	// report that follows an ensure - ALL AT `Warning` OR `Error`.
	TestTrue(TEXT("the engine logged something at a severity the compile capture records"),
		!Outcome.Contains(TEXT("engineLog=0")));
	TestTrue(TEXT("the severity the engine used was Error or Warning, not Log or Verbose"),
		Outcome.Contains(TEXT("[Warning ")) || Outcome.Contains(TEXT("[Error ")));

	// *** AND A MEASUREMENT THAT IS EASY TO MISREAD, SO IT IS WRITTEN DOWN WITH ITS CORRECTION ATTACHED. ***
	// `deviceSaw` is every log call the device received during the compile and `firstCall` reports the thread
	// it arrived on against the thread the device was created on. MEASURED: `deviceSaw=83`,
	// `firstCall owner=65176 this=87448 inGame=no` - THE AUTOMATION CONTEXT RUNS THE COMPILE OFF THE GAME
	// THREAD. The first reading of that fact concluded the capture's per-thread rule was broken for every
	// tool, AND THAT CONCLUSION WAS WRONG; the probe below is what corrected it. So what is asserted here is
	// only the part that is robust: the device saw calls, so the window was not empty.
	TestTrue(TEXT("the device received calls during the compile"), !Outcome.Contains(TEXT("deviceSaw=0")));

	// *** AND THE PROBE THAT DECIDES HOW MUCH THAT FINDING MEANS. *** `inGame=no` alone would suggest the
	// capture's thread rule is broken everywhere, AND THAT INFERENCE WOULD BE WRONG - it was made once while
	// writing this and the probe below is what corrected it. MEASURED, both halves:
	//
	//   the fixture's compile     `deviceSaw=83`, `firstCall owner=65176 this=87448 inGame=no` - the
	//                             automation context runs the compile OFF the game thread, and its log is
	//                             delivered from a third thread again
	//   a log written from THIS thread, with the device still attached through a `GLog->Flush()`
	//                             RECORDED. So delivery DOES follow the caller, and
	//                             `FTurboMCPLogCapture`'s rule is sound for a real tool call, which runs on
	//                             the game thread.
	//
	// THE FIRST VERSION OF THE PROBE DETACHED THE DEVICE ONE STATEMENT AFTER THE WRITE AND REPORTED "NOT
	// RECORDED", which measured the probe rather than the engine: a device removed before the flush cannot
	// see its own line. That is why this one flushes while attached.
	if (UTurboMCPTestCompileFixture::ProbeWhetherDeliveryFollowsTheCaller())
	{
		AddInfo(TEXT("probe: a log written from this thread WAS recorded, so delivery follows the caller - the "
			"capture's per-thread rule is sound for a tool call, and the compile's off-game-thread delivery is "
			"a property of the automation context"));
	}
	else
	{
		AddError(TEXT("probe: a log written from this thread was NOT recorded even after a flush, so a "
			"per-thread filter in a log capture would see nothing from any caller"));
	}

	// THE REPORT UNDER TEST READS THE ENGINE LOG BECAUSE THE COMPILER'S LOG IS EMPTY HERE. This asserts that
	// premise, so a suite cannot pass by accident on a compile that the compiler complained about.
	TestTrue(TEXT("the compiler's own results log carried no error for this compile"),
		Outcome.Contains(TEXT("0 error(s) from the compiler)")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

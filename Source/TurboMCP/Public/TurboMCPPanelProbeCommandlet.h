// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"

#include "TurboMCPPanelProbeCommandlet.generated.h"

/**
 * Probes the panel's tool runner, so that the half of the panel that CAN be verified is verified.
 *
 * WHY THIS EXISTS
 * ---------------
 * `QUESTIONS.md` Q-11 records the gap in the panel's coverage: the panel's widget tree cannot be read
 * because this plugin has no Slate inspection tool, so **no automated check covers its layout**.
 *
 * The panel was therefore built so that as much as possible is NOT Slate. Discovery, sample generation
 * and execution live in `FTurboMCPPanelToolRunner`, which is plain C++ - and the panel is a thin shell
 * that renders it. But when Q-11 was written **nothing reached the runner either**: not an MCP tool,
 * not a commandlet. The testable half existed and was untested, which is the same shape of mistake as a
 * check that passes while the thing it verifies is broken.
 *
 * WHY A COMMANDLET RATHER THAN A TOOL
 * -----------------------------------
 * `P9` says the panel and the MCP share one code path. It does **not** say a panel feature belongs in
 * the MCP API. The runner is a property of the PANEL, so exposing it as an `AICallable` tool would put
 * a UI concern into the surface an agent reads and would tell an agent about a widget it cannot use.
 * A commandlet is a build step, like the `.cpp` next to it, and it reaches the runner without widening
 * the MCP.
 *
 * WHAT IT PROVES, AND EACH OF THESE WAS UNVERIFIED BEFORE IT
 * ---------------------------------------------------------
 * 1. the registry answers, or it refuses with the reason;
 * 2. the four TurboMCP toolsets are among the ones the registry publishes;
 * 3. **each toolset still has tools** - the check that stops a toolset silently shrinking, which no
 *    other check in this project makes: `R-TOOL-INDEX` counts what is there and cannot know what
 *    should be;
 * 4. every tool of every TurboMCP toolset produces a sample, and which ones are PARTIAL, because a
 *    partial sample is a tool a human **cannot try with one click** - which is exactly the user's
 *    requirement that every MCP feature be testable from the panel;
 * 5. whether a REAL CALL completes without an editor tick. See the measurement note below.
 *
 * HOW IT IS INVOKED
 * -----------------
 *     UnrealEditor-Cmd.exe <project> -run=TurboMCP.TurboMCPPanelProbeCommandlet -unattended -nullrhi
 *
 * THE `-run=` TOKEN NEEDS A MODULE PREFIX, and that is a measured engine fact rather than a style
 * choice. `LaunchEngineLoop.cpp:4100` calls `LoadModule` **only when the token contains a period**, so
 * `-run=TurboMCPPanelProbeCommandlet` loads nothing and answers "looked like a commandlet, but we could
 * not find the class". The `.` is the entire mechanism. A static initialiser that called `LoadModule`
 * was tried instead and CRASHED with an access violation in
 * `FModuleManager::LoadModuleWithFailureReason`, so there is none here.
 *
 * THE LIVE CALL IS A MEASUREMENT, NOT A GUARANTEE
 * -----------------------------------------------
 * `FTurboMCPPanelToolRunner::CallAndWait` sleeps on the CALLING thread while the completion is
 * delivered by the EDITOR'S TICK. Called from the game thread it would block the very tick that
 * delivers the answer, and a `-run=` commandlet has no editor tick at all. So this commandlet runs it
 * with a SHORT timeout and REPORTS WHAT HAPPENED rather than assuming either way. A commandlet that
 * hangs is worse than one that admits a limit, so if it cannot work the rest of the probe still runs
 * and the summary says so.
 */
UCLASS()
class UTurboMCPPanelProbeCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UTurboMCPPanelProbeCommandlet();

	virtual int32 Main(const FString& Params) override;

private:
	/** The four toolsets this plugin publishes, in the order the acceptance rows name them. */
	static const TArray<FString>& TurboMCPToolsetNames();

	/** Probes one toolset: its tools, their samples, and which samples are partial. */
	bool ProbeToolset(const FString& InToolsetName, int32& OutToolCount, int32& OutPartialCount);

	/** Runs one real call and reports what the registry did. */
	void ProbeLiveCall();
};

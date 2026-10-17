// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Set.h"
#include "Delegates/IDelegateInstance.h"

class UEdGraphNode;

/**
 * `P/E-TRACE`'s engine side: a non-pausing trace of the nodes a caller marked.
 *
 * `Spec/03_TOOLS.md` asks that marking a node by ID records time, node, pin values and named variables every
 * time it runs in a play session, and that it NEVER PAUSES. Those two requirements rule out the obvious
 * implementations and are the whole reason this module is shaped the way it is.
 *
 * WHY NOT A BREAKPOINT. A Blueprint breakpoint sets `bShouldBreakExecution = true`
 * (Editor/UnrealEd/Private/Kismet2/KismetDebugUtilities.cpp:301-302) and therefore STOPS THE GAME. The engine
 * itself distinguishes the two paths at `:299-306`: `Breakpoint` breaks unconditionally while `Tracepoint`
 * breaks ONLY while already single-stepping. A trace built on breakpoints would be a different capability
 * wearing this one's name.
 *
 * WHY THE SUBSCRIPTION IS THE WHOLE MECHANISM, AND WHY THERE IS NO GRAPH EDIT. `EX_Tracepoint` is already in
 * the bytecode of every normally-compiled Blueprint: Editor/KismetCompiler/Private/KismetCompilerVMBackend.cpp
 * :1969-1971 emits it for `KCST_DebugSite`, and the only thing that suppresses it is
 * `Kismet.bSuppressInstrumentation` (`:1961`), which this project does not set. The VM raises it through
 * `FBlueprintCoreDelegates::OnScriptException` in `execTracepoint`
 * (Runtime/CoreUObject/Private/UObject/ScriptCore.cpp:2529-2537), and `IsDebuggingEnabled()` returns `GIsEditor`
 * (`:290-297`), so in an editor build the trace points are ALWAYS LIVE and need no opt-in.
 *
 * SO ARMING A POINT IS AN IN-MEMORY REGISTRY ENTRY AND NOTHING ELSE. NO node is inserted, NO Blueprint is
 * recompiled, and `AllocateDefaultPins` - the call whose omission has already crashed this project once and
 * silently corrupted a graph once - is never reached, because this module never creates a node.
 *
 * THE ONE THING THIS CLASS DELIBERATELY DOES NOT COPY FROM THE ENGINE. `FKismetDebugUtilities::OnScriptException`
 * records a sample only when `ObjectBeingDebugged == ActiveObject`
 * (Editor/UnrealEd/Private/Kismet2/KismetDebugUtilities.cpp:411), because it is driving the debugger and cares
 * about the object the USER put in the debugger. A trace tool must record every hit for a marked node whatever
 * is being debugged, so that gate is NOT reproduced here. Copying it would make this capability silently
 * produce nothing whenever the Blueprint editor was not watching the running object - the exact failure mode
 * that looks like "the game never reached that node" rather than like a bug.
 */
namespace TurboMCPTrace
{
	/** One observed execution of a marked node. */
	struct FHit
	{
		/** The Blueprint node ID the node was marked by, so a caller can match a hit to what it armed. */
		FString NodeId;

		/** The node's own title, for a reader who has not memorised keys. */
		FString NodeTitle;

		/** The object that ran the node, named by path. A Blueprint can have many instances. */
		FString ContextName;

		/** The frame's time, in seconds, as `FPlatformTime::Seconds()` reports it. */
		double TimeSeconds = 0.0;

		/**
		 * The thread the hit arrived on, RECORDED RATHER THAN FILTERED ON.
		 *
		 * MEASURED, and this project has paid for the lesson twice: `TurboMCPLogCapture` filters records to the
		 * thread that installed it, which is RIGHT for a save - one asset, one thread - and WRONG for a running
		 * session, which executes from many. Filtering here would produce an empty trace that looks like a
		 * clean run, and the play-check workstream already measured exactly that symptom on the engine's log.
		 */
		uint32 ThreadId = 0;
	};

	/** A point the caller marked, and how many times it has run. */
	struct FArmed
	{
		FString NodeId;
		int32 HitCount = 0;
	};

	/** What the caller asked for, decided before anything is changed. */
	struct FPlan
	{
		/** `arm`, `clear` or `status`. */
		FString Action;

		/** The points to arm or clear. Empty with `clear` means every point. */
		TArray<FString> NodeIds;

		/** True when the action was understood and its arguments were usable. */
		bool bAccepted = false;

		/** Why the action was refused, when it was. */
		FString Reason;
	};

	/**
	 * Decides what a request will do, and refuses what it cannot honour.
	 *
	 * EVERY CHECK LIVES HERE RATHER THAN IN THE TOOL, so the Read and Edit entry points reach the same decision
	 * and cannot disagree about what is legal - the rule this project has recorded since phase 1.
	 */
	bool Plan(const FString& Action, const FString& NodeId, FPlan& OutPlan, FString& OutError);

	/** Carries out an accepted plan. Only the Edit entry point may call this. */
	FString Apply(const FPlan& InPlan);

	/** Reports the subscription and every armed point with its hit count. */
	FString Status();

	/** Every hit observed so far, newest last. */
	/**
	 * The recorded hits, AS A COPY TAKEN UNDER THE LOCK.
	 *
	 * *** THIS RETURNED `const TArray<FHit>&` AND TOOK NO LOCK, WHILE THE CALLBACK MUTATES `GHits` UNDER `GMutex`
	 * (`RemoveAt(0)` when the ring is full) - SO THE ONE ACCESSOR THAT HANDED OUT THE ARRAY ITSELF WAS THE ONE
	 * THAT DID NOT GUARD IT, ALONGSIDE `HitCount()`, `Status()` AND `ClearAll()` WHICH ALL DO. *** *** A CALLER
	 * HOLDING THAT REFERENCE WOULD HAVE BEEN READING A BUFFER ANOTHER THREAD COULD REALLOCATE. ***
	 * *** NOTHING CALLS THIS TODAY, SO THE COPY COSTS NOTHING AND MAKES THE FIRST CALLER SAFE RATHER THAN LUCKY. ***
	 */
	TArray<FHit> Hits();

	/** How many hits have been recorded since the registry was last cleared. */
	int32 HitCount();

	/** True when this module is subscribed to the engine's script exception delegate. */
	bool IsSubscribed();

	/** Drops every armed point and every recorded hit. Does not unsubscribe. */
	void ClearAll();

	/** Forgets a session: unsubscribes and clears. Safe to call when nothing was ever armed. */
	void Shutdown();
}

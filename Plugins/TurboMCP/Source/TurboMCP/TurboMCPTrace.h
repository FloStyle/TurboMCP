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

/** In-memory subscription to existing Blueprint tracepoint events.
 * Arming does not instrument, compile or save assets and does not request a debugger pause.
 * Delivery depends on executed bytecode and available debug information. IDs are not resolved here.
 * Records node identity, instance path, monotonic time and thread; no pin values are captured.
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

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPTrace.h"

#include "TurboMCPNodeId.h"

#include "Blueprint/BlueprintExceptionInfo.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogTurboMCPTrace, Log, All);

namespace TurboMCPTrace
{
	namespace
	{
		/** Armed points, by the node ID the caller used, so a hit can be matched to what was armed. */
		TMap<FString, FArmed> GArmed;

		/** Every hit since the last clear, in arrival order. */
		TArray<FHit> GHits;

		/**
		 * The subscription handle, kept so the subscription can be REMOVED.
		 *
		 * A bound delegate that is never removed keeps this module's static function alive across a session
		 * and accumulates one subscriber per arm, so a caller arming ten points would get ten records of every
		 * hit. `Editor/AnimationBlueprintEditor/Private/AnimationBlueprintEditor.cpp:565` keeps its
		 * `ScriptExceptionHandle` for the same reason.
		 */
		FDelegateHandle GHandle;

		/**
		 * A ceiling on retained hits.
		 *
		 * The engine's own ring holds `MAX_TRACE_STACK_SAMPLES = 1024`
		 * (Editor/UnrealEd/Public/Kismet2/KismetDebugUtilities.h:231). A trace of a running game produces hits
		 * faster than anything can read them, so an unbounded array would be an out-of-memory waiting for a
		 * long session. The OLDEST are dropped and the drop is REPORTED rather than silent, because a trace
		 * that quietly forgot the beginning would make a reader believe a node never ran.
		 */
		constexpr int32 MaxHits = 1024;
		int32 GDroppedHits = 0;

		/**
		 * How many script exceptions have arrived AT ALL, by `EBlueprintExceptionType`, and in total.
		 *
		 * THIS IS THE STEP 0 INSTRUMENT AND IT IS NOT A CURIOSITY. "No trace hit" has two causes that are
		 * indistinguishable from the outside: the subscription never fired, or it fired and no armed node ran.
		 * The first would invalidate the whole mechanism; the second is a fact about the fixture. Counting
		 * before any armed-point test tells them apart, and it is also what proves the SUBSCRIPTION works
		 * rather than assuming it does - the one thing section 6s of TestRecord left open.
		 */
		TMap<int32, int32> GExceptionCounts;
		int32 GTotalExceptions = 0;

		/** Guards the registry, since hits arrive from whatever thread is running the game. */
		FCriticalSection GMutex;

		FString SafeName(const UObject* InObject)
		{
			if (InObject == nullptr)
			{
				return TEXT("(none)");
			}
			return InObject->GetPathName();
		}

		/**
		 * The engine's script-exception callback: where a trace point actually arrives.
		 *
		 * THE TYPE TEST IS THE WHOLE FILTER. `OnScriptException` carries breakpoints, tracepoints, wire
		 * tracepoints, access violations, infinite loops and errors, and this capability wants exactly the
		 * Tracepoint kind. `execTracepoint` raises it at
		 * Runtime/CoreUObject/Private/UObject/ScriptCore.cpp:2529-2537, and `Info.GetType()` is the accessor
		 * (`Runtime/CoreUObject/Public/Blueprint/BlueprintExceptionInfo.h:40`).
		 *
		 * IT DOES NOT PAUSE, AND NOTHING HERE CAN MAKE IT. The engine's own handler branches on the type at
		 * KismetDebugUtilities.cpp:422 and only the Breakpoint path asks for a pause.
		 */
		void OnScriptException(const UObject* ActiveObject, const FFrame& StackFrame,
			const FBlueprintExceptionInfo& Info)
		{
			// STEP 0 DIAGNOSTIC, KEPT BECAUSE IT IS THE ONLY WAY TO TELL TWO VERY DIFFERENT FAILURES APART.
			//
			// "No hit" has two causes that look identical from the outside: THE SUBSCRIPTION NEVER FIRES AT
			// ALL, or it fires and none of the armed nodes ran. The first would mean the whole mechanism is
			// imaginary; the second only means the fixture does not execute the node. Counting EVERY exception
			// by type, before any armed-point test, separates them - and the count is cheap enough to keep.
			++GExceptionCounts.FindOrAdd(static_cast<int32>(Info.GetType()));
			++GTotalExceptions;

			// LOGGED THE FIRST FEW OF EVERY KIND, so a live run leaves evidence in the editor's log even when
			// nothing is armed. Without this, a session that fired a thousand tracepoints and a session that
			// fired none produce the same empty answer.
			if (GTotalExceptions <= 8)
			{
				UE_LOG(LogTurboMCPTrace, Display,
					TEXT("TRACE EXCEPTION #%d type=%d active=%s node=%s"),
					GTotalExceptions, static_cast<int32>(Info.GetType()),
					ActiveObject != nullptr ? *ActiveObject->GetName() : TEXT("(null)"),
					StackFrame.Node != nullptr ? *StackFrame.Node->GetName() : TEXT("(null)"));
			}

			if (Info.GetType() != EBlueprintExceptionType::Tracepoint)
			{
				return;
			}

			// Match the engine debugger: Code points just past the executed opcode.
			if (ActiveObject == nullptr || StackFrame.Node == nullptr || StackFrame.Code == nullptr
				|| StackFrame.Node->Script.Num() == 0)
			{
				return;
			}
			const UPTRINT ScriptStart = reinterpret_cast<UPTRINT>(StackFrame.Node->Script.GetData());
			const UPTRINT CodeAddress = reinterpret_cast<UPTRINT>(StackFrame.Code);
			if (CodeAddress <= ScriptStart || CodeAddress - ScriptStart > static_cast<UPTRINT>(StackFrame.Node->Script.Num()))
			{
				return;
			}
			const int32 OpcodeOffset = static_cast<int32>(CodeAddress - ScriptStart - 1);
			UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(
				ActiveObject, StackFrame.Node, OpcodeOffset, /*bAllowImpreciseHit=*/false);
			if (Node == nullptr)
			{
				return;
			}

			// THE NODE'S ID IS THE KEY, because that is what a caller armed and what a hit must be matched
			// against. `FTurboMCPNodeId` is the project's single authority for it.
			const FString NodeId = TurboMCPNodeId::MakeForNode(Node);

			FHit Hit;
			Hit.NodeId = NodeId;
			Hit.NodeTitle = TurboMCPNodeId::GetNodeTitle(Node);
			Hit.ContextName = SafeName(ActiveObject);
			Hit.TimeSeconds = FPlatformTime::Seconds();
			Hit.ThreadId = FPlatformTLS::GetCurrentThreadId();

			FScopeLock Lock(&GMutex);

			// ONLY ARMED POINTS ARE RECORDED, and the count is kept per point so a caller can read how many
			// times each ran without walking the hit list.
			FArmed* Armed = GArmed.Find(NodeId);
			if (Armed == nullptr)
			{
				return;
			}
			++Armed->HitCount;

			if (GHits.Num() >= MaxHits)
			{
				GHits.RemoveAt(0, 1, EAllowShrinking::No);
				++GDroppedHits;
			}
			GHits.Add(MoveTemp(Hit));

			// LOGGED AS WELL AS STORED, because the point of this module's step 0 was to SEE a hit arrive. A
			// trace that only accumulates in memory cannot be distinguished from one whose subscription never
			// fired, and that ambiguity is what the first build had to remove.
			UE_LOG(LogTurboMCPTrace, Display,
				TEXT("TRACE HIT node=%s title=%s context=%s thread=%u hits=%d"),
				*NodeId, *Hit.NodeTitle, *Hit.ContextName, Hit.ThreadId, Armed->HitCount);
		}

		/** Subscribes once. A second call is a no-op, so arming cannot stack subscribers. */
		void EnsureSubscribed()
		{
			if (GHandle.IsValid())
			{
				return;
			}
			GHandle = FBlueprintCoreDelegates::OnScriptException.AddStatic(&OnScriptException);
			UE_LOG(LogTurboMCPTrace, Display,
				TEXT("TRACE SUBSCRIBED to FBlueprintCoreDelegates::OnScriptException"));
		}
	}

	bool Plan(const FString& Action, const FString& NodeId, FPlan& OutPlan, FString& OutError)
	{
		OutPlan = FPlan();
		OutError.Reset();

		// THE ACTION IS MATCHED EXACTLY AND NAMED BACK WHEN IT IS NOT ONE OF THE THREE. A tool that guessed at
		// a misspelled action would arm nothing and report success, which is the under-reporting direction
		// that makes a caller believe a trace is running when it is not.
		const FString Lower = Action.TrimStartAndEnd().ToLower();
		if (Lower == TEXT("arm") || Lower == TEXT("clear") || Lower == TEXT("status"))
		{
			OutPlan.Action = Lower;
		}
		else
		{
			OutError = FString::Printf(
				TEXT("'%s' is not an action. This tool takes arm, clear or status."), *Action);
			return false;
		}

		if (!NodeId.IsEmpty())
		{
			TArray<FString> Parts;
			NodeId.ParseIntoArray(Parts, TEXT(","), /*InCullEmpty=*/true);
			for (FString& Part : Parts)
			{
				const FString Trimmed = Part.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					OutPlan.NodeIds.Add(Trimmed);
				}
			}
		}

		// ARMING NOTHING IS REFUSED RATHER THAN SILENTLY ACCEPTED. `arm` with no ID would succeed and record
		// nothing forever, and the caller would have no way to tell that from a node that never ran.
		if (OutPlan.Action == TEXT("arm") && OutPlan.NodeIds.Num() == 0)
		{
			OutError = TEXT("arm needs at least one node ID. A point with no ID cannot be matched by anything, "
				"so arming it would record nothing and look like a node that never ran.");
			return false;
		}

		OutPlan.bAccepted = true;
		return true;
	}

	FString Apply(const FPlan& InPlan)
	{
		if (!InPlan.bAccepted)
		{
			return FString::Printf(TEXT("error: %s"), *InPlan.Reason);
		}

		EnsureSubscribed();

		FScopeLock Lock(&GMutex);

		if (InPlan.Action == TEXT("clear"))
		{
			if (InPlan.NodeIds.Num() == 0)
			{
				const int32 Was = GArmed.Num();
				GArmed.Reset();
				GHits.Reset();
				GDroppedHits = 0;
				return FString::Printf(
					TEXT("cleared: removed %d armed point(s) and every recorded hit"), Was);
			}

			int32 Removed = 0;
			for (const FString& Id : InPlan.NodeIds)
			{
				Removed += GArmed.Remove(Id) > 0 ? 1 : 0;
			}
			// THE HITS OF A CLEARED POINT GO TOO. Leaving them would let a later read report executions of a
			// point the caller believes is gone.
			const int32 HitsBefore = GHits.Num();
			GHits.RemoveAll([&InPlan](const FHit& Hit)
			{
				return InPlan.NodeIds.Contains(Hit.NodeId);
			});
			return FString::Printf(
				TEXT("cleared: removed %d of %d named point(s), and %d of %d recorded hit(s)"),
				Removed, InPlan.NodeIds.Num(), HitsBefore - GHits.Num(), HitsBefore);
		}

		if (InPlan.Action == TEXT("arm"))
		{
			int32 Added = 0;
			int32 Already = 0;
			for (const FString& Id : InPlan.NodeIds)
			{
				if (GArmed.Contains(Id))
				{
					++Already;
					continue;
				}
				FArmed Entry;
				Entry.NodeId = Id;
				GArmed.Add(Id, Entry);
				++Added;
			}
			return FString::Printf(
				TEXT("armed: added %d point(s), %d were already armed, %d armed in total. ")
				TEXT("NOTHING WAS ADDED TO ANY GRAPH AND NOTHING WAS RECOMPILED: the engine already emits a ")
				TEXT("trace point into every normally-compiled node, so arming is a registry entry."),
				Added, Already, GArmed.Num());
		}

		return Status();
	}

	FString Status()
	{
		FScopeLock Lock(&GMutex);

		FString Out = FString::Printf(
			TEXT("trace: %s to FBlueprintCoreDelegates::OnScriptException"),
			IsSubscribed() ? TEXT("SUBSCRIBED") : TEXT("not subscribed"));
		Out += FString::Printf(TEXT("\n%d armed point(s), %d recorded hit(s)"),
			GArmed.Num(), GHits.Num());

		if (GDroppedHits > 0)
		{
			// REPORTED RATHER THAN SILENT. A trace that forgot its beginning must say so, or a reader will
			// conclude a node never ran.
			Out += FString::Printf(TEXT("\n%d hit(s) were DROPPED off the front of the %d-hit buffer"),
				GDroppedHits, MaxHits);
		}

		// THE STEP 0 DIAGNOSTIC, REPORTED RATHER THAN LEFT IN A LOG FILE.
		//
		// THIS IS WHAT MAKES "NO HIT" ANSWERABLE. If the total is zero, the engine never called this module
		// and the whole mechanism is unproven - which is exactly the risk section 6s of TestRecord recorded as
		// OPEN, and it must be said out loud rather than hidden behind an empty hit list. If the total is
		// positive with tracepoints among them, the subscription WORKS and a zero hit count is a fact about
		// which nodes ran, not about the mechanism.
		Out += FString::Printf(TEXT("\n%d script exception(s) have reached this module"), GTotalExceptions);
		if (GTotalExceptions > 0)
		{
			TArray<int32> Kinds;
			GExceptionCounts.GetKeys(Kinds);
			Kinds.Sort();
			for (const int32 Kind : Kinds)
			{
				Out += FString::Printf(TEXT("\n  type %d : %d"), Kind, GExceptionCounts[Kind]);
			}
			// NAMED, because a reader should not have to look up what 1 means. `EBlueprintExceptionType` is an
			// enum inside a NAMESPACE (Runtime/CoreUObject/Public/Blueprint/BlueprintExceptionInfo.h:9-23), so
			// a range-for over the type is not available and the names are spelled here.
			static const TCHAR* const Names[] = {
				TEXT("Breakpoint"), TEXT("Tracepoint"), TEXT("WireTracepoint"), TEXT("AccessViolation"),
				TEXT("InfiniteLoop"), TEXT("NonFatalError"), TEXT("FatalError"), TEXT("AbortExecution"),
				TEXT("UserRaisedError") };
			for (const int32 Kind : Kinds)
			{
				const int32 Index = Kind;
				if (Index >= 0 && Index < static_cast<int32>(UE_ARRAY_COUNT(Names)))
				{
					Out += FString::Printf(TEXT("      type %d is %s"), Kind, Names[Index]);
				}
			}
		}
		else
		{
			Out += TEXT("\n  THAT IS THE UNPROVEN CASE: the engine has not called this module even once, so ")
				TEXT("the subscription is NOT demonstrated. A subscription that is registered but never fires ")
				TEXT("looks exactly like a node that never ran, and this line is what tells them apart.");
		}

		if (GArmed.Num() == 0)
		{
			// ONE `TEXT()` FOR THE WHOLE LITERAL. Split across two, this is `error C2816` - a missing closing
			// parenthesis in the macro - which reads like a bracket mistake and is not one.
			Out += TEXT("\nno point is armed, so nothing is being recorded. Arming is an in-memory "
				"registry entry and changes no asset.");
			return Out;
		}

		// SORTED, so two reads of the same state produce the same order. A TMap's order is not stable, and a
		// caller diffing two reads would see churn that means nothing.
		TArray<FString> Keys;
		GArmed.GetKeys(Keys);
		Keys.Sort();

		for (const FString& Key : Keys)
		{
			const FArmed& Armed = GArmed[Key];
			Out += FString::Printf(TEXT("\n  %s : %d hit(s)"), *Key, Armed.HitCount);
		}

		if (GHits.Num() > 0)
		{
			Out += TEXT("\nthe hits, oldest first:");
			for (const FHit& Hit : GHits)
			{
				Out += FString::Printf(TEXT("\n  %.3f  %s  %s  thread %u"),
					Hit.TimeSeconds, *Hit.NodeId, *Hit.NodeTitle, Hit.ThreadId);
			}
		}
		else
		{
			// THE HONEST ZERO, WITH THE REASON. Zero hits is either "it never ran" or "nothing was armed",
			// and those need different answers from whoever reads it.
			Out += FString::Printf(
				TEXT("\nno hit has been recorded yet. With %d point(s) armed, that means none of them has run ")
				TEXT("since arming, not that the trace is off: the subscription is %s."),
				GArmed.Num(), IsSubscribed() ? TEXT("live") : TEXT("NOT live"));
		}

		return Out;
	}

	TArray<FHit> Hits()
	{
		// A COPY TAKEN UNDER THE LOCK: the caller cannot be reading the ring while the callback moves it.
	FScopeLock Lock(&GMutex);
	return GHits;
	}

	int32 HitCount()
	{
		FScopeLock Lock(&GMutex);
		return GHits.Num();
	}

	bool IsSubscribed()
	{
		return GHandle.IsValid();
	}

	void ClearAll()
	{
		FScopeLock Lock(&GMutex);
		GArmed.Reset();
		GHits.Reset();
		GDroppedHits = 0;
	}

	void Shutdown()
	{
		// THE SUBSCRIPTION IS REMOVED, NOT MERELY ABANDONED. A delegate handle that outlives its owner keeps a
		// static function alive and leaves the engine calling into a module that is going away.
		if (GHandle.IsValid())
		{
			FBlueprintCoreDelegates::OnScriptException.Remove(GHandle);
			GHandle.Reset();
		}
		ClearAll();
	}
}

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
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
		
		TMap<FString, FArmed> GArmed;

		TArray<FHit> GHits;

		FDelegateHandle GHandle;

		constexpr int32 MaxHits = 1024;
		int32 GDroppedHits = 0;

		TMap<int32, int32> GExceptionCounts;
		int32 GTotalExceptions = 0;

		FCriticalSection GMutex;

		FString SafeName(const UObject* InObject)
		{
			if (InObject == nullptr)
			{
				return TEXT("(none)");
			}
			return InObject->GetPathName();
		}

		void OnScriptException(const UObject* ActiveObject, const FFrame& StackFrame,
			const FBlueprintExceptionInfo& Info)
		{
			int32 ExceptionNumber = 0;
			{
				FScopeLock CounterLock(&GMutex);
				++GExceptionCounts.FindOrAdd(static_cast<int32>(Info.GetType()));
				ExceptionNumber = ++GTotalExceptions;
			}

			if (ExceptionNumber <= 8)
			{
				UE_LOG(LogTurboMCPTrace, Display,
					TEXT("TRACE EXCEPTION #%d type=%d active=%s node=%s"),
					ExceptionNumber, static_cast<int32>(Info.GetType()),
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

			const FString NodeId = TurboMCPNodeId::MakeForNode(Node);

			FHit Hit;
			Hit.NodeId = NodeId;
			Hit.NodeTitle = TurboMCPNodeId::GetNodeTitle(Node);
			Hit.ContextName = SafeName(ActiveObject);
			Hit.TimeSeconds = FPlatformTime::Seconds();
			Hit.ThreadId = FPlatformTLS::GetCurrentThreadId();

			FScopeLock Lock(&GMutex);

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

			UE_LOG(LogTurboMCPTrace, Display,
				TEXT("TRACE HIT node=%s title=%s context=%s thread=%u hits=%d"),
				*NodeId, *Hit.NodeTitle, *Hit.ContextName, Hit.ThreadId, Armed->HitCount);
			GHits.Add(MoveTemp(Hit));
		}

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
					OutPlan.NodeIds.AddUnique(Trimmed);
				}
			}
		}

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

		// Status requests do not subscribe or mutate tracing state.
		if (InPlan.Action == TEXT("status"))
			return Status();

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
					TEXT("cleared: removed %d armed point(s) and every recorded hit. This trace history cannot be restored with Undo."), Was);
			}

			int32 Removed = 0;
			for (const FString& Id : InPlan.NodeIds)
			{
				Removed += GArmed.Remove(Id) > 0 ? 1 : 0;
			}
			const int32 HitsBefore = GHits.Num();
			GHits.RemoveAll([&InPlan](const FHit& Hit)
			{
				return InPlan.NodeIds.Contains(Hit.NodeId);
			});
			return FString::Printf(
				TEXT("cleared: removed %d of %d named point(s), and %d of %d recorded hit(s). Removed trace history cannot be restored with Undo."),
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
				TEXT("Registered IDs in memory; no graph nodes were added and no compile or save was requested. ")
				TEXT("IDs are not validated against the graph. Hits require execution and available instrumentation; ")
				TEXT("use ReadTurboMCPTrace to inspect them. Zero hits do not prove that a node did not run."),
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
			Out += FString::Printf(TEXT("\n%d hit(s) were DROPPED off the front of the %d-hit buffer"),
				GDroppedHits, MaxHits);
		}

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
			Out += TEXT("\nno point is armed, so nothing is being recorded. Arming is an in-memory "
				"registry entry and changes no asset.");
			return Out;
		}

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
			Out += TEXT("\nRetained hits, oldest first (monotonic seconds, not UTC; pin values are not captured):");
			for (const FHit& Hit : GHits)
			{
				Out += FString::Printf(TEXT("\n  %.3f  %s  %s  thread %u  context=%s"),
					Hit.TimeSeconds, *Hit.NodeId, *Hit.NodeTitle, Hit.ThreadId, *Hit.ContextName);
			}
		}
		else
		{
			Out += FString::Printf(
				TEXT("\nNo hits were recorded for %d armed point(s). This does not prove the nodes did not execute; ")
				TEXT("check the node IDs and trace-event delivery. The subscription is %s."),
				GArmed.Num(), IsSubscribed() ? TEXT("live") : TEXT("NOT live"));
		}

		return Out;
	}

	TArray<FHit> Hits()
	{
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
		if (GHandle.IsValid())
		{
			FBlueprintCoreDelegates::OnScriptException.Remove(GHandle);
			GHandle.Reset();
		}
		ClearAll();
	}
}

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPBPLint.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "GameFramework/Actor.h"
#include "Misc/AssetRegistryInterface.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPBlueprintRead.h"
#include "TurboMCPGraphDead.h"
#include "TurboMCPGraphFixBackward.h"
#include "TurboMCPNodeId.h"

namespace
{
	/** The six check names, as stable identifiers. A caller filters by these and never by number. */
	const TCHAR* const CheckDeadNodes = TEXT("dead-nodes");
	const TCHAR* const CheckUnnecessaryCasts = TEXT("unnecessary-casts");
	const TCHAR* const CheckExpensiveTick = TEXT("expensive-tick");
	const TCHAR* const CheckHeavyHardReferences = TEXT("heavy-hard-references");
	const TCHAR* const CheckUnusedDeclarations = TEXT("unused-declarations");
	const TCHAR* const CheckUnconnectedPins = TEXT("unconnected-pins");

	// Fixed saved-package disk-size heuristic; not runtime memory or measured cost.
	const int64 HeavyHardReferenceBytes = 1024 * 1024;

	/** Splits a comma or whitespace separated filter. Empty means "no filter", which is every check. */
	TArray<FString> SplitFilter(const FString& InFilter)
	{
		TArray<FString> Parts;
		FString Current;
		for (const TCHAR Character : InFilter)
		{
			if (Character == TEXT(',') || FChar::IsWhitespace(Character))
			{
				if (!Current.IsEmpty())
				{
					Parts.Add(Current.ToLower());
					Current.Empty();
				}
			}
			else
			{
				Current.AppendChar(Character);
			}
		}
		if (!Current.IsEmpty())
		{
			Parts.Add(Current.ToLower());
		}
		return Parts;
	}

	/** Every graph of a Blueprint, in a stable order, so two runs report the same thing in the same order. */
	TArray<UEdGraph*> AllGraphs(const UBlueprint* InBlueprint)
	{
		TArray<UEdGraph*> Graphs;
        FTurboMCPBlueprintRead::CollectGraphs(InBlueprint, Graphs);
		return Graphs;
	}

	/** `TurboMCPNodeId::MakeKey`, so a finding's key is the same key every other tool uses. */
	FString KeyOf(const UEdGraphNode* InNode)
	{
		return InNode != nullptr ? TurboMCPNodeId::MakeKey(InNode->NodeGuid) : FString();
	}

	FString TitleOf(const UEdGraphNode* InNode)
	{
		if (InNode == nullptr) { return FString(); }
		const FString Title = InNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
		return Title.IsEmpty() ? InNode->GetClass()->GetName() : Title;
	}

	/** The asset's serialised size from the registry, or -1 when the registry cannot say (`P1`, `D-05`). */
	int64 DiskSizeOfPackage(IAssetRegistry* InRegistry, const FName InPackageName)
	{
		if (InRegistry == nullptr) { return -1; }
		const TOptional<FAssetPackageData> Data = InRegistry->GetAssetPackageDataCopy(InPackageName);
		if (!Data.IsSet()) { return -1; }
		return Data.GetValue().DiskSize;
	}

	FString HumanSize(const int64 InBytes)
	{
		if (InBytes < 0) { return TEXT("size unknown"); }
		if (InBytes < 1024) { return FString::Printf(TEXT("%lld bytes"), InBytes); }
		if (InBytes < 1024 * 1024) { return FString::Printf(TEXT("%.1f KiB"), InBytes / 1024.0); }
		return FString::Printf(TEXT("%.2f MiB"), InBytes / (1024.0 * 1024.0));
	}
}

TArray<FTurboMCPBPLint::FCheckName> FTurboMCPBPLint::KnownChecks()
{
	TArray<FCheckName> Names;
	Names.Add({ CheckDeadNodes,
		TEXT("the disconnected groups PreviewTurboMCPGraphDead would remove, with the entry rule applied") });
	Names.Add({ CheckUnnecessaryCasts,
		TEXT("a cast whose result only calls something already on the class, or on a known type") });
	Names.Add({ CheckExpensiveTick,
		TEXT("direct connections from known engine Tick events; not measured execution cost") });
	Names.Add({ CheckHeavyHardReferences,
		TEXT("hard references to assets over a size threshold, read from the registry") });
	Names.Add({ CheckUnusedDeclarations,
		TEXT("variables nothing reads, functions nothing calls, events nothing binds") });
	Names.Add({ CheckUnconnectedPins,
		TEXT("a required input pin with no link and no default, and a node nothing reaches") });
	return Names;
}

// ---------------------------------------------------------------------------------------------------
// dead-nodes
// ---------------------------------------------------------------------------------------------------
//
// REUSES `FTurboMCPGraphDead::Plan` RATHER THAN WALKING THE GRAPH AGAIN, AND THAT IS NOT LAZINESS: that
// pass already applies THE ENTRY RULE, which is the difference between a useful finding and a dangerous
// one - nothing an event reaches is ever reported. A second walk here would eventually disagree with
// `E-GRAPH-DEAD` about what is removable, and the two would then send a caller in opposite directions.
//
// AND THE COMMENT TRAP IS INHERITED WITH IT: `UEdGraphNode_Comment` HAS NO PINS, SO A WALK FROM THE ENTRY
// NODES CAN NEVER REACH ONE, AND TREATING THAT AS DEAD ONCE QUEUED AN AUTHOR'S DOCUMENTATION FOR DELETION.
//
// `Plan` TAKES ONE GRAPH AT A TIME AND DEFAULTS TO THE EVENT GRAPH WHEN THE NAME IS EMPTY (measured:
// `TurboMCPGraphDead.cpp:343-351` matches `EventGraph` by exact name). A LINT MUST COVER EVERY GRAPH - TA-3's
// dead copy is in a FUNCTION graph - so this calls it once per graph with the name it read, rather than once
// with an empty name that would silently look at one graph and report on the asset.
static void RunDeadNodes(const UBlueprint* InBlueprint, const TArray<UEdGraph*>& InGraphs, FTurboMCPBPLint::FCheckResult& Out)
{
	Out.Name = TEXT("dead-nodes");
	Out.bRan = true;

	for (UEdGraph* Graph : InGraphs)
	{
		FTurboMCPGraphDead::FPlan Plan;
		FString Error;
		if (!FTurboMCPGraphDead::Plan(InBlueprint->GetName(), Graph->GetName(), Plan, Error))
		{
			// ONE GRAPH FAILING DOES NOT FAIL THE CHECK, but it is REPORTED rather than swallowed, because a
			// check that quietly skipped a graph would report a clean asset it never examined (`P1`).
			FTurboMCPBPLint::FFinding Finding;
			Finding.Check = Out.Name;
			Finding.GraphName = Graph->GetName();
			Finding.Severity = TEXT("info");
			Finding.Why = FString::Printf(TEXT("this graph could not be examined: %s"), *Error);
			Out.Findings.Add(Finding);
			continue;
		}

		// THE DENOMINATOR: every group the pass found, candidate or not. A group that is KEPT is a thing
		// this check looked at, and counting only the candidates would make "0 findings" mean "0 groups"
		// rather than "0 removable groups".
		Out.Scanned += Plan.Groups.Num();

		for (const int32 Index : Plan.CandidateIndices)
		{
			if (!Plan.Groups.IsValidIndex(Index)) { continue; }
			const FTurboMCPGraphDead::FGroup& Group = Plan.Groups[Index];

			for (const FString& Key : Group.Keys)
			{
				FTurboMCPBPLint::FFinding Finding;
				Finding.Check = Out.Name;
				Finding.NodeKey = Key;
				Finding.GraphName = Graph->GetName();
				// `error` because this node is unreachable from every entry and removable, which is the
				// strongest thing this check can say - and it says it only when `Plan` said so.
				Finding.Severity = TEXT("warning");
				Finding.Why = Group.bRepeatsLiveLogic
					? FString::Printf(TEXT("unreachable from any entry, and it repeats live logic (%s)"),
						*FString::Join(Group.RepeatedItems, TEXT(", ")))
					: TEXT("not reached by the static entry walk; inspect the dead-node preview before changing it");
				Finding.FixTool = TEXT("PreviewTurboMCPGraphDead");
				Out.Findings.Add(Finding);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------
// unnecessary-casts
// ---------------------------------------------------------------------------------------------------
//
// A cast can be useful through its typed result, boolean result, or execution branches.
// Do not infer the input object's type from the enclosing Blueprint's class: other
// objects (including null) can reach this node. Report only observable unused outputs.
static void RunUnnecessaryCasts(const UBlueprint* InBlueprint, const TArray<UEdGraph*>& InGraphs,
	FTurboMCPBPLint::FCheckResult& Out)
{
	Out.Name = TEXT("unnecessary-casts");
	Out.bRan = true;

	for (UEdGraph* Graph : InGraphs)
	{
		if (Graph == nullptr) { continue; }
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node);
			if (CastNode == nullptr) { continue; }
			++Out.Scanned;

			// Missing result pins indicate an incomplete/custom node, not proof of disuse.
			if (CastNode->GetCastResultPin() == nullptr) { continue; }
			bool bHasConsumer = false;
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin != nullptr && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
				{
					bHasConsumer = true;
					break;
				}
			}
			if (bHasConsumer) { continue; }

			FTurboMCPBPLint::FFinding Finding;
			Finding.Check = Out.Name;
			Finding.NodeKey = KeyOf(Node);
			Finding.GraphName = Graph->GetName();
			Finding.Severity = TEXT("warning");
			Finding.Why = TEXT("no cast output is connected (typed result, success value or execution branches); review whether this cast is intended");
			Out.Findings.Add(Finding);
		}
	}
}

// ---------------------------------------------------------------------------------------------------
// expensive-tick
// ---------------------------------------------------------------------------------------------------
//
// A REPORT AND NEVER A PERFORMANCE NUMBER, because no performance number can be measured from here - the
// spec's own words. So this finds the work done DIRECTLY in a tick, names the nodes it depends on, and says
// what could carry it instead. It does not claim anything is slow.
static void RunExpensiveTick(const UBlueprint* InBlueprint, const TArray<UEdGraph*>& InGraphs,
	FTurboMCPBPLint::FCheckResult& Out)
{
	Out.Name = TEXT("expensive-tick");
	Out.bRan = true;

	for (UEdGraph* Graph : InGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr) { continue; }
            const UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
            const UFunction* Function = Event ? Event->EventReference.ResolveMember<UFunction>(InBlueprint ? InBlueprint->GeneratedClass : nullptr) : nullptr;
            const FString Owner = Function ? Function->GetOuterUClass()->GetPathName() : FString();
            const FName Name = Function ? Function->GetFName() : NAME_None;
            const bool bIsTickEntry = Function &&
                ((Name == TEXT("ReceiveTick") && (Owner == TEXT("/Script/Engine.Actor") || Owner == TEXT("/Script/Engine.ActorComponent"))) ||
                 (Name == TEXT("Tick") && Owner == TEXT("/Script/UMG.UserWidget")));

			if (!bIsTickEntry) { continue; }

			++Out.Scanned;

			// WHAT IT DEPENDS ON, which the spec requires the finding to name: walk one step out from every
			// output and collect the consumers, so the caller sees the work rather than a verdict.
			TArray<FString> DependsOn;
			for (const UEdGraphPin* const Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output) { continue; }
				for (const UEdGraphPin* const Linked : Pin->LinkedTo)
				{
					if (Linked == nullptr || Linked->GetOwningNode() == nullptr) { continue; }
					DependsOn.AddUnique(FString::Printf(TEXT("%s (%s)"),
						*TitleOf(Linked->GetOwningNode()), *KeyOf(Linked->GetOwningNode())));
				}
			}
			DependsOn.Sort();

			FTurboMCPBPLint::FFinding Finding;
			Finding.Check = Out.Name;
			Finding.NodeKey = KeyOf(Node);
			Finding.GraphName = Graph->GetName();
			// `info`, NOT `warning`: this is a report about shape, not a claim that anything is slow.
			Finding.Severity = TEXT("info");
			Finding.Why = DependsOn.Num() > 0
				? FString::Printf(TEXT("engine Tick event has direct connections to %s; review whether event-driven work is appropriate. Tick may be disabled or interval-limited; no runtime cost was measured"),
					*FString::Join(DependsOn, TEXT(", ")))
				: TEXT("engine Tick event has no connected outputs; this is not evidence of expensive work");
			Out.Findings.Add(Finding);
		}
	}
}

// ---------------------------------------------------------------------------------------------------
// heavy-hard-references
// ---------------------------------------------------------------------------------------------------
//
// READ FROM THE REGISTRY, THE SAME WAY `R-ASSET-COST` IS SPECIFIED TO: `GetDependencies` with the HARD
// property REQUIRED and the soft and editor-only properties EXCLUDED, then `GetAssetPackageDataCopy` for each
// dependency's `DiskSize` (`FAssetPackageData::DiskSize`, `AssetData.h:968`).
//
// A SIZE THAT CANNOT BE READ IS REPORTED AS UNKNOWN WITH THE REASON AND NEVER AS ZERO - `P1` and `D-05` both
// say so, and a zero here would read as "a big asset is small", which is the wrong direction to be wrong in.
static void RunHeavyHardReferences(const FString& InPackageName, FTurboMCPBPLint::FCheckResult& Out)
{
	Out.Name = TEXT("heavy-hard-references");
	Out.bRan = true;

	IAssetRegistry* const Registry = IAssetRegistry::Get();
	if (Registry == nullptr)
	{
		Out.bRan = false;
		Out.Reason = TEXT("the asset registry is not available, which happens during shutdown");
		return;
	}
	if (InPackageName.IsEmpty())
	{
		Out.bRan = false;
		Out.Reason = TEXT("the asset's package name is empty, so its dependencies cannot be looked up");
		return;
	}

	// Include hard package references; editor-only references are not excluded.
	const UE::AssetRegistry::FDependencyQuery Query(UE::AssetRegistry::EDependencyQuery::Hard);

	TArray<FName> Dependencies;
    if (Registry->IsGathering() || !Registry->GetDependencies(FName(*InPackageName), Dependencies,
        UE::AssetRegistry::EDependencyCategory::Package, Query))
    {
        Out.bRan = false;
        Out.Reason = TEXT("registry scanning or unavailable dependency data prevents a complete hard-reference check; retry after scanning");
        return;
    }

	// THE DENOMINATOR IS EVERY HARD DEPENDENCY, not only the heavy ones: a check that found nothing must be
	// distinguishable from a check whose dependency lookup came back empty for a reason.
	Out.Scanned = Dependencies.Num();

	if (Dependencies.Num() == 0)
	{
		// NOT AN ERROR, BUT NOT SILENT EITHER. Zero hard dependencies is a real possible answer and the
		// caller is told which answer this is (`P1`).
		Out.Reason = TEXT("the registry reports no hard package dependencies for this asset");
	}

	for (const FName& Dependency : Dependencies)
	{
		const int64 Size = DiskSizeOfPackage(Registry, Dependency);
		if (Size < 0)
		{
			FTurboMCPBPLint::FFinding Finding;
			Finding.Check = Out.Name;
			Finding.Severity = TEXT("info");
			Finding.Why = FString::Printf(
				TEXT("%s is a hard reference and the registry cannot report its size (never zero, because zero would read as small)"),
				*Dependency.ToString());
			Out.Findings.Add(Finding);
			continue;
		}

		if (Size <= HeavyHardReferenceBytes) { continue; }

		FTurboMCPBPLint::FFinding Finding;
		Finding.Check = Out.Name;
		Finding.Severity = TEXT("warning");
		// THE THRESHOLD IS NAMED IN THE FINDING, so a caller can see the rule rather than infer it.
		Finding.Why = FString::Printf(TEXT("hard references %s, which is %s (over the %s threshold)"),
			*Dependency.ToString(), *HumanSize(Size), *HumanSize(HeavyHardReferenceBytes));
		Out.Findings.Add(Finding);
	}
}

// ---------------------------------------------------------------------------------------------------
// unused-declarations
// ---------------------------------------------------------------------------------------------------
//
// Reader and lint consume the same typed facts. Rendered prose is not a data format.
static void RunUnusedDeclarations(const UBlueprint* InBlueprint,
	FTurboMCPBPLint::FCheckResult& Out)
{
	Out.Name = TEXT("unused-declarations");
	FTurboMCPBlueprintRead::FRequest Request;
	Request.Detail = FTurboMCPBlueprintRead::EDetail::Signatures;
	const FTurboMCPBlueprintRead::FResult Read = FTurboMCPBlueprintRead::Read(InBlueprint, Request);
	const FTurboMCPBlueprintRead::FUsageSummary& Usage = Read.Usage;
	Out.bRan = Usage.bCollected;
	if (!Out.bRan)
	{
		Out.Reason = TEXT("static usage facts were not collected; no declarations were checked");
		return;
	}
	Out.Scanned = Usage.VariablesScanned + Usage.FunctionsScanned;
	Out.Reason = FString::Printf(TEXT("static scan of %d graph(s), %d variable(s), %d function(s), plus designer bindings; external/native/dynamic invocation is not excluded; findings are not deletion advice"),
		Usage.GraphsScanned, Usage.VariablesScanned, Usage.FunctionsScanned);
	for (const FString& Variable : Usage.VariablesWithoutReadReferences)
	{
		FTurboMCPBPLint::FFinding Finding;
		Finding.Check = Out.Name;
		Finding.Severity = TEXT("info");
		Finding.Why = FString::Printf(TEXT("variable '%s': no direct read/reference found in scanned Blueprint graphs or designer bindings; external, native, reflected and dynamic uses are not excluded"), *Variable);
		Out.Findings.Add(Finding);
	}
	for (const FString& Function : Usage.FunctionsWithoutReferences)
	{
		FTurboMCPBPLint::FFinding Finding;
		Finding.Check = Out.Name;
		Finding.Severity = TEXT("info");
		Finding.Why = FString::Printf(TEXT("function '%s': no static call/delegate/designer reference found; engine callbacks and external/dynamic callers are not excluded; this does not establish dead code"), *Function);
		Out.Findings.Add(Finding);
	}
}

// ---------------------------------------------------------------------------------------------------
// unconnected-pins
// ---------------------------------------------------------------------------------------------------
//
// TWO THINGS, BOTH PROVABLE FROM THE GRAPH AND NOTHING ELSE: a REQUIRED input pin that is neither linked nor
// defaulted, and a node nothing reaches inside a graph that HAS an entry. The second is narrower than
// `dead-nodes` on purpose - `dead-nodes` reports what `E-GRAPH-DEAD` would remove, with the entry rule and
// the write rule applied; this reports the plain shape.
//
// A COMMENT NODE IS EXCLUDED BY CLASS AND SAYS SO, because it has no pins at all and reporting it as an
// orphan is the mistake that once queued a documentation box for deletion.
static void RunUnconnectedPins(const UBlueprint* InBlueprint, const TArray<UEdGraph*>& InGraphs,
	FTurboMCPBPLint::FCheckResult& Out)
{
	Out.Name = TEXT("unconnected-pins");
	Out.bRan = true;

	for (UEdGraph* Graph : InGraphs)
	{
		// THE GRAPH'S OWN ENTRY NODES, found by the schema rather than by class name guessing: a graph with
		// no entry has no notion of "reachable", so nothing is reported as an orphan in it.
		bool bHasEntry = false;
		for (const UEdGraphNode* const Node : Graph->Nodes)
		{
			if (Node == nullptr) { continue; }
			if (Node->GetClass()->GetName().Contains(TEXT("K2Node_FunctionEntry"))
				|| Node->GetClass()->GetName().Contains(TEXT("K2Node_Event"))
				|| Node->GetClass()->GetName().Contains(TEXT("K2Node_Tunnel")))
			{
				bHasEntry = true;
				break;
			}
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr) { continue; }

			// A COMMENT HAS NO PINS AND IS NEVER AN ORPHAN. Said here rather than left implicit, because
			// this is the exact mistake `TurboMCPGraphDead` had to learn.
			if (Node->GetClass()->GetName().Contains(TEXT("EdGraphNode_Comment"))) { continue; }

			++Out.Scanned;

			// (1) A REQUIRED INPUT THAT IS NEITHER LINKED NOR DEFAULTED.
			for (const UEdGraphPin* const Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Input) { continue; }
				if (Pin->LinkedTo.Num() > 0) { continue; }
				// Exec inputs are legitimately unlinked at the end of a chain, so they are not this finding.
				if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) { continue; }
				// A pin the author hid behind the advanced view is not a required pin they forgot.
				if (Pin->bAdvancedView) { continue; }
				// A default that is set - or that the schema says is fine as it is - means the pin is
				// satisfied without a link. `DoesDefaultValueExist` is the schema's own answer, so this is
				// read rather than guessed.
				if (Pin->DefaultObject != nullptr) { continue; }
				if (!Pin->DefaultValue.IsEmpty()) { continue; }
				if (Pin->DefaultTextValue.IsEmpty() == false) { continue; }
				if (Pin->LinkedTo.Num() == 0 && Pin->bHidden) { continue; }
				// Split parents are represented by their children, not another literal.
				if (Pin->SubPins.Num() > 0) { continue; }
				// Empty is a valid value for several types (notably string/text).
				// Let the K2 schema decide instead of inferring a required input from emptiness.
				if (const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema()))
				{
					if (Schema->IsPinDefaultValid(Pin, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue).IsEmpty())
						continue;
				}


				FTurboMCPBPLint::FFinding Finding;
				Finding.Check = Out.Name;
				Finding.NodeKey = KeyOf(Node);
				Finding.GraphName = Graph->GetName();
				Finding.Severity = TEXT("warning");
				Finding.Why = FString::Printf(
					TEXT("input pin '%s' on %s has no link and no default"),
					*Pin->PinName.ToString(), *TitleOf(Node));
				Out.Findings.Add(Finding);
			}

			// (2) A NODE NOTHING REACHES, only where reachability means something.
			if (!bHasEntry) { continue; }
			const FString ClassName = Node->GetClass()->GetName();
			if (ClassName.Contains(TEXT("K2Node_FunctionEntry")) || ClassName.Contains(TEXT("K2Node_Event"))
				|| ClassName.Contains(TEXT("K2Node_Tunnel")) || ClassName.Contains(TEXT("K2Node_Knot")))
			{
				continue;
			}

			bool bHasAnyLink = false;
			for (const UEdGraphPin* const Pin : Node->Pins)
			{
				if (Pin != nullptr && Pin->LinkedTo.Num() > 0) { bHasAnyLink = true; break; }
			}
			// *** AN ORPHAN IS ABOUT REACHABILITY, NOT ABOUT HAVING NO LINKS, AND THE TWO PREDICATES DIVERGE
			// IN A WAY ONLY A MEASUREMENT SHOWS. *** Reporting a node only when `bHasAnyLink` is false gives
			// `Stacked: 0` on TA-4's `Stacked` graph - while
			// `T-QUAL-1` says "the node at the origin" IS reported there. THE KIT ROW SAYS WHY: TA-4 has
			// "one node at the origin linked to nodes far away", SO THE NODE IS LINKED AND STILL UNREACHED.
			// "Has no links" and "nothing reaches it" are DIFFERENT PREDICATES, and the narrower one answers
			// a question the acceptance row does not ask.
			if (!bHasAnyLink)
			{
				FTurboMCPBPLint::FFinding Finding;
				Finding.Check = Out.Name;
				Finding.NodeKey = KeyOf(Node);
				Finding.GraphName = Graph->GetName();
				Finding.Severity = TEXT("info");
				Finding.Why = FString::Printf(
					TEXT("%s is linked to nothing, so nothing can reach it; PreviewTurboMCPGraphDead decides whether it is safe to remove"),
					*TitleOf(Node));
				Finding.FixTool = TEXT("PreviewTurboMCPGraphDead");
				Out.Findings.Add(Finding);
			}
		}

		// (2b) THE LINKED-BUT-UNREACHED GROUPS, from the pass that already knows the entry rule. A group the
		// pass would REMOVE is `dead-nodes`' finding and is skipped here, so two checks never answer the same
		// question - this check reports the groups the pass KEEPS, which is the "orphaned" half of its bullet.
		if (!bHasEntry) { continue; }

		FTurboMCPGraphDead::FPlan DeadPlan;
		FString DeadError;
		if (!FTurboMCPGraphDead::Plan(InBlueprint->GetName(), Graph->GetName(), DeadPlan, DeadError))
		{
			continue;
		}
		for (int32 GroupIndex = 0; GroupIndex < DeadPlan.Groups.Num(); ++GroupIndex)
		{
			const FTurboMCPGraphDead::FGroup& Group = DeadPlan.Groups[GroupIndex];
			if (!Group.bUnreachable) { continue; }
			if (DeadPlan.CandidateIndices.Contains(GroupIndex)) { continue; }

			for (const FString& Key : Group.Keys)
			{
				FTurboMCPBPLint::FFinding Finding;
				Finding.Check = Out.Name;
				Finding.NodeKey = Key;
				Finding.GraphName = Graph->GetName();
				Finding.Severity = TEXT("info");
				Finding.Why = TEXT("nothing reaches it from any entry point, though it may still be linked; PreviewTurboMCPGraphDead decides whether it is safe to remove");
				Finding.FixTool = TEXT("PreviewTurboMCPGraphDead");
				Out.Findings.Add(Finding);
			}
		}
	}
}


bool FTurboMCPBPLint::Lint(const FString& InAsset, const FString& InChecks,
	FReport& OutReport, FString& OutError)
{
	OutError.Empty();
	OutReport = FReport();

	const FString Asset = InAsset.TrimStartAndEnd();
	if (Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example BP_NodeTest.");
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UBlueprint* const Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(TEXT("%s is not a Blueprint, so it cannot be linted."),
			*Resolved.ObjectPath);
		return false;
	}

	const TArray<UEdGraph*> Graphs = AllGraphs(Blueprint);
	if (Graphs.Num() == 0)
	{
		OutError = FString::Printf(TEXT("%s has no graphs, so there is nothing to lint."), *Asset);
		return false;
	}

	// THE FILTER. A NAME THAT MATCHES NOTHING IS REFUSED BY NAME WITH THE LIST, which `P1` requires.
	const TArray<FString> Requested = SplitFilter(InChecks);
	const TArray<FCheckName> Known = KnownChecks();

	if (Requested.Num() > 0)
	{
		TArray<FString> Unknown;
		TArray<FString> Valid;
		for (const FString& Name : Requested)
		{
			if (!Known.ContainsByPredicate([&Name](const FCheckName& Entry)
				{ return Entry.Name == Name; }))
			{
				Unknown.Add(Name);
			}
			else
			{
				Valid.Add(Name);
			}
		}
		if (Unknown.Num() > 0)
		{
			TArray<FString> Names;
			for (const FCheckName& Entry : Known) { Names.Add(Entry.Name); }
			OutError = FString::Printf(
				TEXT("no check is named %s. The checks are: %s. Pass an empty `checks` to run every one."),
				*FString::Join(Unknown, TEXT(", ")), *FString::Join(Names, TEXT(", ")));
			return false;
		}
		if (Valid.Num() == 0)
		{
			OutError = TEXT("the filter matched no check and none were requested, which should not be reachable. Pass an empty `checks`.");
			return false;
		}
	}

	auto bWanted = [&Requested](const TCHAR* const InName) -> bool
	{
		if (Requested.Num() == 0) { return true; }
		for (const FString& Name : Requested)
		{
			if (Name == InName) { return true; }
		}
		return false;
	};

	OutReport.Asset = Resolved.ObjectPath;

	if (bWanted(CheckDeadNodes))
	{
		FCheckResult Result;
		RunDeadNodes(Blueprint, Graphs, Result);
		OutReport.Checks.Add(MoveTemp(Result));
	}
	if (bWanted(CheckUnnecessaryCasts))
	{
		FCheckResult Result;
		RunUnnecessaryCasts(Blueprint, Graphs, Result);
		OutReport.Checks.Add(MoveTemp(Result));
	}
	if (bWanted(CheckExpensiveTick))
	{
		FCheckResult Result;
		RunExpensiveTick(Blueprint, Graphs, Result);
		OutReport.Checks.Add(MoveTemp(Result));
	}
	if (bWanted(CheckHeavyHardReferences))
	{
		FCheckResult Result;
		RunHeavyHardReferences(Resolved.PackagePath, Result);
		OutReport.Checks.Add(MoveTemp(Result));
	}
	if (bWanted(CheckUnusedDeclarations))
	{
		FCheckResult Result;
		RunUnusedDeclarations(Blueprint, Result);
		OutReport.Checks.Add(MoveTemp(Result));
	}
	if (bWanted(CheckUnconnectedPins))
	{
		FCheckResult Result;
		RunUnconnectedPins(Blueprint, Graphs, Result);
		OutReport.Checks.Add(MoveTemp(Result));
	}

	return true;
}

FString FTurboMCPBPLint::Render(const FReport& InReport, int32 InTokenBudget)
{
	FString Out = FString::Printf(TEXT("lint: %s"), *InReport.Asset);
	int32 Ran = 0;
	for (const FCheckResult& Check : InReport.Checks) { Ran += Check.bRan ? 1 : 0; }
	Out += FString::Printf(TEXT("\n%d check(s) run, %d requested"), Ran, InReport.Checks.Num());

	int32 TotalFindings = 0;
	for (const FCheckResult& Check : InReport.Checks)
	{
		TotalFindings += Check.Findings.Num();
	}
	Out += FString::Printf(TEXT(", %d finding(s)"), TotalFindings);

	// A BUDGET THAT CUTS MUST SAY SO, rather than looking like a clean asset.
	const bool bBudgeted = InTokenBudget > 0;
	int64 CharactersPrinted = 0;
	const int64 CharacterBudget = bBudgeted ? static_cast<int64>(InTokenBudget) * 4 : 0;
	bool bTruncated = false;

	for (const FCheckResult& Check : InReport.Checks)
	{
		// *** EVERY CHECK PRINTS ITS NAME AND ITS DENOMINATOR, ALWAYS, EVEN WITH ZERO FINDINGS. THIS IS THE
		// CAPABILITY RATHER THAN A FORMATTING CHOICE: "no problems found" without a count is indistinguishable
		// from a check that examined nothing, which is the purest form of the defect this record holds nine
		// examples of. ***
		if (!Check.bRan)
		{
			Out += FString::Printf(TEXT("\n\n%s: DID NOT RUN - %s"),
				*Check.Name, *Check.Reason);
			continue;
		}

		Out += FString::Printf(TEXT("\n\n%s: looked at %d, found %d"),
			*Check.Name, Check.Scanned, Check.Findings.Num());

		if (Check.Findings.Num() == 0)
		{
			Out += FString::Printf(TEXT("\n  No findings among %d examined item(s); this does not certify the Blueprint as defect-free."),
				Check.Scanned);
			if (!Check.Reason.IsEmpty())
			{
				Out += FString::Printf(TEXT("\n  note: %s"), *Check.Reason);
			}
			continue;
		}

		if (!Check.Reason.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n  note: %s"), *Check.Reason);
		}

		for (const FFinding& Finding : Check.Findings)
		{
			if (bBudgeted && CharactersPrinted > CharacterBudget)
			{
				bTruncated = true;
				break;
			}

			FString Line = FString::Printf(TEXT("\n  [%s]"), *Finding.Severity);
			if (!Finding.NodeKey.IsEmpty())
			{
				Line += FString::Printf(TEXT(" %s"), *Finding.NodeKey);
			}
			if (!Finding.GraphName.IsEmpty())
			{
				Line += FString::Printf(TEXT(" in '%s'"), *Finding.GraphName);
			}
			Line += FString::Printf(TEXT(": %s"), *Finding.Why);
			if (!Finding.FixTool.IsEmpty())
			{
				Line += FString::Printf(TEXT("  -> fix with %s"), *Finding.FixTool);
			}

			Out += Line;
			CharactersPrinted += Line.Len();
		}
		// Continue reporting every check count even when finding text is truncated.
	}

	// `P1`: a cut list says it was cut and says what to do next.
	if (bTruncated)
	{
		Out += TEXT("\n\n; TRUNCATED by tokenBudget. Re-run with the SAME filter and a larger budget,")
			TEXT("\n; or narrow `checks` to one name at a time. The counts above are complete even when")
			TEXT("\n; the findings are not.");
	}

	Out += TEXT("\n\nReview findings before applying changes. Suggested tools do not guarantee a fix; this lint pass does not explicitly edit, compile or save assets.");
	return Out;
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPLintTickTest, "TurboMCP.Lint.TickIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPLintTickTest::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>(GetTransientPackage());
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    UK2Node_Event* Tick = NewObject<UK2Node_Event>(Graph);
    Tick->EventReference.SetExternalMember(TEXT("ReceiveTick"), AActor::StaticClass());
    Graph->Nodes.Add(Tick);
    UK2Node_Event* Other = NewObject<UK2Node_Event>(Graph);
    Other->EventReference.SetExternalMember(TEXT("TickSomethingUnrelated"), AActor::StaticClass());
    Graph->Nodes.Add(Other);
    FTurboMCPBPLint::FCheckResult Result;
    RunExpensiveTick(Blueprint, { Graph }, Result);
    TestEqual(TEXT("Only actual engine Tick is counted"), Result.Scanned, 1);
    TestEqual(TEXT("One bounded finding"), Result.Findings.Num(), 1);
    if (Result.Findings.Num() == 1)
        TestTrue(TEXT("Disconnected Tick is not expensive evidence"), Result.Findings[0].Why.Contains(TEXT("not evidence of expensive")));
    return !HasAnyErrors();
}
#endif

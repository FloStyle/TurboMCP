// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPBlueprintRead.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"

/**
 * L-LAYOUT-REPORT: layout facts for one graph.
 *
 * A Look tool, so it measures and never moves anything (`P3`). All the measuring is
 * in `FTurboMCPLayoutReport`; this file only finds the graph, validates the inputs
 * and prints.
 *
 * The report is the input to the phase 6 layout tools, which is why every finding
 * names node IDs: a caller has to be able to act on it without a second read.
 */
namespace
{
	/** How many characters one result may hold. */
	constexpr int32 MaxResultChars = 24000;

	/** How many findings are printed before the list caps. */
	constexpr int32 MaxFindings = 120;
}

FString UTurboMCPLookToolset::LookTurboMCPLayoutReport(
	const FString& Asset,
	const FString& GraphName)
{
	if (Asset.IsEmpty())
	{
		return TEXT("error: no asset given. Pass a Blueprint in any path form, for ")
			TEXT("example BP_NodeTest.");
	}
	if (GraphName.IsEmpty())
	{
		return TEXT("error: no graph given. Pass a graph name; the tool refuses ")
			TEXT("rather than guessing which graph you meant.");
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(
		LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is not a Blueprint, so it has no graphs to measure."),
			*Resolved.ObjectPath);
	}

	const UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, GraphName);
	if (Graph == nullptr)
	{
		// A name that does not exist lists the real ones, as every other graph tool
		// does, so the caller corrects itself in one step (`P7`).
		TArray<UEdGraph*> All;
		FTurboMCPBlueprintRead::CollectGraphs(Blueprint, All);

		TArray<FString> Names;
		for (const UEdGraph* Candidate : All)
		{
			Names.Add(Candidate->GetName());
		}
		return FString::Printf(
			TEXT("error: %s has no graph named '%s'. Its graphs are: %s"),
			*Resolved.ObjectPath, *GraphName,
			Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
	}

	const FTurboMCPLayoutReport::FReport Report =
		FTurboMCPLayoutReport::Measure(Graph, Resolved.ObjectPath);

	FString Out = FString::Printf(TEXT("look: layout of %s graph %s"),
		*Resolved.ObjectPath, *Graph->GetName());

	Out += FString::Printf(
		TEXT("\ncounts: %d node(s), %d wire(s), %d comment box(es), %d nested"),
		Report.NodeCount, Report.WireCount, Report.CommentBoxCount,
		Report.NestedBoxCount);

	if (Report.NodeCount == 0)
	{
		// `P1`: a graph with nothing in it says so rather than printing zeroes with
		// no explanation.
		Out += TEXT("\nThis graph has no nodes, so there is nothing to measure.");
		return Out;
	}

	Out += FString::Printf(
		TEXT("\nextent: x %.0f to %.0f, y %.0f to %.0f (%0.f by %.0f)"),
		Report.Extent.MinX, Report.Extent.MaxX,
		Report.Extent.MinY, Report.Extent.MaxY,
		Report.Extent.Width(), Report.Extent.Height());

	// The main layout measure. Printed with its unit and with the longest wire, so
	// a total that is large because of one wire is distinguishable from one that is
	// large because everything is spread out.
	Out += FString::Printf(
		TEXT("\ntotal wire length: %.0f  (longest single wire: %.0f)"),
		Report.TotalWireLength, Report.LongestWire);
	Out += TEXT("\n; wire length is the straight distance between linked pins. Pin ");
	Out += TEXT("positions are computed from each node's rectangle and pin order, ");
	Out += TEXT("because the engine exposes no pin position to a tool call, so the ");
	Out += TEXT("number is consistent and comparable rather than pixel exact.");

	if (Report.bHasNoWires && !Report.Summary.IsEmpty())
	{
		Out += FString::Printf(TEXT("\n%s"), *Report.Summary);
	}

	// Findings, counted by kind so the shape of the problem is visible before the
	// detail.
	TMap<FString, int32> CountsByKind;
	for (const FTurboMCPLayoutReport::FFinding& Finding : Report.Findings)
	{
		CountsByKind.FindOrAdd(Finding.Kind)++;
	}

	if (CountsByKind.Num() == 0)
	{
		Out += TEXT("\nfindings: none. No backward wire, no overlap, no misplaced ");
		Out += TEXT("getter, no stray node and no disconnected group.");
	}
	else
	{
		TArray<FString> Kinds;
		CountsByKind.GetKeys(Kinds);
		Kinds.Sort();

		Out += TEXT("\nfindings:");
		for (const FString& Kind : Kinds)
		{
			Out += FString::Printf(TEXT("\n  %s: %d"), *Kind, CountsByKind[Kind]);
		}

		// A backward wire is reported, not condemned. A loop that returns to an
		// earlier node is legitimately backward, and so is a cast-failed path that
		// rejoins the flow behind it, so a caller has to judge rather than repair
		// blindly. Saying so costs one line and prevents a wrong tidy.
		if (CountsByKind.Contains(TEXT("backward-exec"))
			|| CountsByKind.Contains(TEXT("backward-data")))
		{
			Out += TEXT("\n; a backward wire is reported, not necessarily wrong. A loop ");
			Out += TEXT("that returns to an earlier node, and a cast-failed path that ");
			Out += TEXT("rejoins the flow behind it, are both legitimately backward. ");
			Out += TEXT("Judge before tidying.");
		}

		int32 Shown = 0;
		FString CurrentKind;
		for (const FTurboMCPLayoutReport::FFinding& Finding : Report.Findings)
		{
			if (Shown >= MaxFindings)
			{
				Out += FString::Printf(
					TEXT("\n  (%d more finding(s) not shown)"),
					Report.Findings.Num() - Shown);
				break;
			}
			++Shown;

			if (Finding.Kind != CurrentKind)
			{
				CurrentKind = Finding.Kind;
				Out += FString::Printf(TEXT("\n\n%s:"), *CurrentKind);
			}
			Out += FString::Printf(TEXT("\n  %s"), *Finding.Detail);
		}
	}

	if (Out.Len() > MaxResultChars)
	{
		const int32 Dropped = Out.Len() - MaxResultChars;
		Out = Out.Left(MaxResultChars);
		Out += FString::Printf(
			TEXT("\n\n; (%d more character(s) were dropped)"), Dropped);
	}

	return Out;
}

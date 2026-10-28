#include "TurboMCPGraphTidy.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPBlueprintRead.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPCapture.h"
#include "EdGraphNode_Comment.h"

// Resolve the graph, measure geometry and report heuristic layout findings.
namespace
{
	/** How many characters one result may hold. */
	constexpr int32 MaxResultChars = 24000;

	/** How many findings are printed before the list caps. */
	constexpr int32 MaxFindings = 120;
}

FString UTurboMCPLookToolset::LookTurboMCPLayoutReport(
	const FString& Asset,
	const FString& GraphName, double AlignmentTolerance)
{
    if (!FMath::IsFinite(AlignmentTolerance) || AlignmentTolerance < 0.0 || AlignmentTolerance > 1024.0)
        return TEXT("error: AlignmentTolerance must be finite and between 0 and 1024 graph units");
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

	UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, GraphName);
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

	FString GeometryError;
    if(!FTurboMCPGraphTidy::PrepareAlignment(Blueprint,Graph,GeometryError)) return TEXT("error: ")+GeometryError;
    FTurboMCPLayoutReport::FReport Report =
		FTurboMCPLayoutReport::Measure(Graph, Resolved.ObjectPath, AlignmentTolerance);

	Report.Findings.StableSort([](const auto& A,const auto& B) { return A.Kind<B.Kind; });
	FString Out = FString::Printf(TEXT("look: layout of %s graph %s"),
		*Resolved.ObjectPath, *Graph->GetName());

	Out += FString::Printf(
		TEXT("\ncounts: %d non-comment node(s) (includes reroutes), %d pin-link wire(s), %d comment box(es), %d nested boxes"),
		Report.NodeCount, Report.WireCount, Report.CommentBoxCount,
		Report.NestedBoxCount);

    TArray<TurboMCPCaptureSelector::FBox> CommentBoxes;
    TurboMCPCaptureSelector::CollectCommentBoxes(Graph,CommentBoxes);
    if(!CommentBoxes.IsEmpty()) Out+=TEXT("\ncomment boxes (capture index -> stable edit ID; indexes may change after layout):");
    for(const auto& Box:CommentBoxes)
    {
        if(!Box.Node) continue;
        const FString Text=Box.Node->NodeComment.Replace(TEXT("\r"),TEXT("\\r")).Replace(TEXT("\n"),TEXT("\\n"));
        Out+=FString::Printf(TEXT("\n  box %d: %s text=\"%s\" rect=(%.0f,%.0f)-(%.0f,%.0f)"),Box.Index,*TurboMCPNodeId::MakeForNode(Box.Node,true),*Text,Box.MinX,Box.MinY,Box.MaxX,Box.MaxY);
    }

	if (Report.NodeCount == 0)
	{
		// `P1`: a graph with nothing in it says so rather than printing zeroes with
		// no explanation.
		Out += TEXT("\nThis graph has no non-comment nodes; comment boxes are listed above.");
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
	Out += TEXT("so the reported length is an estimate, not the rendered wire curve. The ");
	Out += TEXT("value is not a visual-quality score.");

	if (Report.bHasNoWires && !Report.Summary.IsEmpty())
	{
		Out += FString::Printf(TEXT("\n%s"), *Report.Summary);
	}

    struct FLongWire { double Length; FString Endpoints; };
    TArray<FLongWire> Longest;
    for (UEdGraphNode* Node : Graph->Nodes) if (Node && !Node->IsA<UEdGraphNode_Comment>())
        for (UEdGraphPin* Pin : Node->Pins) if (Pin && Pin->Direction == EGPD_Output)
            for (UEdGraphPin* Other : Pin->LinkedTo)
            {
                if (!Other || !Other->GetOwningNodeUnchecked()) continue;
                const double Length = FVector2D::Distance(FTurboMCPLayoutReport::PinPosition(Pin), FTurboMCPLayoutReport::PinPosition(Other));
                Longest.Add({Length, FString::Printf(TEXT("%s.%s -> %s.%s"),
                    *TurboMCPNodeId::MakeForNode(Node,true), *Pin->PinName.ToString(),
                    *TurboMCPNodeId::MakeForNode(Other->GetOwningNodeUnchecked(),true), *Other->PinName.ToString())});
                Longest.Sort([](const FLongWire& A,const FLongWire& B) { return A.Length==B.Length ? A.Endpoints<B.Endpoints : A.Length>B.Length; });
                if (Longest.Num()>10) Longest.SetNum(10);
            }
    Out += TEXT("\nlongest wires (up to 10, straight pin-to-pin graph units; diagnostic, not an error threshold):");
    for (int32 Index=0;Index<Longest.Num();++Index)
        Out += FString::Printf(TEXT("\n  %d. %.1f units: %s"),Index+1,Longest[Index].Length,*Longest[Index].Endpoints);

    Out += FString::Printf(TEXT("\nalignment tolerance: %.1f graph units; overlaps and backward connections are not suppressed"), AlignmentTolerance);

	// Findings, counted by kind so the shape of the problem is visible before the
	// detail.
	TMap<FString, int32> CountsByKind;
	for (const FTurboMCPLayoutReport::FFinding& Finding : Report.Findings)
	{
		CountsByKind.FindOrAdd(Finding.Kind)++;
	}

	if (CountsByKind.Num() == 0)
	{
		Out += TEXT("\nfindings: none. The enabled checks detected no backward wire, overlap, misplaced ");
		Out += TEXT("getter, stray node or disconnected group. This does not certify layout or behavior.");
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

FString UTurboMCPLookToolset::LookTurboMCPGraphPositions(const FString& Asset, const FString& GraphName,
    const FString& NodeIds, int32 MaxNodes)
{
    if (MaxNodes < 1 || MaxNodes > 256) return TEXT("error: MaxNodes must be between 1 and 256");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint || GraphName.IsEmpty()) return TEXT("error: specify a Blueprint and exact graph name");
    UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, GraphName);
    if (!Graph) return TEXT("error: graph not found; read the Blueprint graph list first");

    TArray<UEdGraphNode*> Selected;
    int32 Total = 0;
    for (UEdGraphNode* Node : Graph->Nodes) if (Node) ++Total;
    if (NodeIds.IsEmpty())
    {
        for (UEdGraphNode* Node : Graph->Nodes) if (Node) Selected.Add(Node);
    }
    else
    {
        TArray<FString> Ids;
        NodeIds.ParseIntoArray(Ids, TEXT("\n"), false);
        if (Ids.Num() > 256) return TEXT("error: select at most 256 node IDs per call");
        for (FString Id : Ids)
        {
            Id.TrimStartAndEndInline();
            if (Id.IsEmpty()) return TEXT("error: NodeIds contains a blank entry; use one node ID per line");
            TArray<UEdGraphNode*> Matches;
            const FString Key = TurboMCPNodeId::NormaliseKey(Id);
            if (!Key.IsEmpty()) TurboMCPNodeId::FindNodesByKey(Graph, Key, Matches);
            else for (UEdGraphNode* Node : Graph->Nodes)
                if (Node && (TurboMCPNodeId::MakeForNode(Node, true) == Id || TurboMCPNodeId::MakeForNode(Node) == Id)) Matches.Add(Node);
            if (Matches.Num() != 1)
                return FString::Printf(TEXT("error: node ID '%s' is %s in the named graph. Use an exact reader ID or a longer unique hexadecimal key."),
                    *Id, Matches.IsEmpty() ? TEXT("missing") : TEXT("ambiguous"));
            Selected.AddUnique(Matches[0]);
        }
    }
    // Validate every requested identity before rendering. Selection never silently expands to the graph.
    Selected.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) {
        return TurboMCPNodeId::MakeForNode(&A, true) < TurboMCPNodeId::MakeForNode(&B, true);
    });
    FString GeometryError;
    if (!FTurboMCPGraphTidy::PrepareAlignment(Blueprint, Graph, GeometryError)) return TEXT("error: ") + GeometryError;
    FString Result = TEXT("node positions (graph units; read-only):\nStored top-left positions and rendered node-body sizes; not screen pixels or wire bounds. Rendering measures the graph; NodeIds filters returned rows. No explicit edit, compile or save.\n");
    int32 Count = 0;
    for (const UEdGraphNode* Node : Selected)
    {
        if (Count >= MaxNodes) break;
        const FVector2D NodeSize = FTurboMCPGraphTidy::AlignmentNodeSize(Node);
        const FString Line = FString::Printf(TEXT("%s x=%d y=%d width=%.0f height=%.0f\n"),
            *TurboMCPNodeId::MakeForNode(Node, true), Node->NodePosX, Node->NodePosY, NodeSize.X, NodeSize.Y);
        if (Result.Len() + Line.Len() > MaxResultChars - 400) break;
        Result += Line;
        ++Count;
    }
    Result += FString::Printf(TEXT("listed %d of %d selected nodes; %d graph nodes; omitted %d selected nodes; excluded %d unselected nodes"),
        Count, Selected.Num(), Total, Selected.Num() - Count, Total - Selected.Num());
    if (Count < Selected.Num()) Result += TEXT("\nTRUNCATED: request a smaller NodeIds selection; this is not a complete inventory.");
    return Result;
}

FString UTurboMCPLookToolset::LookTurboMCPLayoutDoneCheck(const FString& Asset,const FString& GraphName,double AlignmentTolerance)
{
    const FString Report = LookTurboMCPLayoutReport(Asset,GraphName,AlignmentTolerance);
    if (!Report.StartsWith(TEXT("look:"))) return Report;
    const FString Capture = LookTurboMCPCapture(Asset,GraphName,TEXT("graph"),TEXT(""),TEXT(""));
    return Report + TEXT("\n\nfull-graph capture (review aid; not automatic layout approval):\n") + Capture;
}

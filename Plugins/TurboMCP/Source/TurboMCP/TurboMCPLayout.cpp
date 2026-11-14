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
#include "TurboMCPReportScope.h"
#include "EdGraphNode_Comment.h"

// Resolve the graph, measure geometry and report heuristic layout findings.
namespace
{
	/** How many characters one result may hold. */
	constexpr int32 MaxResultChars = 24000;

}

FString UTurboMCPLookToolset::LookTurboMCPLayoutReport(
	const FString& Asset,
	const FString& GraphName, double AlignmentTolerance, const FString& Scope, int32 Offset, int32 Limit)
{
    if (Offset < 0 || Limit < 1 || Limit > 120) return TEXT("error: Offset must be nonnegative and Limit must be between 1 and 120");
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

    TurboMCPReportScope::FSelection Selection;
    FString ScopeError;
    if (!TurboMCPReportScope::Resolve(Graph, Scope, Selection, ScopeError)) return TEXT("error: ") + ScopeError;
    TSet<FString> SelectedIds;
    for (const UEdGraphNode* Node : Selection.Nodes)
    { SelectedIds.Add(TurboMCPNodeId::MakeForNode(Node)); SelectedIds.Add(TurboMCPNodeId::MakeForNode(Node, true)); }
    const int32 AllFindings = Report.Findings.Num();
    if (!Selection.bAll) Report.Findings.RemoveAll([&](const auto& Finding) {
        for (const FString& Id : Finding.NodeIds) if (SelectedIds.Contains(Id)) return false;
        return true;
    });
    Report.Findings.StableSort([](const auto& A, const auto& B) {
        if (A.Classification != B.Classification) return A.Classification == TEXT("defect");
        if ((A.Kind == TEXT("overlap")) != (B.Kind == TEXT("overlap"))) return A.Kind == TEXT("overlap");
        if (A.Kind != B.Kind) return A.Kind < B.Kind;
        return A.Detail < B.Detail;
    });
    if (Offset > Report.Findings.Num()) return TEXT("error: Offset exceeds the matched findings; restart with Offset=0 after graph changes");
    int32 DefectCandidates = 0;
    int32 Intended = 0;
    for (const auto& Finding : Report.Findings)
        if (Finding.Classification == TEXT("intended")) ++Intended; else ++DefectCandidates;
    FString Out = FString::Printf(TEXT("look: layout of %s graph %s\nscope: %s\nInclusion: any involved node intersects the scope; outside endpoints remain in findings. Box/region inclusion uses strict rectangle intersection, excluding edge-only contact.\nclassification: %d defect candidates, %d intended handoffs. R3: routed loop Completed below Body; R4: later sequence lane below the first; R6: routed branch False fallback below True. Intended explains this edge only, not nearby geometry. Defect candidates require visual review.\n"),
        *Resolved.ObjectPath, *Graph->GetName(), Scope.IsEmpty() ? TEXT("whole graph") : *Scope, DefectCandidates, Intended);
    Out += FString::Printf(TEXT("whole-graph context: %d node%s, %d wire%s, %d comment box%s; total wire length %.0f, longest %.0f graph units. Extent: x %.0f to %.0f, y %.0f to %.0f. Wire lengths use computed pin positions and straight-line estimates, not rendered curves. Alignment tolerance %.1f graph units.\n"),
        Report.NodeCount, Report.NodeCount == 1 ? TEXT("") : TEXT("s"),
        Report.WireCount, Report.WireCount == 1 ? TEXT("") : TEXT("s"),
        Report.CommentBoxCount, Report.CommentBoxCount == 1 ? TEXT("") : TEXT("es"), Report.TotalWireLength,
        Report.LongestWire, Report.Extent.MinX, Report.Extent.MaxX, Report.Extent.MinY,
        Report.Extent.MaxY, AlignmentTolerance);
    int32 Shown = 0;
    for (int32 I = Offset; I < Report.Findings.Num() && Shown < Limit; ++I)
    {
        const auto& Finding = Report.Findings[I];
        const FString RuleText = Finding.Rule.IsEmpty() ? FString() : FString::Printf(TEXT(" rule=%s"), *Finding.Rule);
        const FString Row = FString::Printf(TEXT("\n%s: class=%s%s %s"), *Finding.Kind,
            *Finding.Classification, *RuleText, *Finding.Detail);
        if (Out.Len() + Row.Len() > MaxResultChars - 3000) break;
        Out += Row; ++Shown;
    }
    if (Report.Findings.IsEmpty()) Out += TEXT("\nfindings: none in this scope. This does not certify layout or behavior.");
    const int32 Next = Offset + Shown;
    const bool bOversizedRecord = Shown == 0 && Next < Report.Findings.Num();
    Out += FString::Printf(TEXT("\nfindings: matched=%d graphTotal=%d offset=%d shown=%d remaining=%d nextOffset=%d\nPages reflect live state; restart at Offset=0 after edits. Findings are diagnostic, not automatic repair instructions."),
        Report.Findings.Num(), AllFindings, Offset, Shown, Report.Findings.Num() - Next, !bOversizedRecord && Next < Report.Findings.Num() ? Next : -1);
    if (bOversizedRecord) Out += FString::Printf(TEXT("\npageBlocked: finding at Offset=%d exceeds the output budget (%d characters). No partial finding was printed. Narrow Scope to inspect relevant nodes. To explicitly skip this unprinted finding and inspect later findings, use Offset=%d. nextOffset=-1 means automatic paging stopped, not that all findings were returned."),
        Next, Report.Findings[Next].Detail.Len(), Next + 1);
    struct FWireDetail { double Length; FString Text; };
    TArray<FWireDetail> Wires;
    for (const UEdGraphNode* Node : Graph->Nodes) if (Node)
        for (const UEdGraphPin* Pin : Node->Pins) if (Pin && Pin->Direction == EGPD_Output)
            for (const UEdGraphPin* Input : Pin->LinkedTo) if (Input && Input->GetOwningNodeUnchecked()
                && (Selection.Includes(Node) || Selection.Includes(Input->GetOwningNodeUnchecked())))
            {
                const double Length = FVector2D::Distance(FTurboMCPGraphTidy::AlignmentPinPosition(Pin), FTurboMCPGraphTidy::AlignmentPinPosition(Input));
                Wires.Add({Length, FString::Printf(TEXT("\n  %.1f units: %s:%s -> %s:%s"), Length,
                    *TurboMCPNodeId::MakeForNode(Node), *Pin->PinName.ToString(),
                    *TurboMCPNodeId::MakeForNode(Input->GetOwningNodeUnchecked()), *Input->PinName.ToString())});
            }
    Wires.Sort([](const FWireDetail& A, const FWireDetail& B) { return A.Length != B.Length ? A.Length > B.Length : A.Text < B.Text; });
    Out += TEXT("\nlongest scoped wires (up to 10; measured straight pin-to-pin distances, not curves):");
    int32 WiresShown = 0;
    for (const auto& Wire : Wires)
    {
        if (WiresShown >= 10 || Out.Len() + Wire.Text.Len() > MaxResultChars - 1000) break;
        Out += Wire.Text; ++WiresShown;
    }
    Out += FString::Printf(TEXT("\nwire context: shown=%d matched=%d"), WiresShown, Wires.Num());
    TArray<TurboMCPCaptureSelector::FBox> Boxes;
    TurboMCPCaptureSelector::CollectCommentBoxes(Graph, Boxes);
    int32 BoxShown = 0, BoxMatched = 0;
    for (const auto& Box : Boxes) if (Box.Node && Selection.Includes(Box.Node))
    {
        ++BoxMatched;
        const FString Row = FString::Printf(TEXT("\nbox %d: %s rect=(%.0f,%.0f)-(%.0f,%.0f)"), Box.Index,
            *TurboMCPNodeId::MakeForNode(Box.Node, true), Box.MinX, Box.MinY, Box.MaxX, Box.MaxY);
        if (Out.Len() + Row.Len() <= MaxResultChars - 500) { Out += Row; ++BoxShown; }
    }
    Out += FString::Printf(TEXT("\nboxes: shown=%d matched=%d omitted=%d. Use ReadTurboMCPCommentList for details. Use a targeted node/box capture for visual review; this report does not capture the whole graph."), BoxShown, BoxMatched, BoxMatched - BoxShown);
    return Out;
}

FString UTurboMCPLookToolset::LookTurboMCPGraphPositions(const FString& Asset, const FString& GraphName,
    const FString& NodeIds, int32 MaxNodes, int32 Offset)
{
    if (Offset < 0) return TEXT("error: Offset must be zero or greater");
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
    if (Offset > Selected.Num()) return FString::Printf(TEXT("error: Offset %d exceeds the %d selected nodes; restart at Offset=0 if the graph changed"), Offset, Selected.Num());
    FString GeometryError;
    if (!FTurboMCPGraphTidy::PrepareAlignment(Blueprint, Graph, GeometryError)) return TEXT("error: ") + GeometryError;
    FString Result = TEXT("node positions (graph units; read-only):\nStored top-left positions and rendered node-body sizes; not screen pixels or wire bounds. Rendering measures the graph; NodeIds filters returned rows. No explicit edit, compile or save.\n");
    int32 Count = 0;
    for (int32 Index = Offset; Index < Selected.Num(); ++Index)
    {
        const UEdGraphNode* Node = Selected[Index];
        if (Count >= MaxNodes) break;
        const FVector2D NodeSize = FTurboMCPGraphTidy::AlignmentNodeSize(Node);
        const FString Line = FString::Printf(TEXT("%s x=%d y=%d width=%.0f height=%.0f\n"),
            *TurboMCPNodeId::MakeForNode(Node, true), Node->NodePosX, Node->NodePosY, NodeSize.X, NodeSize.Y);
        if (Result.Len() + Line.Len() > MaxResultChars - 400) break;
        Result += Line;
        ++Count;
    }
    Result += FString::Printf(TEXT("listed %d of %d selected nodes; %d graph nodes; omitted %d selected nodes; excluded %d unselected nodes"),
        Count, Selected.Num(), Total, Selected.Num() - Offset - Count, Total - Selected.Num());
    const int32 NextOffset = Offset + Count;
    Result += FString::Printf(TEXT("\noffset=%d; next_offset=%d; complete=%s"),
        Offset, NextOffset < Selected.Num() ? NextOffset : -1,
        NextOffset == Selected.Num() ? TEXT("true") : TEXT("false"));
    if (NextOffset < Selected.Num()) Result += TEXT("\nTRUNCATED: continue with Offset=next_offset and the same selection. Restart at Offset=0 after graph changes.");
    else Result += TEXT("\nEnd of selected nodes. Earlier pages are not repeated.");
    return Result;
}

FString UTurboMCPLookToolset::LookTurboMCPLayoutDoneCheck(const FString& Asset,const FString& GraphName,double AlignmentTolerance,const FString& Scope)
{
    const FString Report = LookTurboMCPLayoutReport(Asset,GraphName,AlignmentTolerance,Scope);
    if (!Report.StartsWith(TEXT("look:"))) return Report;

    // The report has already validated the scope. Resolve its target again so
    // the image frames the same node or graph-space rectangle, including boxes
    // identified by reader ID rather than the capture tool's numeric box index.
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!ResolveError.IsEmpty()) return Report + TEXT("\n\ncapture error: ") + ResolveError;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint) return Report + TEXT("\n\ncapture error: The Blueprint became unavailable after measurement. No image captured.");
    UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, GraphName);
    if (!Graph) return Report + TEXT("\n\ncapture error: The graph became unavailable after measurement. No image captured.");
    TurboMCPReportScope::FSelection Selection;
    FString ScopeError;
    if (!TurboMCPReportScope::Resolve(Graph, Scope, Selection, ScopeError))
        return Report + TEXT("\n\ncapture error: ") + ScopeError;
    const FString Capture = LookTurboMCPCapture(Asset,GraphName,Selection.CaptureMode,Selection.CaptureTarget,TEXT(""));
    return Report + FString::Printf(TEXT("\n\n%s capture (review aid; not automatic layout approval):\n"),
        Selection.bAll ? TEXT("full-graph") : TEXT("scoped")) + Capture;
}

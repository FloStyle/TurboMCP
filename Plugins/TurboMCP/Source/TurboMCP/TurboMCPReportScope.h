// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "TurboMCPNodeId.h"
#include "TurboMCPGraphTidy.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "String/LexFromString.h"

// Shared report selection. Never changes graph membership or expands an invalid scope.
namespace TurboMCPReportScope
{
    struct FSelection
    {
        TSet<const UEdGraphNode*> Nodes;
        bool bAll = true;
        // Use the same resolved graph-space target for an optional visual capture.
        FString CaptureMode = TEXT("graph");
        FString CaptureTarget;
        bool Includes(const UEdGraphNode* Node) const { return bAll || Nodes.Contains(Node); }
    };

    inline bool Resolve(UEdGraph* Graph, const FString& Scope, FSelection& Out, FString& Error)
    {
        Out = FSelection();
        if (Scope.IsEmpty()) return true;
        Out.bAll = false;
        FString Mode, Target;
        if (!Scope.Split(TEXT(":"), &Mode, &Target))
        { Error = TEXT("Scope must be node:<ID>, box:<ID>, region:x,y,width,height, or empty."); return false; }
        Mode.TrimStartAndEndInline(); Mode.ToLowerInline(); Target.TrimStartAndEndInline();
        double X = 0, Y = 0, Width = 0, Height = 0;
        if (Mode == TEXT("region"))
        {
            TArray<FString> Values;
            Target.ParseIntoArray(Values, TEXT(","), false);
            if (Values.Num() != 4 || !LexTryParseString(X, *Values[0]) || !LexTryParseString(Y, *Values[1])
                || !LexTryParseString(Width, *Values[2]) || !LexTryParseString(Height, *Values[3])
                || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Width) || !FMath::IsFinite(Height)
                || Width <= 0 || Height <= 0 || !FMath::IsFinite(X + Width) || !FMath::IsFinite(Y + Height))
            { Error = TEXT("Region requires four finite graph-unit numbers: x,y,width,height, with positive width and height."); return false; }
            Out.CaptureMode = TEXT("region");
            Out.CaptureTarget = Target;
        }
        else if (Mode == TEXT("node") || Mode == TEXT("box"))
        {
            TArray<UEdGraphNode*> Matches;
            const FString Key = TurboMCPNodeId::NormaliseKey(Target);
            if (!Key.IsEmpty()) TurboMCPNodeId::FindNodesByKey(Graph, Key, Matches);
            else for (UEdGraphNode* Node : Graph->Nodes)
                if (Node && (TurboMCPNodeId::MakeForNode(Node, true) == Target || TurboMCPNodeId::MakeForNode(Node) == Target)) Matches.Add(Node);
            if (Matches.Num() != 1)
            { Error = TEXT("Scope node ID is missing or ambiguous in the named graph; use an exact reader ID or unique hexadecimal key."); return false; }
            UEdGraphNode* Node = Matches[0];
            if (Mode == TEXT("node"))
            {
                Out.Nodes.Add(Node);
                Out.CaptureMode = TEXT("node");
                Out.CaptureTarget = TurboMCPNodeId::MakeForNode(Node, true);
                return true;
            }
            if (!Node->IsA<UEdGraphNode_Comment>())
            { Error = TEXT("Box scope requires a comment-box ID, not an ordinary node."); return false; }
            X = Node->NodePosX; Y = Node->NodePosY; Width = Node->NodeWidth; Height = Node->NodeHeight;
            Out.CaptureMode = TEXT("region");
            Out.CaptureTarget = FString::Printf(TEXT("%.17g,%.17g,%.17g,%.17g"), X, Y, Width, Height);
            Out.Nodes.Add(Node);
        }
        else { Error = TEXT("Unknown scope mode; use node, box or region."); return false; }
        for (UEdGraphNode* Node : Graph->Nodes) if (Node)
        {
            const FVector2D Size = FTurboMCPGraphTidy::AlignmentNodeSize(Node);
            // Strict rectangle intersection: a shared edge alone is not inclusion.
            if (Node->NodePosX < X + Width && Node->NodePosX + Size.X > X
                && Node->NodePosY < Y + Height && Node->NodePosY + Size.Y > Y) Out.Nodes.Add(Node);
        }
        return true;
    }
}

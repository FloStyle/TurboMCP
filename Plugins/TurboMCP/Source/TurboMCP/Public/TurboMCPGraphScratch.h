// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "TurboMCPNodeId.h"

namespace TurboMCPGraphScratch
{
inline UEdGraphNode* ByGuid(UEdGraph* Graph,const FGuid& Guid)
{
    if(Graph) for(UEdGraphNode* Node:Graph->Nodes) if(Node && Node->NodeGuid==Guid) return Node;
    return nullptr;
}
inline UEdGraphNode* Find(UEdGraph* Graph,const FString& Id)
{
    UEdGraphNode* Found=nullptr;
    if(Graph) for(UEdGraphNode* Node:Graph->Nodes) if(Node && (TurboMCPNodeId::MakeForNode(Node,true)==Id.TrimStartAndEnd() || TurboMCPNodeId::MakeForNode(Node,false)==Id.TrimStartAndEnd()))
    { if(Found) return nullptr; Found=Node; }
    return Found;
}
struct FCopy
{
    TStrongObjectPtr<UBlueprint> Owner;
    UEdGraph* Graph=nullptr;
    bool Init(UBlueprint* Blueprint,UEdGraph* Original)
    {
        Owner.Reset(NewObject<UBlueprint>(GetTransientPackage(),Blueprint->GetClass(),NAME_None,RF_Transient));
        Owner->ParentClass=Blueprint->ParentClass; Owner->GeneratedClass=Blueprint->GeneratedClass;
        Owner->SkeletonGeneratedClass=Blueprint->SkeletonGeneratedClass; Owner->NewVariables=Blueprint->NewVariables;
        FObjectDuplicationParameters Params(Original,Owner.Get());
        Params.ApplyFlags=RF_Transient; Params.FlagMask &= ~(RF_Public|RF_Standalone|RF_Transactional);
        Params.bAssignExternalPackages=false; Params.DuplicationSeed.Add(Blueprint,Owner.Get());
        Graph=Cast<UEdGraph>(StaticDuplicateObjectEx(Params));
        if(!Graph) return false;
        if(Blueprint->FunctionGraphs.Contains(Original)) Owner->FunctionGraphs.Add(Graph);
        else if(Blueprint->MacroGraphs.Contains(Original)) Owner->MacroGraphs.Add(Graph);
        else Owner->UbergraphPages.Add(Graph);
        return true;
    }
};
}

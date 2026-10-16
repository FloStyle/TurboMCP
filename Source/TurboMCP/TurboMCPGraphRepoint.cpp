// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphRepoint.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"
#include "TurboMCPRollback.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"

namespace {
UEdGraph* FindGraph(UBlueprint* B, const FString& Name) {
 if (!B) return nullptr;
 TArray<UEdGraph*> Graphs; Graphs.Append(B->UbergraphPages); Graphs.Append(B->FunctionGraphs); Graphs.Append(B->MacroGraphs);
 for (UEdGraph* G : Graphs) if (G && (Name.IsEmpty() || G->GetName().Equals(Name, ESearchCase::IgnoreCase))) return G;
 return nullptr;
}
UEdGraphNode* FindNode(UEdGraph* G, const FString& Id) {
 if (!G || Id.IsEmpty()) return nullptr;
 for (UEdGraphNode* N : G->Nodes) if (N && TurboMCPNodeId::MakeForNode(N, true) == Id) return N;
 return nullptr;
}
UEdGraphPin* FindPin(UEdGraphNode* N, const FString& Name, EEdGraphPinDirection Direction) {
 if (!N) return nullptr;
 for (UEdGraphPin* P : N->Pins) if (P && P->PinName.ToString().Equals(Name, ESearchCase::CaseSensitive) && P->Direction == Direction) return P;
 return nullptr;
}
FString BoolText(const FString& S) { return S.TrimStartAndEnd().IsEmpty() || !S.Equals(TEXT("false"), ESearchCase::IgnoreCase) && !S.Equals(TEXT("0"), ESearchCase::CaseSensitive) ? TEXT("true") : TEXT("false"); }
}

bool FTurboMCPGraphRepoint::Plan(const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
 const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId, const FString& TargetPin,
 const FString& KeepSource, FPlan& Out, FString& Error) {
 Out = FPlan(); Error.Empty(); Out.Asset=Asset; Out.GraphName=GraphName; Out.SourceNodeId=SourceNodeId; Out.SourcePin=SourcePin; Out.TargetNodeId=TargetNodeId; Out.TargetTypeId=TargetTypeId; Out.TargetPin=TargetPin; Out.bKeepSource=BoolText(KeepSource)==TEXT("true");
 FTurboMCPAssetResolver::FResolved R; Error=FTurboMCPAssetResolver::Resolve(Asset,R); if (!Error.IsEmpty()) return false;
 UBlueprint* B=LoadObject<UBlueprint>(nullptr,*R.ObjectPath); if (!B) { Error=FString::Printf(TEXT("'%s' is not a Blueprint"),*R.PackagePath); return false; }
 UEdGraph* G=FindGraph(B,GraphName); if (!G) { Error=FString::Printf(TEXT("no graph named '%s' in '%s'"),*GraphName,*R.PackagePath); return false; }
 Out.GraphName=G->GetName(); Out.NodeCountBefore=G->Nodes.Num();
 UEdGraphNode* Source=FindNode(G,SourceNodeId); if (!Source) { Out.Refusals.Add(FString::Printf(TEXT("source node '%s' was not found"),*SourceNodeId)); return true; }
 UEdGraphPin* SourceOut=FindPin(Source,SourcePin,EGPD_Output); if (!SourceOut) { Out.Refusals.Add(FString::Printf(TEXT("source '%s' has no output pin named '%s'"),*SourceNodeId,*SourcePin)); return true; }
 if (TargetNodeId.IsEmpty() && TargetTypeId.IsEmpty()) { Out.Refusals.Add(TEXT("targetTypeId is required when targetNodeId is empty")); return true; }
 UEdGraphNode* Target=nullptr;
 if (!TargetNodeId.IsEmpty()) { Target=FindNode(G,TargetNodeId); if (!Target) { Out.Refusals.Add(FString::Printf(TEXT("target node '%s' was not found"),*TargetNodeId)); return true; } }
 else { FString Note; Target=FTurboMCPNodeType::BuildProbeNode(B,G,TargetTypeId,Note); Out.bCreateTarget=true; if (!Target) { Out.Refusals.Add(FString::Printf(TEXT("could not build target type '%s': %s"),*TargetTypeId,*Note)); return true; } }
 UEdGraphPin* TargetOut=FindPin(Target,TargetPin,EGPD_Output); if (!TargetOut) {
  TArray<FString> Outputs; for (UEdGraphPin* P:Target->Pins) if(P&&P->Direction==EGPD_Output) Outputs.Add(P->PinName.ToString());
  Out.Refusals.Add(FString::Printf(TEXT("target has no output pin named '%s'; outputs: %s"),*TargetPin,Outputs.Num()?*FString::Join(Outputs,TEXT(", ")):TEXT("(none)"))); return true;
 }
 for (UEdGraphPin* ConsumerPin : SourceOut->LinkedTo) {
  if (!ConsumerPin || !ConsumerPin->GetOwningNode()) continue;
  FReader Item; Item.ConsumerNodeId=TurboMCPNodeId::MakeForNode(ConsumerPin->GetOwningNode(),true); Item.ConsumerPin=ConsumerPin->PinName.ToString(); Item.OldSourceNodeId=SourceNodeId; Item.OldSourcePin=SourcePin; Out.Readers.Add(MoveTemp(Item));
  const FPinConnectionResponse Check=G->GetSchema()->CanCreateConnection(TargetOut,ConsumerPin);
  if (Check.Response != CONNECT_RESPONSE_MAKE) Out.Refusals.Add(FString::Printf(TEXT("target '%s' cannot connect to reader '%s.%s': %s"),*TargetPin,*Out.Readers.Last().ConsumerNodeId,*Out.Readers.Last().ConsumerPin,*Check.Message.ToString()));
 }
 if (Out.Readers.Num()==0) Out.Refusals.Add(TEXT("source output has no linked readers; nothing would change"));
 return true;
}

FString FTurboMCPGraphRepoint::RenderPreview(const FPlan& P) {
 FString Out=FString::Printf(TEXT("preview: repoint %s.%s in %s"),*P.SourceNodeId,*P.SourcePin,*P.GraphName);
 Out+=FString::Printf(TEXT("\n%d reader(s) would move to %s.%s"),P.Readers.Num(),P.bCreateTarget?*P.TargetTypeId:*P.TargetNodeId,*P.TargetPin);
 for(const FReader& R:P.Readers) Out+=FString::Printf(TEXT("\n    %s.%s: %s.%s -> %s.%s"),*R.ConsumerNodeId,*R.ConsumerPin,*R.OldSourceNodeId,*R.OldSourcePin,P.bCreateTarget?TEXT("(new target)"):*P.TargetNodeId,*P.TargetPin);
 if(P.Refusals.Num()){Out+=FString::Printf(TEXT("\n\nunchanged: %d refusal(s). Nothing would be written."),P.Refusals.Num()); for(const FString& S:P.Refusals) Out+=FString::Printf(TEXT("\n    refused %s"),*S);}
 else Out+=TEXT("\n; a preview changes NOTHING. Call the edit to write it."); return Out;
}

bool FTurboMCPGraphRepoint::Apply(const FPlan& P, FString& Report, FString& Error) {
 Report.Empty(); Error.Empty(); if(P.Refusals.Num()){Report=RenderPreview(P); return true;}
 FTurboMCPAssetResolver::FResolved R; Error=FTurboMCPAssetResolver::Resolve(P.Asset,R); if(!Error.IsEmpty()) return false; UBlueprint* B=LoadObject<UBlueprint>(nullptr,*R.ObjectPath); UEdGraph* G=FindGraph(B,P.GraphName); if(!B||!G){Error=TEXT("graph disappeared before apply");return false;}
 UEdGraphNode* Source=FindNode(G,P.SourceNodeId); UEdGraphPin* SourceOut=FindPin(Source,P.SourcePin,EGPD_Output); if(!SourceOut){Error=TEXT("source disappeared before apply");return false;}
 UEdGraphNode* Target=nullptr; FScopedTransaction Tx(NSLOCTEXT("TurboMCP","GraphRepoint","TurboMCP repoint graph readers")); B->Modify(); G->Modify();
 if(P.bCreateTarget){FString Note; if(FTurboMCPNodeType::SpawnNodeOfType(B,G,P.TargetTypeId,Target,Note)!=FTurboMCPNodeType::ESpawnResult::Spawned){FTurboMCPRollback::EndAndUndoActiveTransaction();Error=FString::Printf(TEXT("could not create target: %s"),*Note);return false;}} else Target=FindNode(G,P.TargetNodeId);
 UEdGraphPin* TargetOut=FindPin(Target,P.TargetPin,EGPD_Output); if(!TargetOut){FTurboMCPRollback::EndAndUndoActiveTransaction();Error=TEXT("target disappeared before apply");return false;}
 for(const FReader& Item:P.Readers){UEdGraphNode* Consumer=FindNode(G,Item.ConsumerNodeId);UEdGraphPin* ConsumerPin=FindPin(Consumer,Item.ConsumerPin,EGPD_Input); if(!ConsumerPin){FTurboMCPRollback::EndAndUndoActiveTransaction();Error=TEXT("reader disappeared before apply");return false;} G->GetSchema()->BreakSinglePinLink(SourceOut, ConsumerPin); if(!G->GetSchema()->TryCreateConnection(TargetOut,ConsumerPin)){FTurboMCPRollback::EndAndUndoActiveTransaction();Error=FString::Printf(TEXT("connection refused for reader %s.%s"),*Item.ConsumerNodeId,*Item.ConsumerPin);return false;}}
 for(const FReader& Item:P.Readers){UEdGraphNode* Consumer=FindNode(G,Item.ConsumerNodeId);UEdGraphPin* ConsumerPin=FindPin(Consumer,Item.ConsumerPin,EGPD_Input); if(!ConsumerPin||!ConsumerPin->LinkedTo.Contains(TargetOut)){FTurboMCPRollback::EndAndUndoActiveTransaction();Error=TEXT("readback mismatch; all original links restored");return false;}}
 if(!P.bKeepSource){if(Source->Pins.Num()>0){G->RemoveNode(Source);}}
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B); FKismetEditorUtilities::CompileBlueprint(B);
 FString TargetId=P.bCreateTarget?TurboMCPNodeId::MakeForNode(Target,true):P.TargetNodeId; Report=FString::Printf(TEXT("changed: repointed %d reader(s) in %s; target %s.%s; old source %s"),P.Readers.Num(),*P.GraphName,*TargetId,*P.TargetPin,P.bKeepSource?TEXT("retained"):TEXT("deleted")); return true;
}

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPGraphRepoint.h"
#include "TurboMCPGraphScratch.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeType.h"
#include "TurboMCPNodeId.h"
#include "EdGraph/EdGraphPin.h"
#include "TurboMCPRollback.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Editor/TransBuffer.h"

namespace
{
 struct FBatchOperation
 {
  FString Id;
  FString Kind;
  FTurboMCPGraphRepoint::FPlan Repoint;
 };
 static FString Refusal(const FString& Message) { return FString::Printf(TEXT("unchanged\nrefused: %s"), *Message); }
 static bool ParseBatch(const FString& Text, TArray<FBatchOperation>& Out, FString& Error)
 {
  Out.Reset();
  TSharedPtr<FJsonValue> Root;
  const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
  if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { Error = TEXT("Operations contains malformed JSON"); return false; }
  const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
  if(Root->Type==EJson::Array) Values=&Root->AsArray();
  else if(Root->Type==EJson::Object)
  {
   for (const auto& Field : Root->AsObject()->Values)
    if (FString(*Field.Key) != TEXT("operations")) { Error=TEXT("batch wrapper accepts only operations"); return false; }
   Root->AsObject()->TryGetArrayField(TEXT("operations"),Values);
  }
  if(!Values) { Error=TEXT("Expected an array or {\"operations\":[...]}. Each entry needs asset, graphName (or graph), sourceNodeId, sourcePin, targetNodeId, targetPin; optional id, kind=graphRepoint, targetTypeId and keepSource."); return false; }
  TSet<FString> Ids;
  TMap<UEdGraph*,TSharedPtr<TurboMCPGraphScratch::FCopy>> Copies;
  for (int32 Index = 0; Index < Values->Num(); ++Index)
  {
   const TSharedPtr<FJsonObject> Object = (*Values)[Index].IsValid() && (*Values)[Index]->Type==EJson::Object ? (*Values)[Index]->AsObject() : nullptr;
   if (!Object.IsValid()) { Error = FString::Printf(TEXT("operation %d is not an object"), Index); return false; }
   TSet<FString> SeenFields;
   for (const auto& Field : Object->Values)
   {
    const FString Key = FString(*Field.Key).ToLower();
    const bool bKnown = Key==TEXT("id") || Key==TEXT("kind") || Key==TEXT("asset") || Key==TEXT("graph") || Key==TEXT("graphname")
     || Key==TEXT("sourcenodeid") || Key==TEXT("sourcepin") || Key==TEXT("targetnodeid") || Key==TEXT("targetpin") || Key==TEXT("targettypeid") || Key==TEXT("keepsource");
    if (!bKnown || SeenFields.Contains(Key)) { Error=FString::Printf(TEXT("operation %d has unknown or case-duplicate field '%s'"),Index+1,*FString(*Field.Key)); return false; }
    SeenFields.Add(Key);
    if (!Field.Value.IsValid() || (Field.Value->Type!=EJson::String && !(Key==TEXT("keepsource") && Field.Value->Type==EJson::Boolean)))
    { Error=FString::Printf(TEXT("operation %d field '%s' has invalid type; expected string (keepSource also accepts boolean)"),Index+1,*FString(*Field.Key)); return false; }
   }
   if (SeenFields.Contains(TEXT("graph")) && SeenFields.Contains(TEXT("graphname"))) { Error=TEXT("use graph or graphName, not both"); return false; }
   // id and kind use the same case-insensitive lookup policy as arguments.
   auto MetaArg=[&](const TCHAR* Name,FString& Value)
   { for(const auto& Field:Object->Values) if(FString(*Field.Key).Equals(Name,ESearchCase::IgnoreCase)) return Field.Value->TryGetString(Value); return false; };
   FBatchOperation Op;
   if (!MetaArg(TEXT("id"), Op.Id) || Op.Id.IsEmpty()) Op.Id=FString::Printf(TEXT("operation-%d"),Index+1);
   if (Ids.Contains(Op.Id)) { Error = FString::Printf(TEXT("duplicate operation id '%s'"), *Op.Id); return false; }
   Ids.Add(Op.Id);
   if (!MetaArg(TEXT("kind"), Op.Kind) || Op.Kind.IsEmpty()) Op.Kind=TEXT("graphRepoint");
   if (Op.Kind != TEXT("graphRepoint")) { Error = FString::Printf(TEXT("operation '%s' has unknown or unsupported kind '%s'"), *Op.Id, *Op.Kind); return false; }
   FString Asset, Graph, SourceNode, SourcePin, TargetNode, TargetType, TargetPin, KeepSource;
   auto StringArg=[&](const TCHAR* Name,FString& Value)
   {
       for(const auto& Field:Object->Values) if(FString(*Field.Key).Equals(Name,ESearchCase::IgnoreCase))
           return Field.Value.IsValid() && Field.Value->TryGetString(Value);
       return false;
   };
   const bool bArgs = StringArg(TEXT("asset"), Asset) && (StringArg(TEXT("graph"), Graph) || StringArg(TEXT("graphName"),Graph))
    && StringArg(TEXT("sourceNodeId"), SourceNode) && StringArg(TEXT("sourcePin"), SourcePin)
    && StringArg(TEXT("targetNodeId"), TargetNode) && StringArg(TEXT("targetPin"), TargetPin);
   if (!bArgs) { Error = FString::Printf(TEXT("operation '%s' has malformed graphRepoint arguments"), *Op.Id); return false; }
   StringArg(TEXT("targetTypeId"), TargetType); StringArg(TEXT("keepSource"), KeepSource);
   for(const auto& Field:Object->Values) if(FString(*Field.Key).Equals(TEXT("keepSource"),ESearchCase::IgnoreCase) && Field.Value.IsValid() && Field.Value->Type==EJson::Boolean) KeepSource=Field.Value->AsBool()?TEXT("true"):TEXT("false"); if (KeepSource.IsEmpty()) KeepSource = TEXT("true");
   FTurboMCPAssetResolver::FResolved Resolved;
   Error=FTurboMCPAssetResolver::Resolve(Asset,Resolved); if(!Error.IsEmpty()) return false;
   UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath); int32 Matches=0;
   UEdGraph* Original=BP?TurboMCPNodeId::FindGraphByName(BP,Graph,Matches):nullptr;
   if(!Original || Matches!=1) { Error=TEXT("batch graph must resolve uniquely"); return false; }
   if (!Copies.IsEmpty() && !Copies.Contains(Original)) { Error=TEXT("batch operations must target one asset and one graph"); return false; }
   TSharedPtr<TurboMCPGraphScratch::FCopy>& Copy=Copies.FindOrAdd(Original);
   if(!Copy.IsValid()) { Copy=MakeShared<TurboMCPGraphScratch::FCopy>(); if(!Copy->Init(BP,Original)) { Error=TEXT("batch graph copy failed"); return false; } }
   if(!FTurboMCPGraphRepoint::PlanOnGraph(Copy->Owner.Get(),Copy->Graph,Original,Asset,SourceNode,SourcePin,TargetNode,TargetType,TargetPin,KeepSource,Op.Repoint,Error)) return false;
   if(!Op.Repoint.bAcceptable()) { Error=FString::Printf(TEXT("operation '%s' refused after earlier simulated operations: %s"),*Op.Id,*FString::Join(Op.Repoint.Refusals,TEXT("; "))); return false; }
   auto StagedNode=[&](const FString& Id)->UEdGraphNode* { UEdGraphNode* N=TurboMCPGraphScratch::Find(Original,Id); return N?TurboMCPGraphScratch::ByGuid(Copy->Graph,N->NodeGuid):nullptr; };
   UEdGraphNode* From=StagedNode(SourceNode); UEdGraphNode* To=StagedNode(TargetNode);
   if(Op.Repoint.bCreateTarget)
   {
       FString Why; UEdGraphNode* Probe=FTurboMCPNodeType::BuildProbeNode(Copy->Owner.Get(),Copy->Graph,TargetType,Why);
       To=Probe?DuplicateObject<UEdGraphNode>(Probe,Copy->Graph):nullptr;
       if(To) { To->CreateNewGuid(); Copy->Graph->AddNode(To,false,false); }
   }
   UEdGraphPin* A=From?From->FindPin(FName(*SourcePin),EGPD_Output):nullptr;
   UEdGraphPin* B=To?To->FindPin(FName(*TargetPin),EGPD_Output):nullptr;
   if(!A || !B) { Error=TEXT("batch staging pin unavailable"); return false; }
   const TArray<UEdGraphPin*> Readers=A->LinkedTo;
   for(UEdGraphPin* ReaderPin:Readers) { A->BreakLinkTo(ReaderPin); B->MakeLinkTo(ReaderPin); }
   if(!Op.Repoint.bKeepSource) { From->BreakAllNodeLinks(); Copy->Graph->RemoveNode(From); }
   Out.Add(MoveTemp(Op));
  }
  if (Out.Num() == 0) { Error = TEXT("operations array is empty"); return false; }
  return true;
 }
 static FString Render(const TArray<FBatchOperation>& Ops)
 {
  TSet<FString> Assets; FString Result = TEXT("unchanged\nplanned\nplanning=sequential transient graph; each operation sees preceding staged changes\n");
  for (const FBatchOperation& Op : Ops) { Assets.Add(Op.Repoint.Asset); Result += FString::Printf(TEXT("%s [%s]: %s\n"), *Op.Id, *Op.Kind, *FTurboMCPGraphRepoint::RenderPreview(Op.Repoint)); }
  Result += FString::Printf(TEXT("operations=%d\naffectedAssets=%s\nundo=one\ncompile=deferred\nsave=deferred\n"), Ops.Num(), *FString::Join(Assets.Array(), TEXT(",")));
  return Result;
 }
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPBatchEdit(const FString& Operations)
{
 TArray<FBatchOperation> Ops; FString Error; if (!ParseBatch(Operations, Ops, Error)) return Refusal(Error); return Render(Ops);
}
FString UTurboMCPEditToolset::EditTurboMCPBatchEdit(const FString& Operations)
{
 if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress()) return Refusal(TEXT("requires an idle editor outside an active transaction"));
 TArray<FBatchOperation> Ops; FString Error; if (!ParseBatch(Operations, Ops, Error)) return Refusal(Error);
 UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
 if (!Buffer) return Refusal(TEXT("requires an editor transaction buffer"));
 TUniquePtr<FScopedTransaction> Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP", "BatchEdit", "TurboMCP batch edit"));
 if (!Transaction->IsOutstanding() || Buffer->ActiveCount != 1 || Buffer->UndoBuffer.IsEmpty())
  return Refusal(TEXT("could not establish one owned transaction"));
 const FGuid OwnedId = Buffer->UndoBuffer.Last()->GetId();
 TArray<FString> Applied;
 for (FBatchOperation& Op : Ops)
 {
  FString Report; FTurboMCPGraphRepoint::FApplyContext Context; Context.bCompile = false; Context.bSave = false;
  if (!FTurboMCPGraphRepoint::ApplyCore(Op.Repoint, Context, Report, Error))
  {
   // Destroy the scope before undo: a manual EndTransaction leaves its destructor
   // outstanding and would end the transaction a second time on return.
   Transaction.Reset();
   const int32 CurrentIndex = Buffer->GetQueueLength() - Buffer->GetUndoCount() - 1;
   const FTransaction* Current = CurrentIndex >= 0 ? Buffer->GetTransaction(CurrentIndex) : nullptr;
   const bool bRolledBack = OwnedId.IsValid() && Buffer->ActiveCount == 0
    && Current && Current->GetId() == OwnedId && GEditor->UndoTransaction(false);
   // The failing operation may have changed the graph before returning false.
   // Include it in the attempted rollback scope; UndoTransaction alone is not
   // a graph read-back verification, so never promise unchanged state here.
   TArray<FString> Attempted = Applied;
   Attempted.Add(Op.Id);
   return FString::Printf(TEXT("partial\nfailed id=%s kind=%s\nappliedBeforeFailure=%s\nrollbackScope=%s\nrollback=%s\nrestoration=unverified\nretry=inspect_graph_before_retry\nerror=%s"), *Op.Id, *Op.Kind, *FString::Join(Applied, TEXT(",")), *FString::Join(Attempted, TEXT(",")), bRolledBack ? TEXT("undo_applied") : TEXT("not_confirmed"), *Error);
  }
  Applied.Add(Op.Id);
 }
 return FString::Printf(TEXT("changed\noperations=%d\napplied=%s\nundo=one\ncompile=deferred\nsave=deferred\nrollbackScope=none"), Ops.Num(), *FString::Join(Applied, TEXT(",")));
}

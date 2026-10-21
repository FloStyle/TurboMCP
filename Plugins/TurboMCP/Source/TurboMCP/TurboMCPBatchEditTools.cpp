// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPGraphRepoint.h"
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
  else if(Root->Type==EJson::Object) Root->AsObject()->TryGetArrayField(TEXT("operations"),Values);
  if(!Values) { Error=TEXT("Expected an array or {\"operations\":[...]}. Each entry needs asset, graphName (or graph), sourceNodeId, sourcePin, targetNodeId, targetPin; optional id, kind=graphRepoint, targetTypeId and keepSource."); return false; }
  TSet<FString> Ids;
  for (int32 Index = 0; Index < Values->Num(); ++Index)
  {
   const TSharedPtr<FJsonObject> Object = (*Values)[Index].IsValid() && (*Values)[Index]->Type==EJson::Object ? (*Values)[Index]->AsObject() : nullptr;
   if (!Object.IsValid()) { Error = FString::Printf(TEXT("operation %d is not an object"), Index); return false; }
   FBatchOperation Op;
   if (!Object->TryGetStringField(TEXT("id"), Op.Id) || Op.Id.IsEmpty()) Op.Id=FString::Printf(TEXT("operation-%d"),Index+1);
   if (Ids.Contains(Op.Id)) { Error = FString::Printf(TEXT("duplicate operation id '%s'"), *Op.Id); return false; }
   Ids.Add(Op.Id);
   if (!Object->TryGetStringField(TEXT("kind"), Op.Kind) || Op.Kind.IsEmpty()) Op.Kind=TEXT("graphRepoint");
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
   if (!FTurboMCPGraphRepoint::Plan(Asset, Graph, SourceNode, SourcePin, TargetNode, TargetType, TargetPin, KeepSource, Op.Repoint, Error)) { Error = FString::Printf(TEXT("operation '%s' plan failed: %s"), *Op.Id, *Error); return false; }
   if (!Op.Repoint.bAcceptable()) { Error = FString::Printf(TEXT("operation '%s' refused: %s"), *Op.Id, *FString::Join(Op.Repoint.Refusals, TEXT("; "))); return false; }
   Out.Add(MoveTemp(Op));
  }
  if (Out.Num() == 0) { Error = TEXT("operations array is empty"); return false; }
  return true;
 }
 static FString Render(const TArray<FBatchOperation>& Ops)
 {
  TSet<FString> Assets; FString Result = TEXT("unchanged\nplanned\n");
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
   return FString::Printf(TEXT("%s\nfailed id=%s kind=%s\nappliedBeforeFailure=%s\nrollback=%s\nerror=%s"), bRolledBack ? TEXT("unchanged") : TEXT("partial"), *Op.Id, *Op.Kind, *FString::Join(Applied, TEXT(",")), bRolledBack ? TEXT("complete") : TEXT("failed"), *Error);
  }
  Applied.Add(Op.Id);
 }
 return FString::Printf(TEXT("changed\noperations=%d\napplied=%s\nundo=one\ncompile=deferred\nsave=deferred\nrollbackScope=none"), Ops.Num(), *FString::Join(Applied, TEXT(",")));
}

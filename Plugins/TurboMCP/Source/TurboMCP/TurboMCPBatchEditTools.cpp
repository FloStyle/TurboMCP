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
  TSharedPtr<FJsonObject> Root;
  const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
  if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { Error = TEXT("Operations is not valid JSON"); return false; }
  const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
  if (!Root->TryGetArrayField(TEXT("operations"), Values) || !Values) { Error = TEXT("Operations must contain an operations array"); return false; }
  TSet<FString> Ids;
  for (int32 Index = 0; Index < Values->Num(); ++Index)
  {
   const TSharedPtr<FJsonObject> Object = (*Values)[Index].IsValid() ? (*Values)[Index]->AsObject() : nullptr;
   if (!Object.IsValid()) { Error = FString::Printf(TEXT("operation %d is not an object"), Index); return false; }
   FBatchOperation Op;
   if (!Object->TryGetStringField(TEXT("id"), Op.Id) || Op.Id.IsEmpty()) { Error = FString::Printf(TEXT("operation %d has no stable id"), Index); return false; }
   if (Ids.Contains(Op.Id)) { Error = FString::Printf(TEXT("duplicate operation id '%s'"), *Op.Id); return false; }
   Ids.Add(Op.Id);
   if (!Object->TryGetStringField(TEXT("kind"), Op.Kind) || Op.Kind.IsEmpty()) { Error = FString::Printf(TEXT("operation '%s' has no kind"), *Op.Id); return false; }
   if (Op.Kind != TEXT("graphRepoint")) { Error = FString::Printf(TEXT("operation '%s' has unknown or unsupported kind '%s'"), *Op.Id, *Op.Kind); return false; }
   FString Asset, Graph, SourceNode, SourcePin, TargetNode, TargetType, TargetPin, KeepSource;
   const bool bArgs = Object->TryGetStringField(TEXT("asset"), Asset) && Object->TryGetStringField(TEXT("graph"), Graph)
    && Object->TryGetStringField(TEXT("sourceNodeId"), SourceNode) && Object->TryGetStringField(TEXT("sourcePin"), SourcePin)
    && Object->TryGetStringField(TEXT("targetNodeId"), TargetNode) && Object->TryGetStringField(TEXT("targetPin"), TargetPin);
   if (!bArgs) { Error = FString::Printf(TEXT("operation '%s' has malformed graphRepoint arguments"), *Op.Id); return false; }
   Object->TryGetStringField(TEXT("targetTypeId"), TargetType); Object->TryGetStringField(TEXT("keepSource"), KeepSource); if (KeepSource.IsEmpty()) KeepSource = TEXT("true");
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
 if (!GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress()) return Refusal(TEXT("requires an idle editor outside an active transaction"));
 TArray<FBatchOperation> Ops; FString Error; if (!ParseBatch(Operations, Ops, Error)) return Refusal(Error);
 const int32 PreviousIndex = GEditor->Trans ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1 : -1;
 const FTransaction* Previous = PreviousIndex >= 0 ? GEditor->Trans->GetTransaction(PreviousIndex) : nullptr;
 const FGuid PreviousId = Previous ? Previous->GetId() : FGuid();
 FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "BatchEdit", "TurboMCP batch edit"));
 TArray<FString> Applied;
 for (FBatchOperation& Op : Ops)
 {
  FString Report; FTurboMCPGraphRepoint::FApplyContext Context; Context.bCompile = false; Context.bSave = false;
  if (!FTurboMCPGraphRepoint::ApplyCore(Op.Repoint, Context, Report, Error))
  {
   GEditor->EndTransaction();
   const int32 CurrentIndex = GEditor->Trans ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1 : -1;
   const FTransaction* Current = CurrentIndex >= 0 ? GEditor->Trans->GetTransaction(CurrentIndex) : nullptr;
   // Never undo a user's previous edit when this batch failed before recording changes.
   const bool bRolledBack = Current && Current->GetId() != PreviousId && GEditor->UndoTransaction(false);
   return FString::Printf(TEXT("%s\nfailed id=%s kind=%s\nappliedBeforeFailure=%s\nrollback=%s\nerror=%s"), bRolledBack ? TEXT("unchanged") : TEXT("partial"), *Op.Id, *Op.Kind, *FString::Join(Applied, TEXT(",")), bRolledBack ? TEXT("complete") : TEXT("failed"), *Error);
  }
  Applied.Add(Op.Id);
 }
 return FString::Printf(TEXT("changed\noperations=%d\napplied=%s\nundo=one\ncompile=deferred\nsave=deferred\nrollbackScope=none"), Ops.Num(), *FString::Join(Applied, TEXT(",")));
}

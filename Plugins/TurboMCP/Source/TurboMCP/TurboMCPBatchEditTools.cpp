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
#include "Serialization/ObjectWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"

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
   if (!Object.IsValid()) { Error = FString::Printf(TEXT("Operation %d must be a JSON object."), Index + 1); return false; }
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
   if (!BP) { Error=TEXT("batch Blueprint could not be loaded"); return false; }
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
  Result += FString::Printf(TEXT("operations=%d\naffectedAssets=%s\nundo=not_created_by_preview\ncompile=not_run\nsave=not_requested\n"), Ops.Num(), *FString::Join(Assets.Array(), TEXT(",")));
  return Result;
 }
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPBatchEdit(const FString& Operations)
{
 TArray<FBatchOperation> Ops; FString Error; if (!ParseBatch(Operations, Ops, Error)) return Refusal(Error); return Render(Ops);
}
namespace
{
// Capture source objects only. Batch operations do not compile or save, so this
// verification does not claim to restore generated instances or persisted data.
// Strong references keep deleted source nodes available for the owned undo.
struct FBatchSnapshot
{
 struct FEntry
 {
  TStrongObjectPtr<UObject> Object;
  TArray<uint8> Bytes;
  explicit FEntry(UObject* Value) : Object(Value)
  {
   // Verbatim duplication avoids creating a localization namespace while reading.
   FObjectWriter Writer(Value, Bytes, false, false, false, PPF_DuplicateVerbatim);
  }
 };
 TArray<TUniquePtr<FEntry>> Entries;
 bool bDirtyBefore = false;
 explicit FBatchSnapshot(UBlueprint* Blueprint)
 {
  bDirtyBefore = Blueprint->GetOutermost()->IsDirty();
  TArray<UObject*> Objects;
  GetObjectsWithOuter(Blueprint, Objects, EGetObjectsFlags::IncludeNestedObjects);
  Objects.Add(Blueprint);
  for (UObject* Object : Objects)
   if (IsValid(Object)) Entries.Add(MakeUnique<FEntry>(Object));
 }
 bool Matches() const
 {
  for (const TUniquePtr<FEntry>& Entry : Entries)
  {
   if (!IsValid(Entry->Object.Get())) return false;
   TArray<uint8> Current;
   FObjectWriter Writer(Entry->Object.Get(), Current, false, false, false, PPF_DuplicateVerbatim);
   if (Current != Entry->Bytes) return false;
  }
  return true;
 }
};

// The public entry point validates all operations before entering this shared executor.
static FString ExecuteBatch(TArray<FBatchOperation>& Ops)
{
 if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress()) return Refusal(TEXT("requires an idle editor outside an active transaction"));
 FString Error;
 if (Ops.IsEmpty()) return Refusal(TEXT("The batch contains no operations."));
 FTurboMCPAssetResolver::FResolved Resolved;
 Error = FTurboMCPAssetResolver::Resolve(Ops[0].Repoint.Asset, Resolved);
 if (!Error.IsEmpty()) return Refusal(Error);
 UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
 if (!Blueprint) return Refusal(TEXT("The batch Blueprint could not be loaded."));
 const FBatchSnapshot Snapshot(Blueprint);
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
   // Undo success is not proof. Compare all captured source objects before
   // restoring the dirty flag; never hide a mismatch by clearing that flag.
   const bool bSourceRestored = bRolledBack && Snapshot.Matches();
   if (bSourceRestored) Blueprint->GetOutermost()->SetDirtyFlag(Snapshot.bDirtyBefore);
   const bool bVerified = bSourceRestored && Blueprint->GetOutermost()->IsDirty() == Snapshot.bDirtyBefore;
   TArray<FString> Attempted = Applied;
   Attempted.Add(Op.Id);
   return FString::Printf(TEXT("%s\nfailed id=%s kind=%s\nappliedBeforeFailure=%s\nrollbackScope=%s\nrollback=%s\nrestoration=%s\ncompile=not_run\nsave=not_requested\nretry=inspect_graph_before_retry\nerror=%s"),
    bVerified ? TEXT("unchanged") : TEXT("partial"), *Op.Id, *Op.Kind,
    *FString::Join(Applied, TEXT(",")), *FString::Join(Attempted, TEXT(",")),
    bRolledBack ? TEXT("undo_applied") : TEXT("not_confirmed"),
    bVerified ? TEXT("source_objects_and_dirty_flag_verified") : TEXT("not_verified"), *Error);
  }
  Applied.Add(Op.Id);
 }
 return FString::Printf(TEXT("changed\noperations=%d\napplied=%s\nundo=one\ncompile=not_run\nsave=not_requested\nrollbackScope=none\nInspect the graph and compile before saving; runtime behavior is not verified."), Ops.Num(), *FString::Join(Applied, TEXT(",")));
}

}

FString UTurboMCPEditToolset::EditTurboMCPBatchEdit(const FString& Operations)
{
 if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
  return Refusal(TEXT("requires an idle editor outside an active transaction"));
 TArray<FBatchOperation> Ops;
 FString Error;
 if (!ParseBatch(Operations, Ops, Error)) return Refusal(Error);
 return ExecuteBatch(Ops);
}

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPBatchRollbackTest,
    "TurboMCP.Batch.PostMutationFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPBatchRollbackTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Game/Tests/BatchRollback_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BatchRollbackFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPBatchTest"));
    if (!TestNotNull(TEXT("Blueprint factory succeeds"), Blueprint)) return false;
    FAssetRegistryModule::AssetCreated(Blueprint);
    ON_SCOPE_EXIT
    {
        FAssetRegistryModule::AssetDeleted(Blueprint);
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false); // Disposable, unsaved fixture only.
        Package->MarkAsGarbage();
    };
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, TEXT("FailureGraph"), RF_Transactional);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    auto MakeCall = [&](int32 X)
    {
        UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
        Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        Graph->AddNode(Node, false, false);
        Node->CreateNewGuid();
        Node->AllocateDefaultPins();
        Node->NodePosX = X;
        return Node;
    };
    UK2Node_CallFunction* Source = MakeCall(0);
    UK2Node_CallFunction* Consumer = MakeCall(300);
    UK2Node_CallFunction* Target = MakeCall(600);
    Source->FindPinChecked(TEXT("then"))->MakeLinkTo(Consumer->FindPinChecked(TEXT("execute")));
    Consumer->FindPinChecked(TEXT("InString"))->DefaultValue = TEXT("preserve-me");
    Package->SetDirtyFlag(false);

    UK2Node_CallFunction* FinalTarget = MakeCall(900);
    auto Operation = [&](const TCHAR* Id, UEdGraphNode* From, UEdGraphNode* To, const TCHAR* Pin)
    {
        return FString::Printf(TEXT("{\"id\":\"%s\",\"asset\":\"%s\",\"graphName\":\"%s\",\"sourceNodeId\":\"%s\",\"sourcePin\":\"then\",\"targetNodeId\":\"%s\",\"targetPin\":\"%s\",\"keepSource\":true}"),
            Id, *Blueprint->GetPathName(), *Graph->GetName(), *TurboMCPNodeId::MakeForNode(From, true), *TurboMCPNodeId::MakeForNode(To, true), Pin);
    };

    const FString Payload = TEXT("[") + Operation(TEXT("first"), Source, Target, TEXT("then")) + TEXT(",")
        + Operation(TEXT("second"), Target, FinalTarget, TEXT("then")) + TEXT("]");
    TArray<FBatchOperation> Ops;
    FString Error;
    if (!TestTrue(TEXT("Production batch planning succeeds"), ParseBatch(Payload, Ops, Error))) { AddError(Error); return false; }
    if (!TestEqual(TEXT("Two operations planned"), Ops.Num(), 2)) return false;
    // Force the second operation's final verification to fail after its rewiring.
    ++Ops[1].Repoint.NodeCountBefore;
    const FString Result = ExecuteBatch(Ops);
    AddInfo(TEXT("Failed batch result: ") + Result);
    TestTrue(TEXT("Second operation failed after first applied"), Result.Contains(TEXT("failed id=second"))
        && Result.Contains(TEXT("appliedBeforeFailure=first")));
    TestTrue(TEXT("Both operations included in undo"), Result.Contains(TEXT("rollbackScope=first,second"))
        && Result.Contains(TEXT("rollback=undo_applied")));
    TestTrue(TEXT("Restoration verified independently"), Result.StartsWith(TEXT("unchanged")) && Result.Contains(TEXT("restoration=source_objects_and_dirty_flag_verified")));
    TestFalse(TEXT("Original clean flag restored"), Package->IsDirty());
    TestFalse(TEXT("No active transaction remains"), GEditor->IsTransactionActive());
    TestEqual(TEXT("Original nodes retained"), Graph->Nodes.Num(), 4);
    UEdGraphPin* Input = Consumer->FindPinChecked(TEXT("execute"));
    UEdGraphPin* OriginalOutput = Source->FindPinChecked(TEXT("then"));
    TestTrue(TEXT("Original reciprocal connection restored"), Input->LinkedTo.Num() == 1 && Input->LinkedTo.Contains(OriginalOutput)
        && OriginalOutput->LinkedTo.Num() == 1 && OriginalOutput->LinkedTo.Contains(Input));
    TestTrue(TEXT("Both attempted target outputs restored"), Target->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty()
        && FinalTarget->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    TestEqual(TEXT("Unrelated default preserved"), Consumer->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
    TestEqual(TEXT("Unrelated position preserved"), Consumer->NodePosX, 300);
    return !HasAnyErrors();
}
#endif

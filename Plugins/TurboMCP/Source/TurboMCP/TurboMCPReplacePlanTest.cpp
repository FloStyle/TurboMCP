// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPGraphReplace.h"
#include "Tools/TurboMCPToolsets.h"
#include "Editor/TransBuffer.h"
#include "EdGraphNode_Comment.h"
#include "Kismet/KismetTextLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Tunnel.h"
#include "Editor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPReplacePlanTest,
    "TurboMCP.Replace.LinkedRetention",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPReplacePlanTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Game/Tests/ReplacePlan_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("ReplaceFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPReplaceTest"));
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
    UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
    if (!TestNotNull(TEXT("Editor transaction buffer"), Buffer)) return false;

    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, TEXT("FailureGraph"), RF_Transactional);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    auto Call = [&](UClass* Owner, FName Function, int32 X)
    {
        UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
        Node->SetFromFunction(Owner->FindFunctionByName(Function));
        Graph->AddNode(Node, false, false);
        Node->CreateNewGuid();
        Node->AllocateDefaultPins();
        Node->NodePosX = X;
        return Node;
    };
    auto Print = [&](int32 X) { return Call(UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), X); };
    UK2Node_CallFunction* Source = Print(280);
    UK2Node_CallFunction* Consumer = Print(620);
    UK2Node_CallFunction* Target = Print(980);
    UK2Node_CallFunction* TextSource = Call(UKismetTextLibrary::StaticClass(), TEXT("Conv_TextToString"), 180);
    UK2Node_CallFunction* ObjectSource = Call(UKismetSystemLibrary::StaticClass(), TEXT("IsValid"), 100);
    UEdGraphPin* TextDefault = TextSource->FindPinChecked(TEXT("InText"));
    const FText Localized = FText::ChangeKey(TEXT("FixtureNamespace"), TEXT("ExactKey"), FText::FromString(TEXT("shared display text")));
    TextDefault->DefaultTextValue = Localized;
    ObjectSource->FindPinChecked(TEXT("Object"))->DefaultObject = GetDefault<UKismetTextLibrary>();
    TextSource->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Source->FindPinChecked(TEXT("InString")));
    TextSource->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Target->FindPinChecked(TEXT("InString")));
    Source->FindPinChecked(TEXT("then"))->MakeLinkTo(Consumer->FindPinChecked(TEXT("execute")));
    Consumer->FindPinChecked(TEXT("then"))->MakeLinkTo(Target->FindPinChecked(TEXT("execute")));
    Consumer->FindPinChecked(TEXT("InString"))->DefaultValue = TEXT("preserve \"quotes\"; and\nnewlines\\");
    Target->NodeComment = TEXT("test deletion restores authored state");

    UK2Node_CustomEvent* Event = NewObject<UK2Node_CustomEvent>(Graph, NAME_None, RF_Transactional);
    Event->CustomFunctionName = TEXT("ReplacementStart");
    Graph->AddNode(Event, false, false);
    Event->CreateNewGuid();
    Event->AllocateDefaultPins();
    Event->FindPinChecked(TEXT("then"))->MakeLinkTo(Source->FindPinChecked(TEXT("execute")));
    UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Graph, NAME_None, RF_Transactional);
    Graph->AddNode(Comment, false, false);
    Comment->CreateNewGuid();
    Comment->NodeComment = TEXT("protected fixture documentation");
    Comment->NodePosX = -300;
    Package->SetDirtyFlag(false); // Establish a clean, unsaved baseline for refusal/no-op checks.

    const FGuid TargetGuid = Target->NodeGuid;
    auto Id = [](const UEdGraphNode& Node) { return Node.NodeGuid.ToString(EGuidFormats::DigitsLower); };
    auto Directive = [&](const UEdGraphNode& Node)
    {
        return TEXT("; tmcp-delete-v1: {\"nodeId\":\"") + Id(Node) + TEXT("\"}\n");
    };
    auto Load = [&](int32 MaxChars = 24000)
    {
        return UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Blueprint->GetPathName(), Graph->GetName(),
            TEXT("edit"), TEXT("bodies"), 0, MaxChars);
    };
    const FString Export = Load();
    if (!TestTrue(TEXT("Supported graph exports a complete edit document"), Export.Contains(TEXT("\"complete\":true"))))
    { AddError(Export); return false; }
    TestEqual(TEXT("Repeated export uses stable order and fingerprint"), Load(), Export);
    TestTrue(TEXT("Localized default is rendered with its namespace and key"), Export.Contains(TEXT("FixtureNamespace")) && Export.Contains(TEXT("ExactKey")));
    TestTrue(TEXT("Object default is rendered by exact object path"), Export.Contains(GetDefault<UKismetTextLibrary>()->GetPathName()));
    const FString TextFormTag = TEXT(" ; ") + Id(*TextSource) + TEXT("\n");
    TestEqual(TEXT("Shared pure node emitted exactly once"), Export.Replace(*TextFormTag, TEXT("")).Len(), Export.Len() - TextFormTag.Len());
    const FString TargetTag = TEXT(" ; ") + Id(*Target) + TEXT("\n");
    const int32 Tag = Export.Find(TargetTag, ESearchCase::CaseSensitive);
    if (!TestTrue(TEXT("Exact target form exists"), Tag != INDEX_NONE)) return false;
    const int32 Begin = Export.Left(Tag).Find(TEXT("\n("), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1;
    const int32 Closing = Export.Find(TEXT("\n)\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Tag);
    if (!TestTrue(TEXT("Exact target form has bounds"), Begin > 0 && Closing != INDEX_NONE)) return false;
    const FString RemovedForm = Export.Left(Begin) + Export.Mid(Closing + 3);
    const FString AddedForm = TEXT("(Development|PrintString\n InString \"replacement\"\n execute (-> ") + Id(*Consumer) + TEXT(":then)\n)\n");
    const FString NewBody = RemovedForm + Directive(*Target) + AddedForm;

    // Independent inventory of live identities, defaults, geometry and both
    // endpoints. The oracle calls neither the exporter nor Replace snapshot.
    auto Inspect = [&]()
    {
        TArray<FString> Records;
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            Records.Add(FString::Printf(TEXT("node %s %s %d,%d %s"), *Id(*Node), *Node->GetClass()->GetPathName(),
                Node->NodePosX, Node->NodePosY, *Node->NodeComment));
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                FString Localization;
                FTextStringHelper::WriteToBuffer(Localization, Pin->DefaultTextValue, true);
                const FString Key = Id(*Node) + TEXT(":") + Pin->PinId.ToString();
                Records.Add(Key + TEXT(" ") + Pin->PinName.ToString() + TEXT(" ") + Pin->PinType.PinCategory.ToString()
                    + TEXT(" ") + Pin->DefaultValue + TEXT(" ") + GetPathNameSafe(Pin->DefaultObject) + TEXT(" ") + Localization);
                for (const UEdGraphPin* Other : Pin->LinkedTo)
                    Records.Add(Key + TEXT(" -> ") + Id(*Other->GetOwningNode()) + TEXT(":") + Other->PinId.ToString());
            }
        }
        Records.Sort();
        return FString::Join(Records, TEXT("\n"));
    };
    auto AppliedUndoCount = [&]() { return Buffer->GetQueueLength() - Buffer->GetUndoCount(); };
    const FString Original = Inspect();
    const int32 BaselineUndo = Buffer->GetQueueLength();
    FString Error, Report;
    auto Plan = [&](const FString& Text, FTurboMCPGraphReplace::FPlan& Out, const FString& Name = FString())
    {
        return FTurboMCPGraphReplace::Plan(Blueprint->GetPathName(), Name.IsEmpty() ? Graph->GetName() : Name, Text, Out, Error);
    };
    auto Refused = [&](const TCHAR* Label, const FString& Text, const FString& Reason)
    {
        FTurboMCPGraphReplace::FPlan Candidate;
        TestTrue(Label, Plan(Text, Candidate) && !Candidate.bAcceptable());
        const FString Preview = FTurboMCPGraphReplace::RenderPreview(Candidate);
        TestTrue(TEXT("Refusal gives the expected reason"), Preview.Contains(Reason));
        if (!Preview.Contains(Reason)) AddError(Preview + TEXT("\n") + Error);
        TestEqual(TEXT("Refusal preserves graph identities, defaults and exact endpoints"), Inspect(), Original);
        TestFalse(TEXT("Refusal preserves clean package"), Package->IsDirty());
        TestEqual(TEXT("Refusal adds no undo history"), Buffer->GetQueueLength(), BaselineUndo);
    };

    FTurboMCPGraphReplace::FPlan NoOp;
    TestTrue(TEXT("Complete round trip plans"), Plan(Export, NoOp) && NoOp.bAcceptable());
    TestTrue(TEXT("Complete round trip applies as a clean no-op"), FTurboMCPGraphReplace::Apply(NoOp, Report, Error)
        && Report.StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("No-op preserves graph state"), Inspect(), Original);
    TestFalse(TEXT("No-op preserves clean package"), Package->IsDirty());
    TestEqual(TEXT("No-op adds no undo history"), Buffer->GetQueueLength(), BaselineUndo);

    Refused(TEXT("Legacy snippet refused"), AddedForm, TEXT("tmcp-edit-v1 header"));
    Refused(TEXT("Truncated export refused"), Load(256), TEXT("complete tmcp-edit-v1"));
    Refused(TEXT("Omission alone refused"), RemovedForm + AddedForm, TEXT("omitted semantic node"));
    Refused(TEXT("Duplicate deletion refused"), NewBody + Directive(*Target), TEXT("duplicate deletion"));
    Refused(TEXT("Conflicting retention/deletion refused"), Export + Directive(*Target), TEXT("conflicting retention"));
    Refused(TEXT("Duplicate existing form refused"), Export + Export.Mid(Begin, Closing + 3 - Begin), TEXT("duplicate node form"));
    Refused(TEXT("Protected event refused"), Export + Directive(*Event), TEXT("protected deletion"));
    Refused(TEXT("Protected comment refused"), Export + Directive(*Comment), TEXT("protected deletion"));
    Refused(TEXT("Foreign node refused"), RemovedForm + Directive(*Target)
        + AddedForm.Replace(*Id(*Consumer), *FGuid::NewGuid().ToString(EGuidFormats::DigitsLower)), TEXT("foreign, omitted, deleted"));
    Refused(TEXT("Foreign graph GUID refused"), Export.Replace(*Graph->GraphGuid.ToString(EGuidFormats::DigitsLower),
        *FGuid::NewGuid().ToString(EGuidFormats::DigitsLower)), TEXT("foreign edit header"));
    Refused(TEXT("Foreign asset refused"), Export.Replace(*Blueprint->GetPathName(), TEXT("/Game/Foreign.Foreign")), TEXT("foreign edit header"));

    UEdGraph* Macro = NewObject<UEdGraph>(Blueprint, TEXT("ProtectedMacro"), RF_Transactional);
    Macro->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->MacroGraphs.Add(Macro);
    UK2Node_Tunnel* Tunnel = NewObject<UK2Node_Tunnel>(Macro, NAME_None, RF_Transactional);
    Macro->AddNode(Tunnel, false, false);
    Tunnel->CreateNewGuid();
    Tunnel->AllocateDefaultPins();
    TestFalse(TEXT("Macro tunnel is protected by engine"), Tunnel->CanUserDeleteNode());
    const FString Unsupported = UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Blueprint->GetPathName(), Macro->GetName(),
        TEXT("edit"), TEXT("bodies"), 0, 24000);
    TestTrue(TEXT("Unsupported export is visibly incomplete and identifies exact node"), Unsupported.Contains(TEXT("\"complete\":false"))
        && Unsupported.Contains(Id(*Tunnel)) && Unsupported.Contains(TEXT("unsupported node semantics")));
    FTurboMCPGraphReplace::FPlan ForgedComplete;
    TestTrue(TEXT("Caller cannot promote unsupported export to complete"), Plan(
        Unsupported.Replace(TEXT("\"complete\":false"), TEXT("\"complete\":true")) + Directive(*Tunnel), ForgedComplete, Macro->GetName())
        && !ForgedComplete.bAcceptable());
    TestTrue(TEXT("Unsupported graph retains its exact protected node"), Macro->Nodes.Num() == 1 && Macro->Nodes.Contains(Tunnel));
    TestEqual(TEXT("Unsupported graph refusal adds no undo history"), Buffer->GetQueueLength(), BaselineUndo);

    FTurboMCPGraphReplace::FPlan LivePlan;
    if (!TestTrue(TEXT("Explicit one-node removal and addition plan before editing"), Plan(NewBody, LivePlan) && LivePlan.bAcceptable()))
    { AddError(Error + FTurboMCPGraphReplace::RenderPreview(LivePlan)); return false; }
    TestEqual(TEXT("Scratch plan preserves original graph exactly"), Inspect(), Original);
    TestFalse(TEXT("Scratch plan leaves package clean"), Package->IsDirty());
    TestEqual(TEXT("Scratch plan adds no undo history"), Buffer->GetQueueLength(), BaselineUndo);

    // Change only the localization identity, while leaving displayed text and
    // the node/pin count unchanged. A stored plan must fail its live SHA-256 check.
    TextDefault->DefaultTextValue = FText::ChangeKey(TEXT("FixtureNamespace"), TEXT("EditedOutsidePlan"), Localized);
    const FString ChangedOutside = Inspect();
    TestFalse(TEXT("Apply refuses semantic change since planning"), FTurboMCPGraphReplace::Apply(LivePlan, Report, Error));
    TestTrue(TEXT("Apply reports stale graph"), Error.Contains(TEXT("stale")));
    TestEqual(TEXT("Stale apply preserves human change and all endpoints"), Inspect(), ChangedOutside);
    TestFalse(TEXT("Stale apply preserves package state"), Package->IsDirty());
    TestEqual(TEXT("Stale apply adds no undo history"), Buffer->GetQueueLength(), BaselineUndo);
    TextDefault->DefaultTextValue = Localized;

    const int32 BeforeSuccess = AppliedUndoCount();
    TestFalse(TEXT("Injected failure after live removal/write returns false"),
        FTurboMCPGraphReplace::ApplyInternal(LivePlan, Report, Error, true));
    AddInfo(Error);
    TestTrue(TEXT("Failure reports owned undo without claiming restoration"), Error.Contains(TEXT("owned undo applied; restoration is unverified")));
    TestFalse(TEXT("Failure leaves no active transaction"), GEditor->IsTransactionActive());
    TestTrue(TEXT("Failure never reports a changed result"), Report.IsEmpty());
    TestEqual(TEXT("Owned rollback restores graph nodes, positions, exact edges and defaults"), Inspect(), Original);
    TestEqual(TEXT("Failed live edit leaves no applied undo record"), AppliedUndoCount(), BeforeSuccess);

    if (!TestTrue(TEXT("Explicit one-node replacement applies"), FTurboMCPGraphReplace::Apply(LivePlan, Report, Error)))
    { AddError(Error); return false; }
    TestTrue(TEXT("Success reports one explicit deletion"), Report.Contains(TEXT("1 node(s) removed")) && Report.Contains(TargetGuid.ToString(EGuidFormats::DigitsLower)));
    TestFalse(TEXT("Only the exact requested node was deleted"), Graph->Nodes.ContainsByPredicate([&](const UEdGraphNode* Node)
        { return Node->NodeGuid == TargetGuid; }));
    TestEqual(TEXT("One replacement node added"), Graph->Nodes.Num(), 7);
    TestTrue(TEXT("All unrelated node identities survive"), Graph->Nodes.Contains(Event) && Graph->Nodes.Contains(Comment)
        && Graph->Nodes.Contains(Source) && Graph->Nodes.Contains(Consumer) && Graph->Nodes.Contains(TextSource) && Graph->Nodes.Contains(ObjectSource));
    UEdGraphPin* ConsumerThen = Consumer->FindPinChecked(TEXT("then"));
    if (!TestEqual(TEXT("Exactly one replacement wire"), ConsumerThen->LinkedTo.Num(), 1)) return false;
    UEdGraphPin* NewExec = ConsumerThen->LinkedTo[0];
    TestTrue(TEXT("Replacement wire is reciprocal and uses exact input"), NewExec->PinName == TEXT("execute")
        && NewExec->LinkedTo.Contains(ConsumerThen));
    TestEqual(TEXT("New literal read from live pin"), NewExec->GetOwningNode()->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("replacement")));
    TestEqual(TEXT("Existing linked and unlinked defaults are retained"), Consumer->FindPinChecked(TEXT("InString"))->DefaultValue,
        FString(TEXT("preserve \"quotes\"; and\nnewlines\\")));
    TestTrue(TEXT("Entry and unaffected surviving wire are intact"), Event->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Source->FindPinChecked(TEXT("execute")))
        && Source->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Consumer->FindPinChecked(TEXT("execute")))
        && TextSource->FindPinChecked(TEXT("ReturnValue"))->LinkedTo.Contains(Source->FindPinChecked(TEXT("InString"))));
    TestTrue(TEXT("Package is dirty after successful edit"), Package->IsDirty());
    TestEqual(TEXT("Exactly one applied undo record"), AppliedUndoCount(), BeforeSuccess + 1);
    TestTrue(TEXT("One undo restores the original graph"), GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo restores actual graph, endpoints, and all defaults"), Inspect(), Original);
    TestFalse(TEXT("Success and undo leave no active transaction"), GEditor->IsTransactionActive());
    return !HasAnyErrors();
}
#endif

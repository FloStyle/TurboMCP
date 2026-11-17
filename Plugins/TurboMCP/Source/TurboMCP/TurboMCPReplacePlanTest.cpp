// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPGraphReplace.h"
#include "TurboMCPGraphText.h"
#include "TurboMCPNodeType.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "Tools/TurboMCPToolsets.h"
#include "Editor/TransBuffer.h"
#include "BlueprintEditorSettings.h"
#include "ScopedTransaction.h"
#include "EdGraphNode_Comment.h"
#include "Kismet/KismetTextLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Tunnel.h"
#include "Editor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

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
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
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
        Node->PostPlacedNewNode();
        Node->NodePosX = X;
        return Node;
    };
    auto Print = [&](int32 X) { return Call(UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), X); };
    UK2Node_CallFunction* Source = Print(280);
    UK2Node_CallFunction* Consumer = Print(620);
    UK2Node_CallFunction* Target = Print(980);
    // Stock PrintString placement sets DevelopmentOnly. Undo would otherwise
    // normalize a manually created Enabled node during transaction loading.
    if (!TestTrue(TEXT("Stock placement enables each PrintString for development only"),
        Source->GetDesiredEnabledState() == ENodeEnabledState::DevelopmentOnly
        && Consumer->GetDesiredEnabledState() == ENodeEnabledState::DevelopmentOnly
        && Target->GetDesiredEnabledState() == ENodeEnabledState::DevelopmentOnly))
        return false;
    UK2Node_CallFunction* TextSource = Call(UKismetTextLibrary::StaticClass(), TEXT("Conv_TextToString"), 180);
    UK2Node_CallFunction* TextLiteral = Call(UKismetSystemLibrary::StaticClass(), TEXT("MakeLiteralText"), 0);
    UK2Node_CallFunction* ObjectSource = Call(UKismetSystemLibrary::StaticClass(), TEXT("IsValid"), 100);
    // The converter's const-reference input ignores its default. Use a real
    // editable by-value text pin, then wire its value into the converter.
    UEdGraphPin* TextDefault = TextLiteral->FindPinChecked(TEXT("Value"));
    if (!TestTrue(TEXT("Stock literal node exposes an editable text default"),
        TextDefault->PinType.PinCategory == UEdGraphSchema_K2::PC_Text && TextDefault->Direction == EGPD_Input
        && !TextDefault->bHidden && !TextDefault->bDefaultValueIsIgnored && !TextDefault->bDefaultValueIsReadOnly))
        return false;
    FString TextNamespace = TEXT("FixtureNamespace");
#if USE_STABLE_LOCALIZATION_KEYS
    TextNamespace = TextNamespaceUtil::BuildFullNamespace(TextNamespace, TextNamespaceUtil::EnsurePackageNamespace(Package), true);
#endif
    const FText Localized = FText::ChangeKey(*TextNamespace, TEXT("ExactKey"), FText::FromString(TEXT("shared display text")));
    TextDefault->DefaultTextValue = Localized;
    const FTextId InitialTextId = FTextInspector::GetTextId(TextDefault->DefaultTextValue);
    TestEqual(TEXT("Fixture text namespace is installed on the pin"), InitialTextId.GetNamespace().ToString(), TextNamespace);
    TestEqual(TEXT("Fixture text key is installed on the pin"), InitialTextId.GetKey().ToString(), FString(TEXT("ExactKey")));
    TestTrue(TEXT("Literal text feeds the conversion input"), Schema->TryCreateConnection(
        TextLiteral->FindPinChecked(TEXT("ReturnValue")), TextSource->FindPinChecked(TEXT("InText"))));
    ObjectSource->FindPinChecked(TEXT("Object"))->DefaultObject = GetMutableDefault<UKismetTextLibrary>();
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

    UEdGraph* ValidMacro = NewObject<UEdGraph>(Blueprint, TEXT("ReplaceFixtureValidMacro"), RF_Transactional);
    ValidMacro->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->MacroGraphs.Add(ValidMacro);
    Schema->CreateMacroGraphTerminators(*ValidMacro, nullptr);
    auto* ValidMetadata = UK2Node_MacroInstance::GetAssociatedGraphMetadata(ValidMacro);
    if (!TestNotNull(TEXT("Stock macro terminators provide entry metadata"), ValidMetadata)) return false;
    ValidMetadata->Category = FText::FromString(TEXT("ReplaceFixture"));

    // Normal editor placement of a custom event registers its skeleton function.
    // Without this step the first stock undo fills a delegate reference that was
    // missing from the initial graph, so the old fingerprint correctly refuses.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    auto ReadEventDelegate = [&]()
    {
        return Event->FindPinChecked(UK2Node_Event::DelegateOutputName)->PinType.PinSubCategoryMemberReference;
    };
    const FSimpleMemberReference EventDelegate = ReadEventDelegate();
    const FString DelegateOwner = GetPathNameSafe(EventDelegate.MemberParent.Get());
    if (!TestTrue(TEXT("Fixture event delegate is bound before taking its baseline"),
        Blueprint->SkeletonGeneratedClass
        && Blueprint->SkeletonGeneratedClass->FindFunctionByName(Event->CustomFunctionName)
        && EventDelegate.MemberName == Event->CustomFunctionName
        && EventDelegate.MemberParent.Get() == Blueprint->GeneratedClass.Get()))
        return false;
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
    UEdGraphPin* SourceThen = Source->FindPinChecked(TEXT("then"));
    UEdGraphPin* TargetExecute = Target->FindPinChecked(TEXT("execute"));
    SourceThen->MakeLinkTo(TargetExecute);
    const FString FanOutReason = Id(*Source) + TEXT(":then: execution output fan-out is inspection only");
    auto ExpectFanOutRefusal = [&]()
    {
        const FString Incomplete = Load();
        TestTrue(TEXT("Exec output fan-out export is incomplete and identifies the exact output"),
            Incomplete.Contains(TEXT("\"complete\":false")) && Incomplete.Contains(FanOutReason));
        FTurboMCPGraphReplace::FPlan Candidate;
        FString FanOutError;
        TestTrue(TEXT("Current fan-out graph refuses an earlier complete document"),
            FTurboMCPGraphReplace::Plan(Blueprint->GetPathName(), Graph->GetName(), Export, Candidate, FanOutError)
            && !Candidate.bAcceptable() && FTurboMCPGraphReplace::RenderPreview(Candidate).Contains(FanOutReason));
    };
    if (!TestEqual(TEXT("Fixture has both exec output destinations"), SourceThen->LinkedTo.Num(), 2)) return false;
    ExpectFanOutRefusal();
    SourceThen->LinkedTo.Swap(0, 1);
    ExpectFanOutRefusal();
    TestTrue(TEXT("Rejected Replace never reorders exec output destinations"),
        SourceThen->LinkedTo.Num() == 2 && SourceThen->LinkedTo[0] == TargetExecute);
    SourceThen->BreakLinkTo(TargetExecute, false);

    UEdGraphPin* EventThen = Event->FindPinChecked(TEXT("then"));
    UEdGraphPin* SourceExecute = Source->FindPinChecked(TEXT("execute"));
    UEdGraphPin* ConsumerExecute = Consumer->FindPinChecked(TEXT("execute"));
    EventThen->BreakLinkTo(SourceExecute, false);
    EventThen->MakeLinkTo(ConsumerExecute);
    TestEqual(TEXT("Fixture has a valid merge into one exec input"), ConsumerExecute->LinkedTo.Num(), 2);
    const FString Merge = Load();
    TestTrue(TEXT("Both incoming exec wires remain representable"),
        Merge.Contains(TEXT("\"complete\":true"))
        && Merge.Contains(TEXT("\"execute\" (-> ") + Id(*Event) + TEXT(":\"then\")"))
        && Merge.Contains(TEXT("\"execute\" (-> ") + Id(*Source) + TEXT(":\"then\")")));
    FTurboMCPGraphReplace::FPlan MergePlan;
    FString MergeError;
    TestTrue(TEXT("An exact exec merge export can be submitted unchanged"),
        FTurboMCPGraphReplace::Plan(Blueprint->GetPathName(), Graph->GetName(), Merge, MergePlan, MergeError)
        && MergePlan.bAcceptable());
    EventThen->BreakLinkTo(ConsumerExecute, false);
    EventThen->MakeLinkTo(SourceExecute);
    Package->SetDirtyFlag(false); // End of fixture setup.
    TestEqual(TEXT("Fixture restored exact initial semantic state"), Load(), Export);
    TestEqual(TEXT("Repeated export uses stable order and fingerprint"), Load(), Export);
    const FString LiteralFormTag = TEXT(" ; ") + Id(*TextLiteral) + TEXT("\n");
    const int32 LiteralForm = Export.Find(LiteralFormTag, ESearchCase::CaseSensitive);
    const int32 LiteralFormEnd = LiteralForm != INDEX_NONE
        ? Export.Find(TEXT("\n)\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LiteralForm) : INDEX_NONE;
    const FString ExactLocalizedLine = TEXT(" \"Value\" \"NSLOCTEXT(\\\"") + TextNamespace
        + TEXT("\\\", \\\"ExactKey\\\", \\\"shared display text\\\")\"");
    if (!TestTrue(TEXT("Exact localized pin form is present"), LiteralForm != INDEX_NONE && LiteralFormEnd != INDEX_NONE)) return false;
    const FString LiteralFormBody = Export.Mid(LiteralForm, LiteralFormEnd - LiteralForm);
    if (!TestTrue(TEXT("Localized default is rendered with its namespace and key on the exact text pin"),
        LiteralFormBody.Contains(ExactLocalizedLine, ESearchCase::CaseSensitive)))
        AddError(TEXT("Exported literal form: ") + LiteralFormBody);
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
            Records.Add(FString::Printf(TEXT("node %s %s %d,%d enabled=%d %s"), *Id(*Node), *Node->GetClass()->GetPathName(),
                Node->NodePosX, Node->NodePosY, int32(Node->GetDesiredEnabledState()), *Node->NodeComment));
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

    // Add disposable debugger attachments directly in memory so this fixture
    // never writes editor settings via the public debugger mutation commands.
    auto* DebuggerSettings = GetMutableDefault<UBlueprintEditorSettings>();
    const FString DebuggerPath = Blueprint->GetPathName();
    TestFalse(TEXT("Fixture starts with no debugger settings"), DebuggerSettings->PerBlueprintSettings.Contains(DebuggerPath));
    DebuggerSettings->PerBlueprintSettings.Add(DebuggerPath, FPerBlueprintSettings());
    auto& Attachments = DebuggerSettings->PerBlueprintSettings.FindChecked(DebuggerPath);
    ON_SCOPE_EXIT { DebuggerSettings->PerBlueprintSettings.Remove(DebuggerPath); };
    const auto* BreakpointNode = FindFProperty<FSoftObjectProperty>(FBlueprintBreakpoint::StaticStruct(), TEXT("Node"));
    if (!TestNotNull(TEXT("Breakpoint fixture node property"), BreakpointNode)) return false;
    FBlueprintBreakpoint& Breakpoint = Attachments.Breakpoints.AddDefaulted_GetRef();
    BreakpointNode->SetObjectPropertyValue_InContainer(&Breakpoint, Target);
    if (!TestTrue(TEXT("Fixture attaches a breakpoint to exact removal target"), Breakpoint.GetLocation() == Target)) return false;
    Refused(TEXT("Deletion with a breakpoint refused"), NewBody, TEXT("Blueprint breakpoint"));
    TestTrue(TEXT("Breakpoint record survives refusal"), Attachments.Breakpoints.Num() == 1 && Breakpoint.GetLocation() == Target);
    Attachments.Breakpoints.Reset();

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

    // One applied record and one unapplied record expose both redo pruning and
    // empty transaction churn. Keep them independent of the edited graph.
    // A sibling graph of this Blueprint is not independent. The stock
    // Blueprint undo client walks the transacted object's outers and
    // rebuilds that Blueprint's skeleton. Use a graph with no Blueprint outer.
    const TStrongObjectPtr<UEdGraph> HistoryGraph(NewObject<UEdGraph>(GetTransientPackage(), NAME_None, RF_Transactional));
    HistoryGraph->Schema = UEdGraphSchema_K2::StaticClass();
    UEdGraphNode_Comment* HistoryWitness = NewObject<UEdGraphNode_Comment>(HistoryGraph.Get(), NAME_None, RF_Transactional);
    HistoryGraph->AddNode(HistoryWitness, false, false);
    HistoryWitness->CreateNewGuid();
    {
        const FScopedTransaction Record(NSLOCTEXT("TurboMCP", "ReplaceFixtureUndo", "Replace fixture undo record"));
        HistoryWitness->Modify();
        HistoryWitness->NodeComment = TEXT("first record");
    }
    {
        const FScopedTransaction Record(NSLOCTEXT("TurboMCP", "ReplaceFixtureRedo", "Replace fixture redo record"));
        HistoryWitness->Modify();
        HistoryWitness->NodeComment = TEXT("second record");
    }
    if (!TestTrue(TEXT("Fixture establishes a redo record"), Buffer->Undo(true))) return false;
    TestEqual(TEXT("Undo produces the first record's actual state"), HistoryWitness->NodeComment, FString(TEXT("first record")));
    if (!TestTrue(TEXT("Fixture has existing undo and redo entries"), Buffer->GetUndoCount() > 0 && AppliedUndoCount() > 0)) return false;
    TestFalse(TEXT("Independent undo fixture leaves the target package clean"), Package->IsDirty());
    TestEqual(TEXT("Independent undo fixture leaves target nodes, edges and defaults unchanged"), Inspect(), Original);
    const FString AfterHistorySetup = Load();
    if (!TestTrue(TEXT("Independent undo fixture preserves the original edit export"),
        AfterHistorySetup.Equals(Export, ESearchCase::CaseSensitive)))
    {
        FString BeforeHeader, AfterHeader;
        Export.Split(TEXT("\n"), &BeforeHeader, nullptr);
        AfterHistorySetup.Split(TEXT("\n"), &AfterHeader, nullptr);
        AddError(TEXT("Target edit header before history setup: ") + BeforeHeader);
        AddError(TEXT("Target edit header after history setup: ") + AfterHeader);
        return false;
    }
    auto ReadHistory = [&]()
    {
        TArray<FGuid> Ids;
        for (int32 Index = 0; Index < Buffer->GetQueueLength(); ++Index)
        {
            const FTransaction* Record = Buffer->GetTransaction(Index);
            Ids.Add(Record ? Record->GetId() : FGuid());
        }
        return Ids;
    };
    const TArray<FGuid> HistoryBefore = ReadHistory();
    const int32 RedoBefore = Buffer->GetUndoCount();
    auto ExpectHistoryUnchanged = [&]()
    {
        TestTrue(TEXT("Global undo and redo retain every existing transaction ID"), ReadHistory() == HistoryBefore);
        TestEqual(TEXT("Redo cursor stays at the same record"), Buffer->GetUndoCount(), RedoBefore);
        TestFalse(TEXT("No editor transaction remains active"), GEditor->IsTransactionActive());
        TestEqual(TEXT("Unrelated undo witness is untouched"), HistoryWitness->NodeComment, FString(TEXT("first record")));
    };

    // Publish the malformed macro before resolving the new PrintString in
    // Replace. Remove only this fixture's actions before deferred stock priming.
    FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
    ON_SCOPE_EXIT { Database.ClearAssetActions(Blueprint); };
    Database.RefreshAssetActions(Blueprint);
    const auto* FixtureActions = Database.GetAllActions().Find(FObjectKey(Blueprint));
    if (!TestTrue(TEXT("Malformed macro has a registered stock action"), FixtureActions
        && FixtureActions->ContainsByPredicate([&](const UBlueprintNodeSpawner* Spawner)
        {
            if (!Spawner || Spawner->NodeClass.Get() != UK2Node_MacroInstance::StaticClass()) return false;
            const auto* Instance = Cast<UK2Node_MacroInstance>(Spawner->GetTemplateNode());
            return Instance && Instance->GetMacroGraph() == Macro
                && !UK2Node_MacroInstance::GetAssociatedGraphMetadata(Macro);
        })))
        return false;

    FTurboMCPGraphReplace::FPlan LivePlan;
    {
        int32 HistoryEvents = 0;
        auto Handle = Buffer->OnTransactionStateChanged().AddLambda(
            [&](const FTransactionContext&, ETransactionStateEventType) { ++HistoryEvents; });
        ON_SCOPE_EXIT { Buffer->OnTransactionStateChanged().Remove(Handle); };
        if (!TestTrue(TEXT("Explicit one-node removal and addition plan before editing"), Plan(NewBody, LivePlan) && LivePlan.bAcceptable()))
        { AddError(Error + FTurboMCPGraphReplace::RenderPreview(LivePlan)); return false; }
        const TArray<FString> TypeIds = FTurboMCPNodeType::AvailableTypeIds(Blueprint, Graph);
        TestTrue(TEXT("Valid local stock macro keeps its exact discovery ID"),
            TypeIds.Contains(TEXT("ReplaceFixture|ReplaceFixtureValidMacro")));
        TestFalse(TEXT("Malformed macro is absent from discovery"), TypeIds.ContainsByPredicate([](const FString& TypeId)
            { return TypeId.EndsWith(TEXT("|ProtectedMacro"), ESearchCase::CaseSensitive); }));
        ExpectHistoryUnchanged();

        FString Rewire = NewBody;
        const FString OldLine = TEXT("\"InString\" (-> ") + Id(*TextSource) + TEXT(":\"ReturnValue\")");
        if (!TestEqual(TEXT("Find exact input for scratch-only disconnect"), Rewire.ReplaceInline(
            *OldLine, TEXT("\"InString\" \"staged default\""), ESearchCase::CaseSensitive), 1)) return false;
        FTurboMCPGraphReplace::FPlan RewirePlan;
        if (!TestTrue(TEXT("A supported scratch edit breaks an existing retained wire"), Plan(Rewire, RewirePlan) && RewirePlan.bAcceptable()))
        { AddError(Error + FTurboMCPGraphReplace::RenderPreview(RewirePlan)); return false; }
        ExpectHistoryUnchanged();

        FTurboMCPGraphReplace::FPlan FailingScratch;
        TestTrue(TEXT("A later scratch failure rejects a disconnected execution node"),
            Plan(Rewire + TEXT("(Development|PrintString\n InString \"unreachable\"\n)\n"), FailingScratch)
            && !FailingScratch.bAcceptable()
            && FTurboMCPGraphReplace::RenderPreview(FailingScratch).Contains(TEXT("no path from a recognized entry")));
        ExpectHistoryUnchanged();
        TestEqual(TEXT("Neither successful nor failed scratch execution emits transaction events"), HistoryEvents, 0);
    }
    TestEqual(TEXT("Scratch execution preserves original graph exactly"), Inspect(), Original);
    TestFalse(TEXT("Scratch execution leaves package clean"), Package->IsDirty());

    // A property watch added after planning leaves the graph fingerprint
    // unchanged. Apply must still refuse before opening an undo transaction.
    UEdGraphPin* Watched = Target->FindPinChecked(TEXT("InString"));
    const TArray<FName> WatchPath{TEXT("NestedFixtureProperty")};
    Attachments.WatchedPins.Emplace(Watched, TArray<FName>(WatchPath));
    TestFalse(TEXT("Whole-pin watch query alone would miss the nested property"),
        FKismetDebugUtilities::IsPinBeingWatched(Blueprint, Watched));
    TestTrue(TEXT("Fixture adds a nested property watch"),
        FKismetDebugUtilities::IsPinBeingWatched(Blueprint, Watched, WatchPath));
    TestFalse(TEXT("Apply refuses a watch added since planning"), FTurboMCPGraphReplace::Apply(LivePlan, Report, Error));
    TestTrue(TEXT("Apply names the debugger attachment"), Error.Contains(TEXT("Blueprint pin watch: InString")));
    TestTrue(TEXT("Nested debugger watch survives refusal"),
        Attachments.WatchedPins.Num() == 1 && FKismetDebugUtilities::IsPinBeingWatched(Blueprint, Watched, WatchPath));
    TestEqual(TEXT("Debugger refusal preserves the graph"), Inspect(), Original);
    TestFalse(TEXT("Debugger refusal preserves package state"), Package->IsDirty());
    ExpectHistoryUnchanged();
    DebuggerSettings->PerBlueprintSettings.Remove(DebuggerPath);

    // Change only the localization identity, while leaving displayed text and
    // the node/pin count unchanged. A stored plan must fail its live SHA-256 check.
    TextDefault->DefaultTextValue = FText::ChangeKey(*TextNamespace, TEXT("EditedOutsidePlan"), Localized);
    TestEqual(TEXT("Localization-only edit retains the display string"), TextDefault->DefaultTextValue.ToString(), Localized.ToString());
    TestEqual(TEXT("Localization-only edit changes the exact key"),
        FTextInspector::GetTextId(TextDefault->DefaultTextValue).GetKey().ToString(), FString(TEXT("EditedOutsidePlan")));
    const FString ChangedOutside = Inspect();
    TestFalse(TEXT("Apply refuses semantic change since planning"), FTurboMCPGraphReplace::Apply(LivePlan, Report, Error));
    TestTrue(TEXT("Apply reports stale graph"), Error.Contains(TEXT("stale")));
    TestEqual(TEXT("Stale apply preserves human change and all endpoints"), Inspect(), ChangedOutside);
    TestFalse(TEXT("Stale apply preserves package state"), Package->IsDirty());
    ExpectHistoryUnchanged();
    TextDefault->DefaultTextValue = Localized;

    // Diagnostic records use the real fingerprint fields. The independent
    // graph oracle above still determines whether authored state was restored.
    auto FingerprintRecords = [&]()
    {
        TMap<FString, FString> Records;
        Records.Add(TEXT("Blueprint parent"), GetPathNameSafe(Blueprint->ParentClass));
        Records.Add(TEXT("Graph schema"), GetPathNameSafe(Graph->GetSchema()->GetClass()));
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const FString Key = Node->GetName() + TEXT(" [") + Id(*Node) + TEXT("]");
            Records.Add(Key, FTurboMCPGraphText::EditNodeState(*Node));
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                const FString PinKey = Key + TEXT(".") + Pin->PinName.ToString() + TEXT(" [") + Pin->PinId.ToString() + TEXT("]");
                Records.Add(PinKey + TEXT(" shape"), FTurboMCPGraphText::EditPinShape(*Pin));
                Records.Add(PinKey + TEXT(" default"), FTurboMCPGraphText::EditDefaultState(*Pin));
            }
        }
        return Records;
    };
    const auto RecordsBeforeRollback = FingerprintRecords();
    const int32 BeforeSuccess = AppliedUndoCount();
    TestFalse(TEXT("Injected failure after live removal/write returns false"),
        FTurboMCPGraphReplace::ApplyInternal(LivePlan, Report, Error, true));
    AddInfo(Error);
    TestTrue(TEXT("Failure reports owned undo without claiming restoration"), Error.Contains(TEXT("owned undo applied; restoration is unverified")));
    TestFalse(TEXT("Failure leaves no active transaction"), GEditor->IsTransactionActive());
    TestTrue(TEXT("Failure never reports a changed result"), Report.IsEmpty());
    TestEqual(TEXT("Owned rollback restores graph nodes, positions, exact edges and defaults"), Inspect(), Original);
    TestEqual(TEXT("Failed live edit leaves no applied undo record"), AppliedUndoCount(), BeforeSuccess);
    const FSimpleMemberReference RestoredDelegate = ReadEventDelegate();
    TestEqual(TEXT("Owned rollback preserves the delegate member reference"),
        GetPathNameSafe(RestoredDelegate.MemberParent.Get()) + TEXT(".") + RestoredDelegate.MemberName.ToString()
            + TEXT(":") + RestoredDelegate.MemberGuid.ToString(),
        DelegateOwner + TEXT(".") + EventDelegate.MemberName.ToString() + TEXT(":") + EventDelegate.MemberGuid.ToString());
    const FString RestoredExport = Load();
    if (!TestTrue(TEXT("Owned rollback restores the exact edit export including its fingerprint"),
        RestoredExport.Equals(Export, ESearchCase::CaseSensitive)))
    {
        FString BeforeHeader, AfterHeader;
        Export.Split(TEXT("\n"), &BeforeHeader, nullptr);
        RestoredExport.Split(TEXT("\n"), &AfterHeader, nullptr);
        AddError(TEXT("Edit header before owned rollback: ") + BeforeHeader);
        AddError(TEXT("Edit header after owned rollback: ") + AfterHeader);
        const auto RecordsAfterRollback = FingerprintRecords();
        TArray<FString> Keys;
        RecordsBeforeRollback.GetKeys(Keys);
        for (const auto& Record : RecordsAfterRollback) Keys.AddUnique(Record.Key);
        Keys.Sort();
        for (const FString& Key : Keys)
        {
            const FString Before = RecordsBeforeRollback.FindRef(Key);
            const FString After = RecordsAfterRollback.FindRef(Key);
            if (Before.Equals(After, ESearchCase::CaseSensitive)) continue;
            int32 Offset = 0;
            while (Offset < FMath::Min(Before.Len(), After.Len()) && Before[Offset] == After[Offset]) ++Offset;
            const int32 Start = FMath::Max(0, Offset - 30);
            AddError(FString::Printf(TEXT("Fingerprint record %s differs at %d. Before [%s] after [%s]"),
                *Key, Offset, *Before.Mid(Start, 120), *After.Mid(Start, 120)));
        }
    }

    // Stock undo marks the package dirty. Start the independent successful
    // edit with a clean package after checking the state restored above.
    Package->SetDirtyFlag(false);
    if (!TestTrue(TEXT("Explicit one-node replacement applies"), FTurboMCPGraphReplace::Apply(LivePlan, Report, Error)))
    { AddError(Error); return false; }
    TestTrue(TEXT("Success reports one explicit deletion"), Report.Contains(TEXT("1 node(s) removed")) && Report.Contains(TargetGuid.ToString(EGuidFormats::DigitsLower)));
    TestFalse(TEXT("Only the exact requested node was deleted"), Graph->Nodes.ContainsByPredicate([&](const UEdGraphNode* Node)
        { return Node->NodeGuid == TargetGuid; }));
    TestEqual(TEXT("One replacement node added"), Graph->Nodes.Num(), 8);
    TestTrue(TEXT("All unrelated node identities survive"), Graph->Nodes.Contains(Event) && Graph->Nodes.Contains(Comment)
        && Graph->Nodes.Contains(Source) && Graph->Nodes.Contains(Consumer) && Graph->Nodes.Contains(TextSource) && Graph->Nodes.Contains(TextLiteral)
        && Graph->Nodes.Contains(ObjectSource));
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
    TestEqual(TEXT("Live edit preserves the localized text key"),
        FTextInspector::GetTextId(TextLiteral->FindPinChecked(TEXT("Value"))->DefaultTextValue).GetKey().ToString(), FString(TEXT("ExactKey")));
    TestTrue(TEXT("Package is dirty after successful edit"), Package->IsDirty());
    TestEqual(TEXT("Exactly one applied undo record"), AppliedUndoCount(), BeforeSuccess + 1);
    TestTrue(TEXT("One undo restores the original graph"), GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo restores actual graph, endpoints, and all defaults"), Inspect(), Original);
    TestFalse(TEXT("Success and undo leave no active transaction"), GEditor->IsTransactionActive());
    return !HasAnyErrors();
}
#endif

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "TurboMCPNodeId.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

// Similar nodes in independent event flows deliberately prevent title-only
// selection. This never-saved fixture records real tool output for prose review.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPEventConnectWorkflowTest,
    "TurboMCP.Workflow.EventConnect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPEventConnectWorkflowTest::RunTest(const FString& Parameters)
{
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/EventConnect_") + Suffix));
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_EventConnectReview"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("EventConnectReview"));
    if (!TestNotNull(TEXT("Fixture created"), BP)) return false;
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/Workflow"), Suffix);
        IFileManager::Get().MakeDirectory(*Folder, true);
        FFileHelper::SaveStringToFile(Transcript, *FPaths::Combine(Folder, TEXT("event-connect-review.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        BP->ClearFlags(RF_Public | RF_Standalone); BP->MarkAsGarbage();
        Package->SetDirtyFlag(false); Package->MarkAsGarbage(); // Only this disposable package.
    };
    auto Record = [&](const TCHAR* Tool, const FString& Text)
    { Transcript += FString::Printf(TEXT("\n=== %s ===\n%s\n"), Tool, *Text); return Text; };
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("PurchaseEvents"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
    UEdGraph* OtherGraph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("AuditEvents"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, OtherGraph);
    FEdGraphPinType Integer; Integer.PinCategory = UEdGraphSchema_K2::PC_Int;
    for (const TCHAR* Name : {TEXT("PurchaseValue"), TEXT("RefundValue"), TEXT("AuditValue")})
        TestTrue(TEXT("Fixture member added"), FBlueprintEditorUtils::AddMemberVariable(BP, Name, Integer, TEXT("0")));
    FKismetEditorUtilities::CompileBlueprint(BP);
    auto AddCall = [&](UEdGraph* G, int32 X, int32 Y, int32 A, int32 B)
    {
        UK2Node_CallFunction* N = NewObject<UK2Node_CallFunction>(G, NAME_None, RF_Transactional);
        N->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")));
        G->AddNode(N, false, false); N->CreateNewGuid(); N->PostPlacedNewNode(); N->AllocateDefaultPins();
        N->NodePosX = X; N->NodePosY = Y;
        N->FindPinChecked(TEXT("A"))->DefaultValue = FString::FromInt(A);
        N->FindPinChecked(TEXT("B"))->DefaultValue = FString::FromInt(B);
        return N;
    };
    auto AddFlow = [&](UEdGraph* G, const TCHAR* EventName, const TCHAR* Member, int32 Y, int32 A, int32 B)
    {
        UK2Node_CustomEvent* Event = NewObject<UK2Node_CustomEvent>(G, NAME_None, RF_Transactional);
        Event->CustomFunctionName = EventName; G->AddNode(Event, false, false); Event->CreateNewGuid(); Event->PostPlacedNewNode(); Event->AllocateDefaultPins(); Event->NodePosY = Y;
        UK2Node_VariableSet* Set = NewObject<UK2Node_VariableSet>(G, NAME_None, RF_Transactional);
        Set->VariableReference.SetSelfMember(Member); G->AddNode(Set, false, false); Set->CreateNewGuid(); Set->PostPlacedNewNode(); Set->AllocateDefaultPins(); Set->NodePosX = 700; Set->NodePosY = Y;
        UK2Node_CallFunction* Value = AddCall(G, 350, Y + 140, A, B);
        TestTrue(TEXT("Fixture execution connected"), G->GetSchema()->TryCreateConnection(Event->FindPinChecked(TEXT("then")), Set->FindPinChecked(TEXT("execute"))));
        TestTrue(TEXT("Fixture value connected"), G->GetSchema()->TryCreateConnection(Value->FindPinChecked(TEXT("ReturnValue")), Set->FindPinChecked(Member)));
        return Set;
    };
    UK2Node_VariableSet* Purchase = AddFlow(Graph, TEXT("Purchase"), TEXT("PurchaseValue"), 0, 1, 2);
    AddFlow(Graph, TEXT("Refund"), TEXT("RefundValue"), 600, 10, 20);
    UK2Node_VariableSet* Audit = AddFlow(OtherGraph, TEXT("Audit"), TEXT("AuditValue"), 0, 100, 200);
    UK2Node_CallFunction* Correct = AddCall(Graph, 350, 300, 40, 2);
    const FString Asset = BP->GetPathName(), GraphName = Graph->GetName();
    const FString SourceId = TurboMCPNodeId::MakeForNode(Correct, true), TargetId = TurboMCPNodeId::MakeForNode(Purchase, true);
    UEdGraphPin* TargetPin = Purchase->FindPinChecked(TEXT("PurchaseValue"));
    UEdGraphPin* OldSource = TargetPin->LinkedTo[0];
    UEdGraphPin* NewSource = Correct->FindPinChecked(TEXT("ReturnValue"));
    UEdGraphNode* Obsolete = OldSource->GetOwningNode();
    const FString ObsoleteId = TurboMCPNodeId::MakeForNode(Obsolete, true);
    auto Snapshot = [&](UEdGraphNode* Excluded = nullptr)
    {
        TArray<FString> Rows;
        for (UEdGraph* G : {Graph, OtherGraph}) for (UEdGraphNode* N : G->Nodes)
        {
            if (N == Excluded) continue;
            Rows.Add(G->GetName() + N->NodeGuid.ToString() + FString::Printf(TEXT(":%d,%d"), N->NodePosX, N->NodePosY));
            for (UEdGraphPin* P : N->Pins) if (P)
            {
                const FString Key = N->NodeGuid.ToString() + P->PinName.ToString();
                Rows.Add(Key + P->DefaultValue + P->DefaultTextValue.ToString() + GetPathNameSafe(P->DefaultObject));
                for (UEdGraphPin* Q : P->LinkedTo) Rows.Add(Key + TEXT("->") + Q->GetOwningNode()->NodeGuid.ToString() + Q->PinName.ToString());
            }
        }
        Rows.Sort(); return FString::Join(Rows, TEXT("\n"));
    };
    // An isolated Create Event node proves stored binding inspection without executing it.
    UK2Node_CreateDelegate* Delegate = NewObject<UK2Node_CreateDelegate>(Graph);
    Graph->AddNode(Delegate, false, false); Delegate->CreateNewGuid(); Delegate->AllocateDefaultPins();
    Delegate->SetFunction(TEXT("Purchase"));
    const FString BindingBefore = Snapshot();
    const bool BindingDirtyBefore = Package->IsDirty();
    const FString DelegateId = TurboMCPNodeId::MakeForNode(Delegate, true);
    const FString BindingGraph = Record(TEXT("GraphLoad delegate binding"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, GraphName, TEXT("read"), TEXT("full")));
    const FString BindingContext = Record(TEXT("NodeContext delegate binding"), UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, DelegateId, 0));
    TestTrue(TEXT("Graph reader exposes configured delegate function"), BindingGraph.Contains(TEXT("configured delegate function: Purchase")));
    TestTrue(TEXT("Context exposes configured function and runtime limit"), BindingContext.Contains(TEXT("configured delegate function: Purchase")) && BindingContext.Contains(TEXT("runtime target not verified")));
    TestEqual(TEXT("Binding inspection leaves nodes and pins unchanged"), Snapshot(), BindingBefore);
    TestEqual(TEXT("Binding inspection preserves dirty flag"), Package->IsDirty(), BindingDirtyBefore);
    Delegate->SetFunction(NAME_None);
    TestTrue(TEXT("Unbound delegate is explicit"), UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, DelegateId, 0).Contains(TEXT("none (unbound)")));
    Graph->RemoveNode(Delegate); Delegate->MarkAsGarbage();
    const FString Before = Snapshot(); const bool DirtyBefore = Package->IsDirty();
    const FString Overview = Record(TEXT("ReadTurboMCPBlueprintLoad"), UTurboMCPReadToolset::ReadTurboMCPBlueprintLoad(Asset, TEXT("signatures"), TEXT(""), TEXT("read"), TEXT("60000")));
    TestTrue(TEXT("Overview identifies both graphs"), Overview.Contains(GraphName) && Overview.Contains(OtherGraph->GetName()));
    const FString Read = Record(TEXT("ReadTurboMCPGraphLoad"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, GraphName, TEXT("read"), TEXT("full")));
    TestTrue(TEXT("Exact reader exposes both event roots"), Read.Contains(TEXT("Purchase")) && Read.Contains(TEXT("Refund")));
    const FString Found = Record(TEXT("ReadTurboMCPNodeFind"), UTurboMCPReadToolset::ReadTurboMCPNodeFind(Asset, TEXT("Add"), TEXT("")));
    TestTrue(TEXT("Search reports the other graph as well"), Found.Contains(OtherGraph->GetName()));
    const FString Context = Record(TEXT("ReadTurboMCPNodeContext"), UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, TargetId, 3));
    TestTrue(TEXT("Target context identifies Purchase event"), Context.Contains(TEXT("AddEvent|Custom|Purchase")) && !Context.Contains(TEXT("AddEvent|Custom|Refund")));
    const FString SourceContext = Record(TEXT("ReadTurboMCPNodeContext source"), UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, SourceId, 3));
    TestTrue(TEXT("Unconnected candidate source has inspectable values"), SourceContext.Contains(TEXT("default=40")) && SourceContext.Contains(TEXT("default=2")));
    TestTrue(TEXT("IDs used by edit exist in reader/search output"), Read.Contains(TargetId) && Found.Contains(SourceId));
    const FString ShortKey = Purchase->NodeGuid.ToString(EGuidFormats::DigitsLower).Left(6);
    TestEqual(TEXT("Graph-local short key returns the same context as full ID"),
        UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, ShortKey, 3), Context);
    TestTrue(TEXT("Qualified ID from another graph stays refused"),
        UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, TurboMCPNodeId::MakeForNode(Audit, true), 3).StartsWith(TEXT("error:")));
    const FGuid SavedGuid = Correct->NodeGuid;
    Correct->NodeGuid = Purchase->NodeGuid;
    TestTrue(TEXT("Ambiguous bare key is refused instead of selecting a node"),
        UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, ShortKey, 3).Contains(TEXT("ambiguous")));
    Correct->NodeGuid = SavedGuid;
    TestEqual(TEXT("Inspection preserves fixture"), Snapshot(), Before);
    for (int32 Case = 0; Case < 3; ++Case)
        for (bool Edit : {false, true})
        {
            const FString BadSource = Case == 0 ? TurboMCPNodeId::MakeForNode(Audit->FindPinChecked(TEXT("AuditValue"))->LinkedTo[0]->GetOwningNode(), true) : SourceId;
            const FString BadPin = Case == 1 ? TEXT("MissingPin") : TEXT("ReturnValue");
            const FString BadTarget = Case == 2 ? TEXT("execute") : TEXT("PurchaseValue");
            const FString Refusal = Edit ? UTurboMCPEditToolset::EditTurboMCPGraphConnect(Asset, GraphName, BadSource, BadPin, TargetId, BadTarget)
                : UTurboMCPPreviewToolset::PreviewTurboMCPGraphConnect(Asset, GraphName, BadSource, BadPin, TargetId, BadTarget);
            Record(TEXT("GraphConnect refusal"), Refusal);
            TestTrue(TEXT("Invalid connection explicitly refused"), Refusal.StartsWith(TEXT("unchanged\nrefused:")));
            TestEqual(TEXT("Refusal preserves every flow"), Snapshot(), Before);
            TestEqual(TEXT("Refusal preserves dirty flag"), Package->IsDirty(), DirtyBefore);
        }
    const FString SourceKey = Correct->NodeGuid.ToString(EGuidFormats::DigitsLower).Left(6);
    const FString ShortPreview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphConnect(Asset, GraphName, SourceKey, TEXT("ReturnValue"), ShortKey, TEXT("PurchaseValue"));
    TestTrue(TEXT("Unique keys preview canonical endpoints"), ShortPreview.Contains(SourceId) && ShortPreview.Contains(TargetId) && !ShortPreview.Contains(TEXT("refused:")));
    TestEqual(TEXT("Short-key preview preserves graph"), Snapshot(), Before);
    const FGuid OriginalObsoleteGuid = Obsolete->NodeGuid;
    Obsolete->NodeGuid = Correct->NodeGuid;
    const FString AmbiguousBefore = Snapshot();
    for (bool Edit : {false, true})
    {
        const FString Result = Edit
            ? UTurboMCPEditToolset::EditTurboMCPGraphConnect(Asset, GraphName, SourceKey, TEXT("ReturnValue"), ShortKey, TEXT("PurchaseValue"))
            : UTurboMCPPreviewToolset::PreviewTurboMCPGraphConnect(Asset, GraphName, SourceKey, TEXT("ReturnValue"), ShortKey, TEXT("PurchaseValue"));
        TestTrue(TEXT("Ambiguous connect key is explicitly refused"), Result.Contains(TEXT("ambiguous")));
        TestEqual(TEXT("Ambiguous connect preserves every link"), Snapshot(), AmbiguousBefore);
        TestEqual(TEXT("Ambiguous connect preserves dirty flag"), Package->IsDirty(), DirtyBefore);
    }
    Obsolete->NodeGuid = OriginalObsoleteGuid;
    // A source still used elsewhere is not a cleanup candidate. This temporary
    // fixture variant gives it a second consumer, then restores the baseline.
    UEdGraphPin* SharedInput = Correct->FindPinChecked(TEXT("A"));
    OldSource->MakeLinkTo(SharedInput);
    const FString SharedBefore = Snapshot();
    const FString SharedPreview = Record(TEXT("PreviewTurboMCPGraphConnect shared source"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphConnect(Asset, GraphName, SourceId, TEXT("ReturnValue"), TargetId, TEXT("PurchaseValue")));
    TestFalse(TEXT("Shared source is not proposed for cleanup"), SharedPreview.Contains(TEXT("cleanup review:")));
    TestEqual(TEXT("Shared preview preserves all flows"), Snapshot(), SharedBefore);
    OldSource->BreakLinkTo(SharedInput);
    TestEqual(TEXT("Shared fixture variant restored"), Snapshot(), Before);
    const FString Preview = Record(TEXT("PreviewTurboMCPGraphConnect"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphConnect(Asset, GraphName, SourceId, TEXT("ReturnValue"), TargetId, TEXT("PurchaseValue")));
    TestTrue(TEXT("Preview identifies endpoints and displaced source"), Preview.Contains(SourceId) && Preview.Contains(TargetId) && Preview.Contains(TurboMCPNodeId::MakeForNode(OldSource->GetOwningNode(), true)));
    TestTrue(TEXT("Preview identifies orphan cleanup"), Preview.Contains(TEXT("cleanup review:")) && Preview.Contains(ObsoleteId));
    TestTrue(TEXT("Preview explicitly transient"), Preview.Contains(TEXT("preview (transient graph)")));
    TestFalse(TEXT("Connect preview does not imply new node IDs"), Preview.Contains(TEXT("New node IDs")));
    TestEqual(TEXT("Preview preserves all state"), Snapshot(), Before);
    const FString Applied = Record(TEXT("EditTurboMCPGraphConnect"), UTurboMCPEditToolset::EditTurboMCPGraphConnect(Asset, GraphName, SourceKey, TEXT("ReturnValue"), ShortKey, TEXT("PurchaseValue")));
    TestTrue(TEXT("Edit exposes cleanup instead of silently deleting"), Applied.Contains(TEXT("cleanup review:")) && Graph->Nodes.Contains(Obsolete));
    TestTrue(TEXT("Edit reports changed"), Applied.StartsWith(TEXT("changed\n")));
    TestTrue(TEXT("Edit identifies endpoints"), Applied.Contains(SourceId) && Applied.Contains(TargetId));
    TestTrue(TEXT("Only intended input now uses correct source"), TargetPin->LinkedTo.Num() == 1 && TargetPin->LinkedTo[0] == NewSource);
    // Normalize only the expected edge for a complete independent before/after comparison.
    TargetPin->BreakLinkTo(NewSource); TargetPin->MakeLinkTo(OldSource);
    TestEqual(TEXT("Every other node, position, default and edge survives"), Snapshot(), Before);
    TargetPin->BreakLinkTo(OldSource); TargetPin->MakeLinkTo(NewSource);
    const FString After = Snapshot();
    const FString Again = Record(TEXT("EditTurboMCPGraphConnect repeated"), UTurboMCPEditToolset::EditTurboMCPGraphConnect(Asset, GraphName, SourceId, TEXT("ReturnValue"), TargetId, TEXT("PurchaseValue")));
    TestTrue(TEXT("Repeated connection is a no-op"), Again.StartsWith(TEXT("unchanged\nno edit needed")));
    TestEqual(TEXT("No-op preserves edited state"), Snapshot(), After);
    // Cleanup is an explicit second operation, not an invisible side effect of
    // reconnect. The fixture's obsolete constant addition has no dependencies.
    TestTrue(TEXT("Obsolete calculation is now completely disconnected"),
        !Obsolete->Pins.ContainsByPredicate([](const UEdGraphPin* P) { return P && !P->LinkedTo.IsEmpty(); }));
    const FString RemoveNodes = FString::Printf(TEXT("[\"%s\"]"), *ObsoleteId);
    const FString RemovePreview = Record(TEXT("PreviewTurboMCPGraphRemove"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphRemove(Asset, GraphName, RemoveNodes, TEXT("[]")));
    TestTrue(TEXT("Cleanup preview names exactly one node"), RemovePreview.Contains(ObsoleteId) && RemovePreview.Contains(TEXT("nodes=1")));
    TestFalse(TEXT("Cleanup preview does not imply new IDs"), RemovePreview.Contains(TEXT("New node IDs")));
    TestEqual(TEXT("Cleanup preview leaves live graph untouched"), Snapshot(), After);
    const FString ExpectedClean = Snapshot(Obsolete);
    const int32 BeforeCleanupCount = Graph->Nodes.Num();
    const FString Removed = Record(TEXT("EditTurboMCPGraphRemove"), UTurboMCPEditToolset::EditTurboMCPGraphRemove(Asset, GraphName, RemoveNodes, TEXT("[]")));
    TestTrue(TEXT("Cleanup reports changed"), Removed.StartsWith(TEXT("changed\n")));
    TestEqual(TEXT("Exactly the obsolete node removed"), Graph->Nodes.Num(), BeforeCleanupCount - 1);
    TestFalse(TEXT("Obsolete node is gone"), Graph->Nodes.Contains(Obsolete));
    TestEqual(TEXT("Cleanup preserves every other node and flow"), Snapshot(), ExpectedClean);
    const FString MissingRemove = Record(TEXT("EditTurboMCPGraphRemove missing"), UTurboMCPEditToolset::EditTurboMCPGraphRemove(Asset, GraphName, RemoveNodes, TEXT("[]")));
    TestTrue(TEXT("Repeated removal refuses missing node"), MissingRemove.StartsWith(TEXT("unchanged\nrefused:")));
    TestEqual(TEXT("Repeated cleanup preserves all state"), Snapshot(), ExpectedClean);
    // Insert a false guard; only its else output must continue the original Purchase flow.
    UK2Node_IfThenElse* BranchProbe = NewObject<UK2Node_IfThenElse>(Graph);
    BranchProbe->AllocateDefaultPins();
    const FString BranchType = TurboMCPNodeId::GetNodeTypeId(BranchProbe);
    BranchProbe->MarkAsGarbage();
    UEdGraphPin* PurchaseExec = Purchase->FindPinChecked(TEXT("execute"));
    UEdGraphPin* EntryExec = PurchaseExec->LinkedTo[0];
    const FString GuardText = FString::Printf(TEXT("(%s\n execute (-> %s:%s)\n Condition false\n) ; label Guard"),
        *BranchType, *TurboMCPNodeId::MakeForNode(EntryExec->GetOwningNode(), true), *EntryExec->PinName.ToString());
    const FString GuardBefore = Snapshot();
    const bool GuardDirtyBefore = Package->IsDirty();
    const FString BadGuard = Record(TEXT("Guard invalid exit preview"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Asset, GraphName, GuardText, TEXT("Guard:missing")));
    TestTrue(TEXT("Invalid guard exit refused"), BadGuard.Contains(TEXT("refus")) || BadGuard.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Failed guard preview preserves graph"), Snapshot(), GuardBefore);
    const FString GuardPreview = Record(TEXT("Guard preview"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Asset, GraphName, GuardText, TEXT("Guard:else")));
    TestFalse(TEXT("Explicit guard continuation accepted during placement planning"), GuardPreview.Contains(TEXT("refused:")) || GuardPreview.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Guard preview preserves graph"), Snapshot(), GuardBefore);
    TestEqual(TEXT("Guard preview preserves dirty state"), Package->IsDirty(), GuardDirtyBefore);
    // A supplied input link takes precedence over a conflicting continuation option.
    const FString ExplicitGuardText = GuardText + FString::Printf(TEXT("\n(\"%s\" ; %s\n execute (-> Guard:else))"),
        *TurboMCPNodeId::GetNodeTypeId(Purchase), *TurboMCPNodeId::MakeForNode(Purchase, true));
    const FString ExplicitPreview = Record(TEXT("Guard explicit-link preview"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Asset, GraphName, ExplicitGuardText, TEXT("Guard:then")));
    TestTrue(TEXT("Preview identifies skipped named continuation"), ExplicitPreview.Contains(TEXT("Continuation='Guard:then' is skipped")));
    TestEqual(TEXT("Explicit-link preview preserves graph"), Snapshot(), GuardBefore);
    TestEqual(TEXT("Explicit-link preview preserves dirty state"), Package->IsDirty(), GuardDirtyBefore);
    TMap<UEdGraphNode*, FIntPoint> BeforeGuardPositions;
    for (UEdGraphNode* N : Graph->Nodes) BeforeGuardPositions.Add(N, FIntPoint(N->NodePosX, N->NodePosY));
    const FString GuardEdit = Record(TEXT("Guard edit"), UTurboMCPEditToolset::EditTurboMCPGraphAppend(Asset, GraphName, ExplicitGuardText, TEXT("Guard:then")));
    TestTrue(TEXT("Guard edit reports changed"), GuardEdit.StartsWith(TEXT("changed")));
    TestTrue(TEXT("Edit identifies skipped named continuation"), GuardEdit.Contains(TEXT("Continuation='Guard:then' is skipped")));
    UK2Node_IfThenElse* Guard = EntryExec->LinkedTo.Num() == 1 ? Cast<UK2Node_IfThenElse>(EntryExec->LinkedTo[0]->GetOwningNode()) : nullptr;
    if (!TestNotNull(TEXT("Entry now reaches Branch"), Guard)) return false;
    TestTrue(TEXT("Guard stays in its own execution row"), FMath::Abs(Guard->NodePosY - EntryExec->GetOwningNode()->NodePosY) <= 64);
    TestTrue(TEXT("Guard is between execution endpoints"), Guard->NodePosX > EntryExec->GetOwningNode()->NodePosX && Guard->NodePosX < Purchase->NodePosX);
    for (const auto& Position : BeforeGuardPositions)
        TestEqual(TEXT("Guard insertion preserves existing positions"), FIntPoint(Position.Key->NodePosX, Position.Key->NodePosY), Position.Value);
    TestTrue(TEXT("Only else continues old flow"), Guard->FindPinChecked(TEXT("else"))->LinkedTo.Contains(PurchaseExec) && Guard->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    Record(TEXT("EditTurboMCPCompile"), UTurboMCPEditToolset::EditTurboMCPCompile(Asset));
    if (!TestTrue(TEXT("Blueprint compiles"), BP->Status != BS_Error && BP->GeneratedClass != nullptr)) return false;
    UObject* Object = BP->GeneratedClass->GetDefaultObject();
    for (const auto& Case : {TPair<FName,int32>(TEXT("Purchase"),42), {TEXT("Refund"),30}, {TEXT("Audit"),300}})
    {
        UFunction* Event = BP->GeneratedClass->FindFunctionByName(Case.Key);
        if (!TestNotNull(TEXT("Event executable"), Event)) return false;
        Object->ProcessEvent(Event, nullptr);
        const FIntProperty* Value = FindFProperty<FIntProperty>(BP->GeneratedClass, FName(*(Case.Key.ToString() + TEXT("Value"))));
        if (!TestNotNull(TEXT("Event result property"), Value)) return false;
        TestEqual(TEXT("Independent event result"), Value->GetPropertyValue_InContainer(Object), Case.Value);
    }
    const FString Capture = Record(TEXT("LookTurboMCPCapture"), UTurboMCPLookToolset::LookTurboMCPCapture(Asset, GraphName, TEXT("graph"), TEXT(""), TEXT("1600x900")));
    FString Prefix, Path;
    TestTrue(TEXT("Native capture returned"), Capture.Split(TEXT("\nimage: "), &Prefix, &Path));
    TestTrue(TEXT("Capture exists"), IFileManager::Get().FileSize(*Path.TrimStartAndEnd()) > 1000);
    return !HasAnyErrors();
}
#endif

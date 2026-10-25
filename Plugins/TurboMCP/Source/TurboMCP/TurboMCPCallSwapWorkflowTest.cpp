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
#include "K2Node_CustomEvent.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCallSwapWorkflowTest,
    "TurboMCP.Workflow.CallSwap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPCallSwapWorkflowTest::RunTest(const FString& Parameters)
{
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/CallSwap_") + Suffix));
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_CallSwapReview"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("CallSwapReview"));
    if (!TestNotNull(TEXT("Fixture created"), BP)) return false;
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/Workflow"), Suffix);
        IFileManager::Get().MakeDirectory(*Folder, true);
        FFileHelper::SaveStringToFile(Transcript, *FPaths::Combine(Folder, TEXT("call-swap-review.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
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
    OldSource->GetOwningNode()->FindPinChecked(TEXT("A"))->DefaultValue = TEXT("0");
    Graph->GetSchema()->TryCreateConnection(NewSource, OldSource->GetOwningNode()->FindPinChecked(TEXT("A")));
    const FString ObsoleteId = TurboMCPNodeId::MakeForNode(Obsolete, true);
    auto Snapshot = [&](UEdGraphNode* Excluded = nullptr)
    {
        TArray<FString> Rows;
        for (UEdGraph* G : {Graph, OtherGraph}) for (UEdGraphNode* N : G->Nodes)
        {
            if (N == Excluded) continue;
            Rows.Add(G->GetName() + N->NodeGuid.ToString() + FString::Printf(TEXT(":%d,%d"), N->NodePosX, N->NodePosY));
            for (UEdGraphPin* P : N->Pins) if (P && !(P->bHidden && P->PinName == UEdGraphSchema_K2::PN_Self && P->LinkedTo.IsEmpty()))
            {
                const FString Key = N->NodeGuid.ToString() + P->PinName.ToString();
                Rows.Add(Key + P->DefaultValue + P->DefaultTextValue.ToString() + GetPathNameSafe(P->DefaultObject));
                for (UEdGraphPin* Q : P->LinkedTo) Rows.Add(Key + TEXT("->") + Q->GetOwningNode()->NodeGuid.ToString() + Q->PinName.ToString());
            }
        }
        Rows.Sort(); return FString::Join(Rows, TEXT("\n"));
    };
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
    TestTrue(TEXT("Connected source has inspectable values"), SourceContext.Contains(TEXT("default=40")) && SourceContext.Contains(TEXT("default=2")));
    TestTrue(TEXT("IDs used by edit exist in reader/search output"), Read.Contains(TargetId) && Found.Contains(SourceId));
    TestEqual(TEXT("Inspection preserves fixture"), Snapshot(), Before);
    const FString Types = Record(TEXT("PreviewTurboMCPNodeType"), UTurboMCPPreviewToolset::PreviewTurboMCPNodeType(Asset, GraphName, TEXT("Multiply"), TEXT("20")));
    TArray<FString> TypeLines; Types.ParseIntoArrayLines(TypeLines);
    FString Replacement;
    for (const FString& Line : TypeLines)
        if (Line.StartsWith(TEXT("Utilities|Operators|Multiply"))) { Replacement = Line.TrimStartAndEnd(); break; }
    if (!TestFalse(TEXT("Discovered multiplication type"), Replacement.IsEmpty())) return false;
    Record(TEXT("PreviewTurboMCPNodePins"), UTurboMCPPreviewToolset::PreviewTurboMCPNodePins(Asset, GraphName, Replacement));
    for (int32 Case = 0; Case < 4; ++Case)
        for (bool Edit : {false, true})
        {
            const FString Id = Case == 0 ? TEXT("missing") : ObsoleteId;
            const FString Keep = Case == 1 ? TEXT("maybe") : TEXT("false");
            const FString Options = Case == 2 ? TEXT("{\"unknown\":true}") : Case == 3 ? TEXT("{\"pin_map\":{\"A\":\"Missing\"}}") : TEXT("");
            const FString Refusal = Edit ? UTurboMCPEditToolset::EditTurboMCPGraphSwapCall(Asset, GraphName, Id, Replacement, Keep, Options)
                : UTurboMCPPreviewToolset::PreviewTurboMCPGraphSwapCall(Asset, GraphName, Id, Replacement, Keep, Options);
            Record(TEXT("GraphSwapCall refusal"), Refusal);
            TestTrue(TEXT("Invalid swap refused"), Refusal.StartsWith(TEXT("error:")) || Refusal.Contains(TEXT("refused:")));
            TestEqual(TEXT("Refusal preserves all flows"), Snapshot(), Before);
            TestEqual(TEXT("Refusal preserves dirty state"), Package->IsDirty(), DirtyBefore);
        }
    const FString Retain = Record(TEXT("PreviewTurboMCPGraphSwapCall keep old"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphSwapCall(Asset, GraphName, ObsoleteId, Replacement, TEXT("true")));
    TestTrue(TEXT("Retention choice explicit"), Retain.Contains(TEXT("keepOld: true")));
    TestEqual(TEXT("Retention preview is nonmutating"), Snapshot(), Before);
    const FString Preview = Record(TEXT("PreviewTurboMCPGraphSwapCall"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphSwapCall(Asset, GraphName, ObsoleteId, Replacement, TEXT("false")));
    TestFalse(TEXT("Compatible swap not refused"), Preview.Contains(TEXT("refused:")) || Preview.StartsWith(TEXT("error:")));
    TestTrue(TEXT("Preview names target and migration"), Preview.Contains(ObsoleteId) && Preview.Contains(TEXT("links to migrate: 2")));
    TestTrue(TEXT("Preview exposes preserved default"), Preview.Contains(TEXT("default B")) && Preview.Contains(TEXT("2")));
    TestEqual(TEXT("Preview preserves all state"), Snapshot(), Before);
    const FString Applied = Record(TEXT("EditTurboMCPGraphSwapCall"), UTurboMCPEditToolset::EditTurboMCPGraphSwapCall(Asset, GraphName, ObsoleteId, Replacement, TEXT("false")));
    if (!TestTrue(TEXT("Swap reports changed"), Applied.StartsWith(TEXT("changed:")))) return false;
    TestFalse(TEXT("Old node removed without orphan"), Graph->Nodes.Contains(Obsolete));
    UEdGraphNode* ReplacementNode = TargetPin->LinkedTo.Num() == 1 ? TargetPin->LinkedTo[0]->GetOwningNode() : nullptr;
    if (!TestNotNull(TEXT("Output consumer transferred"), ReplacementNode)) return false;
    TestTrue(TEXT("Connected input transferred"), ReplacementNode->FindPinChecked(TEXT("A"))->LinkedTo.Contains(NewSource));
    TestEqual(TEXT("Unconnected default preserved"), ReplacementNode->FindPinChecked(TEXT("B"))->DefaultValue, FString(TEXT("2")));
    // Normalize identity before sorting: a new GUID changes the snapshot ordering.
    TArray<FString> NormalizedLines;
    Snapshot().Replace(*ReplacementNode->NodeGuid.ToString(), *Obsolete->NodeGuid.ToString()).ParseIntoArrayLines(NormalizedLines);
    NormalizedLines.Sort();
    const FString Normalized = FString::Join(NormalizedLines, TEXT("\n"));
    TestEqual(TEXT("All other graph state unchanged after replacing node identity"), Normalized, Before);
    // Swap preserves positions. Tidy the affected pure expression explicitly.
    const TArray<FString> TidyIds = {SourceId, TurboMCPNodeId::MakeForNode(ReplacementNode, true)};
    const FString BeforeTidy = Snapshot();
    TMap<UEdGraphNode*, FIntPoint> Positions;
    TMap<UEdGraphPin*, TArray<UEdGraphPin*>> Links;
    TMap<UEdGraphPin*, FString> Defaults;
    for (UEdGraph* G : {Graph, OtherGraph}) for (UEdGraphNode* N : G->Nodes)
    {
        Positions.Add(N, FIntPoint(N->NodePosX, N->NodePosY));
        for (UEdGraphPin* P : N->Pins) if (P)
        { Links.Add(P, P->LinkedTo); Defaults.Add(P, P->DefaultValue + P->DefaultTextValue.ToString() + GetPathNameSafe(P->DefaultObject)); }
    }
    Record(TEXT("PreviewTurboMCPGraphTidy"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(Asset, GraphName, false, TidyIds));
    TestEqual(TEXT("Tidy preview preserves state"), Snapshot(), BeforeTidy);
    const FString Tidied = Record(TEXT("EditTurboMCPGraphTidy"), UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset, GraphName, false, TidyIds));
    TestFalse(TEXT("Scoped tidy succeeds"), Tidied.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Tidy preserves node count"), Graph->Nodes.Num() + OtherGraph->Nodes.Num(), Positions.Num());
    for (const auto& Pair : Positions) if (Pair.Key != Correct && Pair.Key != ReplacementNode)
        TestEqual(TEXT("Unrelated positions preserved"), FIntPoint(Pair.Key->NodePosX, Pair.Key->NodePosY), Pair.Value);
    for (const auto& Pair : Links) TestTrue(TEXT("Tidy preserves links"), Pair.Key->LinkedTo == Pair.Value);
    for (const auto& Pair : Defaults) TestEqual(TEXT("Tidy preserves defaults"), Pair.Key->DefaultValue + Pair.Key->DefaultTextValue.ToString() + GetPathNameSafe(Pair.Key->DefaultObject), Pair.Value);
    TestTrue(TEXT("Source precedes replacement"), Correct->NodePosX < ReplacementNode->NodePosX);
    TestTrue(TEXT("Replacement precedes consumer"), ReplacementNode->NodePosX < Purchase->NodePosX);
    Record(TEXT("EditTurboMCPCompile"), UTurboMCPEditToolset::EditTurboMCPCompile(Asset));
    if (!TestTrue(TEXT("Blueprint compiles"), BP->Status != BS_Error && BP->GeneratedClass != nullptr)) return false;
    UObject* Object = BP->GeneratedClass->GetDefaultObject();
    for (const auto& Case : {TPair<FName,int32>(TEXT("Purchase"),84), {TEXT("Refund"),30}, {TEXT("Audit"),300}})
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

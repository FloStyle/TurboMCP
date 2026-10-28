// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "BlueprintEditorLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "TurboMCPGraphTidy.h"
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
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

// Each run creates its own unsaved fixture. The only persistent output is the
// evidence transcript; no existing project asset is edited or saved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPPurchaseWorkflowTest,
    "TurboMCP.Workflow.Purchase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPPurchaseWorkflowTest::RunTest(const FString& Parameters)
{
    // Exercise refusal wording independently of the fixture's changing geometry.
    // This is output coverage, not a reproduction of the original field layout.
    FTurboMCPGraphTidy::FPlan RejectedPlan;
    RejectedPlan.RollbackReason = TEXT("candidate does not improve structural quality");
    RejectedPlan.SelectedNodeKeys.Add(TEXT("new-node"));
    const FString RejectedPreview = FTurboMCPGraphTidy::RenderPreview(RejectedPlan);
    TestTrue(TEXT("Rejected scoped tidy explains fixed surrounding nodes"), RejectedPreview.Contains(TEXT("keeps surrounding nodes fixed")));
    TestTrue(TEXT("Rejected scoped tidy names a concrete recovery preview"), RejectedPreview.Contains(TEXT("PreviewTurboMCPGraphGroupMove")));
    TestTrue(TEXT("Rejected scoped tidy does not recommend forcing acceptance"), RejectedPreview.Contains(TEXT("Do not enable Force")));
    RejectedPlan.bAlreadyTidy = true;
    TestFalse(TEXT("Already tidy does not suggest recovery"), FTurboMCPGraphTidy::RenderPreview(RejectedPlan).Contains(TEXT("Next step:")));

    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/PurchaseWorkflow_") + Suffix));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_PurchaseReview"), BPTYPE_Normal, UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPPurchaseReview"));
    if (!TestNotNull(TEXT("Disposable Blueprint created"), Blueprint)) return false;
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/Workflow"), Suffix);
        IFileManager::Get().MakeDirectory(*Directory, true);
        FFileHelper::SaveStringToFile(Transcript, *FPaths::Combine(Directory, TEXT("purchase-review.txt")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false); // Only this never-saved disposable package.
        Package->MarkAsGarbage();
    };
    auto Record = [&](const TCHAR* Tool, const FString& Output)
    {
        Transcript += FString::Printf(TEXT("\n=== %s ===\n%s\n"), Tool, *Output);
        AddInfo(FString(Tool) + TEXT(": ") + Output.Left(160));
        if (!FString(Tool).Contains(TEXT("missing graph")))
            TestFalse(FString(Tool) + TEXT(" succeeds"), Output.StartsWith(TEXT("error:")));
        if (FString(Tool).StartsWith(TEXT("LookTurboMCPCapture")) || FString(Tool) == TEXT("LookTurboMCPLayoutDoneCheck"))
        {
            FString BeforeImage, ImagePath;
            TestTrue(FString(Tool) + TEXT(" returns an image path"), Output.Split(TEXT("\nimage: "), &BeforeImage, &ImagePath));
            TestTrue(FString(Tool) + TEXT(" writes a nonempty image"), IFileManager::Get().FileSize(*ImagePath.TrimStartAndEnd()) > 1000);
        }
        return Output;
    };
    UEdGraph* Graph = UBlueprintEditorLibrary::AddFunctionGraph(Blueprint, TEXT("CalculatePurchase"));
    UEdGraph* Untouched = UBlueprintEditorLibrary::AddFunctionGraph(Blueprint, TEXT("UnrelatedFunction"));
    if (!TestNotNull(TEXT("Purchase graph"), Graph) || !TestNotNull(TEXT("Unrelated graph"), Untouched)) return false;
    TArray<UK2Node_FunctionEntry*> Entries;
    Graph->GetNodesOfClass(Entries);
    if (!TestEqual(TEXT("One entry"), Entries.Num(), 1)) return false;
    UK2Node_FunctionEntry* Entry = Entries[0];
    UK2Node_FunctionResult* Result = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
    if (!TestNotNull(TEXT("Function result"), Result)) return false;
    FEdGraphPinType NumberType;
    NumberType.PinCategory = UEdGraphSchema_K2::PC_Real;
    NumberType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    TMap<FName, UEdGraphPin*> Inputs;
    for (const TCHAR* Name : {TEXT("UnitPrice"), TEXT("Quantity"), TEXT("DiscountPercent"), TEXT("AvailableFunds"), TEXT("MinimumQuantity")})
        Inputs.Add(Name, Entry->CreateUserDefinedPin(Name, NumberType, EGPD_Output, false));
    UEdGraphPin* PriceOutput = Result->CreateUserDefinedPin(TEXT("FinalPrice"), NumberType, EGPD_Input, false);
    UEdGraphPin* AffordableOutput = Result->CreateUserDefinedPin(TEXT("CanAfford"), BoolType, EGPD_Input, false);
    auto Call = [&](const TCHAR* Name, int32 X, int32 Y)
    {
        UFunction* Function = UKismetMathLibrary::StaticClass()->FindFunctionByName(Name);
        if (!TestNotNull(Name, Function)) return static_cast<UK2Node_CallFunction*>(nullptr);
        UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
        Node->SetFromFunction(Function);
        Node->CreateNewGuid();
        Graph->AddNode(Node, false, false);
        Node->AllocateDefaultPins();
        Node->NodePosX = X;
        Node->NodePosY = Y;
        return Node;
    };
    UK2Node_CallFunction* Subtotal = Call(TEXT("Multiply_DoubleDouble"), 800, 550);
    UK2Node_CallFunction* Rate = Call(TEXT("Multiply_DoubleDouble"), 100, 650);
    UK2Node_CallFunction* Discount = Call(TEXT("Multiply_DoubleDouble"), 1100, 800);
    UK2Node_CallFunction* Final = Call(TEXT("Subtract_DoubleDouble"), 300, 450);
    UK2Node_CallFunction* Affordable = Call(TEXT("LessEqual_DoubleDouble"), 1500, 500);
    if (!Subtotal || !Rate || !Discount || !Final || !Affordable) return false;
    Entry->NodePosX = 0; Entry->NodePosY = 0;
    Result->NodePosX = 2100; Result->NodePosY = 0;
    Inputs[TEXT("UnitPrice")]->MakeLinkTo(Subtotal->FindPinChecked(TEXT("A")));
    Inputs[TEXT("Quantity")]->MakeLinkTo(Subtotal->FindPinChecked(TEXT("B")));
    Inputs[TEXT("DiscountPercent")]->MakeLinkTo(Rate->FindPinChecked(TEXT("A")));
    Rate->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("0.1"); // Deliberate percent conversion bug.
    Subtotal->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Discount->FindPinChecked(TEXT("A")));
    Rate->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Discount->FindPinChecked(TEXT("B")));
    Subtotal->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Final->FindPinChecked(TEXT("A")));
    Discount->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Final->FindPinChecked(TEXT("B")));
    Final->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(PriceOutput);
    Final->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Affordable->FindPinChecked(TEXT("A")));
    Inputs[TEXT("AvailableFunds")]->MakeLinkTo(Affordable->FindPinChecked(TEXT("B")));
    Affordable->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(AffordableOutput);
    Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Result->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    const FString Asset = Blueprint->GetPathName();
    const FString GraphName = Graph->GetName();
    const FGuid RateGuid = Rate->NodeGuid;
    const int32 OriginalCount = Graph->Nodes.Num();
    auto ReadGraph = [&]() { return UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, GraphName, TEXT("read"), TEXT("full")); };
    auto CheckValues = [&](bool Correct)
    {
        UFunction* Function = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(*GraphName) : nullptr;
        if (!TestNotNull(TEXT("Executable purchase function"), Function)) return false;
        for (double Quantity : {2.0, 4.0})
        {
            FStructOnScope Args(Function);
            for (const auto& Pair : TMap<FName, double>{{TEXT("UnitPrice"), 50.0}, {TEXT("Quantity"), Quantity},
                {TEXT("DiscountPercent"), 10.0}, {TEXT("AvailableFunds"), 100.0}})
            {
                const FDoubleProperty* Property = FindFProperty<FDoubleProperty>(Function, Pair.Key);
                if (!TestNotNull(TEXT("Numeric input exists"), Property)) return false;
                Property->SetPropertyValue_InContainer(Args.GetStructMemory(), Pair.Value);
            }
            Blueprint->GeneratedClass->GetDefaultObject()->ProcessEvent(Function, Args.GetStructMemory());
            const FDoubleProperty* Price = FindFProperty<FDoubleProperty>(Function, TEXT("FinalPrice"));
            const FBoolProperty* CanAfford = FindFProperty<FBoolProperty>(Function, TEXT("CanAfford"));
            if (!TestNotNull(TEXT("Price output exists"), Price) || !TestNotNull(TEXT("Affordability output exists"), CanAfford)) return false;
            const double Expected = Correct ? 50.0 * Quantity * 0.9 : 0.0;
            TestTrue(TEXT("Price matches independent expected calculation"), FMath::IsNearlyEqual(Price->GetPropertyValue_InContainer(Args.GetStructMemory()), Expected));
            TestEqual(TEXT("Affordability follows current inputs"), CanAfford->GetPropertyValue_InContainer(Args.GetStructMemory()), Expected <= 100.0);
        }
        return !HasAnyErrors();
    };
    const FString InitialCompile = Record(TEXT("EditTurboMCPCompile baseline"), UTurboMCPEditToolset::EditTurboMCPCompile(Asset));
    if (!CheckValues(false)) return false;
    const FString UnrelatedBefore = UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, Untouched->GetName(), TEXT("read"), TEXT("full"));
    Package->SetDirtyFlag(false); // Establish a clean baseline only on this disposable fixture.
    const FString Before = Record(TEXT("ReadTurboMCPGraphLoad baseline"), ReadGraph());
    const FString Overview = Record(TEXT("ReadTurboMCPBlueprintLoad overview"), UTurboMCPReadToolset::ReadTurboMCPBlueprintLoad(Asset, TEXT("signatures"), TEXT(""), TEXT("read"), TEXT("60000")));
    TestTrue(TEXT("Overview names purchase function"), Overview.Contains(GraphName));
    TestTrue(TEXT("Overview names unrelated function"), Overview.Contains(Untouched->GetName()));
    TestTrue(TEXT("Detailed read includes supplied context"), Before.Contains(TEXT("UnitPrice")) && Before.Contains(TEXT("DiscountPercent")));
    TestEqual(TEXT("Overview does not change graph content"), ReadGraph(), Before);
    Record(TEXT("ReadTurboMCPNodeFind"), UTurboMCPReadToolset::ReadTurboMCPNodeFind(Asset, TEXT("Multiply"), GraphName));
    TestTrue(TEXT("Find explains a search with no matches"), UTurboMCPReadToolset::ReadTurboMCPNodeFind(Asset, TEXT("NoSuchPurchaseNode"), GraphName).Contains(TEXT("Nothing matched")));
    const FString RateId = TurboMCPNodeId::Make(Asset, GraphName, RateGuid, true);
    Record(TEXT("ReadTurboMCPNodeContext"), UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, RateId, 3));
    Record(TEXT("LookTurboMCPGraphPositions before"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName));
    const FString Geometry = Record(TEXT("GraphPositions selected"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName, RateId));
    TestTrue(TEXT("Geometry selects one requested node"), Geometry.Contains(RateId) && Geometry.Contains(TEXT("listed 1 of 1 selected nodes")));
    TestTrue(TEXT("Geometry explains units and rendering limits"), Geometry.Contains(TEXT("graph units")) && Geometry.Contains(TEXT("not screen pixels")));
    UEdGraphNode* GeometryNode = nullptr;
    UEdGraphNode* OtherGeometryNode = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes) if (Node)
    {
        if (Node->NodeGuid == RateGuid) GeometryNode = Node;
        else OtherGeometryNode = Node;
    }
    if (!TestNotNull(TEXT("Geometry target survives compile"), GeometryNode)) return false;
    TestTrue(TEXT("Geometry reports actual stored coordinates"), Geometry.Contains(FString::Printf(TEXT("x=%d y=%d"), GeometryNode->NodePosX, GeometryNode->NodePosY)));
    TestEqual(TEXT("Unique key and exact ID select same geometry"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName, TurboMCPNodeId::MakeKey(RateGuid, 32)), Geometry);
    TestEqual(TEXT("Duplicate requested IDs do not duplicate rows"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName, RateId + TEXT("\n") + RateId), Geometry);
    const FString Limited = UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName, TEXT(""), 1);
    TestTrue(TEXT("Bounded geometry announces omitted nodes"), Limited.Contains(TEXT("listed 1 of")) && Limited.Contains(TEXT("TRUNCATED:")));
    TestTrue(TEXT("Missing geometry ID refuses"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName, TEXT("missing")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Blank geometry entry refuses"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName, TEXT(" ")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Invalid geometry limit refuses"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName, TEXT(""), 0).StartsWith(TEXT("error:")));
    if (OtherGeometryNode)
    {
        const FGuid OriginalGuid = OtherGeometryNode->NodeGuid;
        OtherGeometryNode->NodeGuid = RateGuid; // Deliberate malformed identity on this disposable fixture only.
        const FString Ambiguous = UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName, TurboMCPNodeId::MakeKey(RateGuid, 32));
        OtherGeometryNode->NodeGuid = OriginalGuid;
        TestTrue(TEXT("Ambiguous geometry identity refuses"), Ambiguous.StartsWith(TEXT("error:")) && Ambiguous.Contains(TEXT("ambiguous")));
    }
    const FString GeometryHelp = UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCP.TurboMCPLookToolset"), TEXT("LookTurboMCPGraphPositions"));
    TestTrue(TEXT("Live geometry help exposes selection"), GeometryHelp.Contains(TEXT("NodeIds"), ESearchCase::IgnoreCase) && GeometryHelp.Contains(TEXT("MaxNodes"), ESearchCase::IgnoreCase));
    TestEqual(TEXT("Geometry inspection preserves graph content"), ReadGraph(), Before);

    Record(TEXT("LookTurboMCPLayoutReport before"), UTurboMCPLookToolset::LookTurboMCPLayoutReport(Asset, GraphName));
    Record(TEXT("LookTurboMCPCapture before"), UTurboMCPLookToolset::LookTurboMCPCapture(Asset, GraphName, TEXT("graph"), TEXT(""), TEXT("1600x900")));
    TestFalse(TEXT("Inspection and scratch capture do not dirty the asset"), Package->IsDirty());
    const FString MissingGraph = Record(TEXT("GraphLoad missing graph"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, TEXT("MissingGraph"), TEXT("read"), TEXT("full")));
    TestTrue(TEXT("Missing graph gives actual choices"), MissingGraph.StartsWith(TEXT("error:")) && MissingGraph.Contains(GraphName));
    const FString Page = Record(TEXT("GraphLoad first page"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, GraphName, TEXT("read"), TEXT("full"), 0, 256));
    TestTrue(TEXT("Expected pagination is not an error"), Page.StartsWith(TEXT("; partial:")));
    TestTrue(TEXT("Page is refused as edit input"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphCheck(Asset, GraphName, Page).Contains(TEXT("inspection-only fragment")));
    const FString Filtered = UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, GraphName, TEXT("read"), TEXT("full"), 0, 24000, TEXT(""), GraphName);
    TestTrue(TEXT("Filtered view is refused as edit input"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphCheck(Asset, GraphName, Filtered).Contains(TEXT("inspection-only fragment")));
    TestTrue(TEXT("Page describes continuation"), Page.Contains(TEXT("revision=")) && Page.Contains(TEXT("nextOffset=")));
    TestTrue(TEXT("Stale continuation refuses"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, GraphName, TEXT("read"), TEXT("full"), 256, 256, TEXT("stale")).StartsWith(TEXT("error:")));
    for (const FString& Refusal : {
        UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, TEXT("missing"), 3),
        UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, RateId, -1),
        UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, TEXT("MissingGraph")),
        UTurboMCPLookToolset::LookTurboMCPLayoutReport(Asset, GraphName, -1),
        UTurboMCPLookToolset::LookTurboMCPCapture(Asset, TEXT("MissingGraph"), TEXT("graph"), TEXT(""), TEXT("1600x900")),
        UTurboMCPPreviewToolset::PreviewTurboMCPComment(Asset, GraphName, TEXT("create"), TEXT("missing"), TEXT(""), TEXT("Invalid"), TEXT(""), TEXT("")),
        UTurboMCPEditToolset::EditTurboMCPComment(Asset, GraphName, TEXT("create"), TEXT("missing"), TEXT(""), TEXT("Invalid"), TEXT(""), TEXT("")),
        UTurboMCPReadToolset::ReadTurboMCPCommentList(Asset, TEXT("MissingGraph"))})
        TestTrue(TEXT("Invalid target or parameter gives a named refusal"), Refusal.StartsWith(TEXT("error:")));
    const FString MissingPin = UTurboMCPEditToolset::EditTurboMCPPinDefault(Asset, GraphName, RateId, TEXT("NoSuchPin"), TEXT("0.01"));
    TestTrue(TEXT("Missing pin is refused with its exact name"), MissingPin.StartsWith(TEXT("unchanged: refused;")) && MissingPin.Contains(TEXT("NoSuchPin")));
    TestEqual(TEXT("Refused and paged reads preserve graph"), ReadGraph(), Before);
    const FString Fixed = Record(TEXT("EditTurboMCPPinDefault"), UTurboMCPEditToolset::EditTurboMCPPinDefault(Asset, GraphName, RateId, TEXT("B"), TEXT("0.01")));
    TestTrue(TEXT("Targeted default edit reports change"), Fixed.StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Default result identifies the edited node and value"), Fixed.Contains(RateId) && Fixed.Contains(TEXT("pin: \"B\"")) && Fixed.Contains(TEXT("value: \"0.01\"")));
    const FString NoOp = UTurboMCPEditToolset::EditTurboMCPPinDefault(Asset, GraphName, RateId, TEXT("B"), TEXT("0.01"));
    TestTrue(TEXT("Repeated default reports unchanged with its target"), NoOp.StartsWith(TEXT("unchanged:")) && NoOp.Contains(RateId));
    TestEqual(TEXT("Default edit preserves node count"), Graph->Nodes.Num(), OriginalCount);
    Record(TEXT("EditTurboMCPCompile corrected"), UTurboMCPEditToolset::EditTurboMCPCompile(Asset));
    if (!CheckValues(true)) return false;
    Record(TEXT("ReadTurboMCPGraphLoad corrected"), ReadGraph());
    const FString Names = FString::Printf(TEXT("{\"%s:ReturnValue\":\"Subtotal\",\"%s:ReturnValue\":\"PurchaseTotal\"}"),
        *TurboMCPNodeId::Make(Asset, GraphName, Subtotal->NodeGuid, true),
        *TurboMCPNodeId::Make(Asset, GraphName, Final->NodeGuid, true));
    const FString BeforeInvalidNames = ReadGraph();
    TestTrue(TEXT("Unknown local output refuses"), UTurboMCPEditToolset::EditTurboMCPLocalVars(Asset, GraphName, TEXT("{\"missing\":\"Subtotal\"}")).StartsWith(TEXT("error:")));
    for (const FString& InvalidNames : {Names.Replace(TEXT("Subtotal"), TEXT("UnitPrice")),
        Names.Replace(TEXT("Subtotal"), TEXT("PurchaseTotal")), Names.Replace(TEXT("Subtotal"), TEXT("123 invalid")), FString(TEXT("[]"))})
        TestTrue(TEXT("Invalid or colliding names refuse before editing"), UTurboMCPEditToolset::EditTurboMCPLocalVars(Asset, GraphName, InvalidNames).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Invalid naming does not mutate"), ReadGraph(), BeforeInvalidNames);
    const FString BeforeLocalPreview = ReadGraph();
    Record(TEXT("PreviewTurboMCPLocalVars"), UTurboMCPPreviewToolset::PreviewTurboMCPLocalVars(Asset, GraphName, Names));
    TestEqual(TEXT("Local preview does not edit content"), ReadGraph(), BeforeLocalPreview);
    const FString Locals = Record(TEXT("EditTurboMCPLocalVars"), UTurboMCPEditToolset::EditTurboMCPLocalVars(Asset, GraphName, Names));
    TestTrue(TEXT("Supported repeated arithmetic is promoted"), Locals.StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Meaningful local names survive the edit"), ReadGraph().Contains(TEXT("Subtotal")) && ReadGraph().Contains(TEXT("PurchaseTotal")));
    if (!CheckValues(true)) return false;
    Record(TEXT("ReadTurboMCPGraphLoad locals"), ReadGraph());
    const TArray<FString> WholeGraph;
    const FString BeforeTidyPreview = ReadGraph();
    const FString BeforeTidyPositions = UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName);
    Record(TEXT("PreviewTurboMCPGraphTidy"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(Asset, GraphName, false, WholeGraph));
    TestEqual(TEXT("Tidy preview preserves content"), ReadGraph(), BeforeTidyPreview);
    TestEqual(TEXT("Tidy preview preserves positions"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName), BeforeTidyPositions);
    const FString Tidy = Record(TEXT("EditTurboMCPGraphTidy"), UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset, GraphName, false, WholeGraph));
    TestFalse(TEXT("Tidy does not return an error"), Tidy.StartsWith(TEXT("error:")));
    // A collision-free graph can still have backward, looping getter wires.
    // Check the consumer relation, not just whether node rectangles overlap.
    int32 LocalReaderLinks = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (const UK2Node_VariableGet* Reader = Cast<UK2Node_VariableGet>(Node))
            for (UEdGraphPin* Output : Reader->Pins)
                if (Output && Output->Direction == EGPD_Output)
                    for (UEdGraphPin* Input : Output->LinkedTo)
                        if (Input && Input->GetOwningNodeUnchecked())
                        {
                            ++LocalReaderLinks;
                            TestTrue(TEXT("Getter output stays left of its consumer input"),
                                FTurboMCPGraphTidy::AlignmentPinPosition(Output).X
                                < FTurboMCPGraphTidy::AlignmentPinPosition(Input).X);
                        }
    TestTrue(TEXT("Layout direction check covers local readers"), LocalReaderLinks >= 4);
    for (UEdGraphNode* Consumer : Graph->Nodes)
        for (UEdGraphPin* Upper : Consumer->Pins)
            if (Upper && Upper->Direction == EGPD_Input && Upper->LinkedTo.Num() == 1
                && Cast<UK2Node_VariableGet>(Upper->LinkedTo[0]->GetOwningNodeUnchecked()))
                for (UEdGraphPin* Lower : Consumer->Pins)
                    if (Lower && Lower->Direction == EGPD_Input && Lower->LinkedTo.Num() == 1
                        && Lower->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
                        && FTurboMCPGraphTidy::AlignmentPinPosition(Upper).Y < FTurboMCPGraphTidy::AlignmentPinPosition(Lower).Y)
                        TestTrue(TEXT("Getter inputs preserve vertical pin order"),
                            FTurboMCPGraphTidy::AlignmentPinPosition(Upper->LinkedTo[0]).Y
                            <= FTurboMCPGraphTidy::AlignmentPinPosition(Lower->LinkedTo[0]).Y);

    if (!CheckValues(true)) return false;
    // TD-42: a mixed selection must keep a single-consumer getter near its
    // fixed consumer, even when its starting position is thousands of units away.
    UK2Node_VariableGet* ScopedReader = nullptr;
    UEdGraphPin* ScopedInput = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (UK2Node_VariableGet* Candidate = Cast<UK2Node_VariableGet>(Node))
            for (UEdGraphPin* Pin : Candidate->Pins)
                if (!ScopedReader && Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() == 1
                    && Pin->LinkedTo[0]->GetOwningNode() != Result)
                { ScopedReader = Candidate; ScopedInput = Pin->LinkedTo[0]; }
    if (!TestNotNull(TEXT("Scoped tidy fixture has a single-consumer reader"), ScopedReader)) return false;
    TMap<UEdGraphNode*, FIntPoint> SavedPositions;
    for (UEdGraphNode* Node : Graph->Nodes) SavedPositions.Add(Node, FIntPoint(Node->NodePosX, Node->NodePosY));
    ScopedReader->NodePosX += 4000;
    const FString ScopedBefore = ReadGraph();
    Record(TEXT("LookTurboMCPCapture scoped tidy before"), UTurboMCPLookToolset::LookTurboMCPCapture(Asset, GraphName, TEXT("graph"), TEXT(""), TEXT("2048x1152")));
    const TArray<FString> MixedSelection{TurboMCPNodeId::MakeForNode(ScopedReader, true), TurboMCPNodeId::MakeForNode(Result, true)};
    Record(TEXT("Scoped tidy mixed selection"), UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset, GraphName, true, MixedSelection));
    TestEqual(TEXT("Scoped tidy preserves graph content and links"), ReadGraph(), ScopedBefore);
    for (const auto& Saved : SavedPositions)
        if (Saved.Key != ScopedReader && Saved.Key != Result)
            TestTrue(TEXT("Scoped tidy preserves unselected positions"), Saved.Value == FIntPoint(Saved.Key->NodePosX, Saved.Key->NodePosY));
    UEdGraphPin* ScopedExec = Result->FindPinChecked(UEdGraphSchema_K2::PN_Execute);
    TestEqual(TEXT("Scoped return keeps one execution input"), ScopedExec->LinkedTo.Num(), 1);
    if (ScopedExec->LinkedTo.Num() == 1)
        TestTrue(TEXT("Scoped return stays on the fixed execution lane"), FMath::Abs(FTurboMCPGraphTidy::AlignmentPinPosition(ScopedExec).Y
            - FTurboMCPGraphTidy::AlignmentPinPosition(ScopedExec->LinkedTo[0]).Y) <= 1.0);
    const UEdGraphNode* ScopedConsumer = ScopedInput->GetOwningNode();
    const FVector2D ReaderSize = FTurboMCPGraphTidy::AlignmentNodeSize(ScopedReader);
    TestTrue(TEXT("Scoped getter is left of its consumer"), ScopedReader->NodePosX + ReaderSize.X < ScopedConsumer->NodePosX);
    TestTrue(TEXT("Scoped getter stays near its consumer"), FMath::Abs(ScopedReader->NodePosX - ScopedConsumer->NodePosX) < 1200
        && FMath::Abs(ScopedReader->NodePosY - ScopedConsumer->NodePosY) < 1200);
    for (UEdGraphNode* Other : Graph->Nodes)
        if (Other != ScopedReader)
        {
            const FVector2D OtherSize = FTurboMCPGraphTidy::AlignmentNodeSize(Other);
            TestFalse(TEXT("Scoped getter does not overlap another node"), ScopedReader->NodePosX < Other->NodePosX + OtherSize.X
                && ScopedReader->NodePosX + ReaderSize.X > Other->NodePosX && ScopedReader->NodePosY < Other->NodePosY + OtherSize.Y
                && ScopedReader->NodePosY + ReaderSize.Y > Other->NodePosY);
        }
    Record(TEXT("LookTurboMCPCapture scoped tidy after"), UTurboMCPLookToolset::LookTurboMCPCapture(Asset, GraphName, TEXT("graph"), TEXT(""), TEXT("2048x1152")));
    for (const auto& Saved : SavedPositions) { Saved.Key->NodePosX = Saved.Value.X; Saved.Key->NodePosY = Saved.Value.Y; }
    Graph->NotifyGraphChanged();

    const FString NodeIds = TurboMCPNodeId::Make(Asset, GraphName, RateGuid, true);
    const int32 BeforeCommentCount = Graph->Nodes.Num();
    const FIntPoint RatePosition(Rate->NodePosX, Rate->NodePosY);
    Record(TEXT("PreviewTurboMCPComment"), UTurboMCPPreviewToolset::PreviewTurboMCPComment(Asset, GraphName, TEXT("create"), NodeIds, TEXT(""), TEXT("Convert discount percent to a fraction"), TEXT(""), TEXT("")));
    TestEqual(TEXT("Comment preview creates no node"), Graph->Nodes.Num(), BeforeCommentCount);
    Record(TEXT("EditTurboMCPComment"), UTurboMCPEditToolset::EditTurboMCPComment(Asset, GraphName, TEXT("create"), NodeIds, TEXT(""), TEXT("Convert discount percent to a fraction"), TEXT(""), TEXT("")));
    TArray<UEdGraphNode_Comment*> Comments;
    Graph->GetNodesOfClass(Comments);
    if (!TestEqual(TEXT("One annotation created"), Comments.Num(), 1)) return false;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (Node && Node != Rate && !Node->IsA<UEdGraphNode_Comment>())
        {
            const FVector2D Size = FTurboMCPGraphTidy::AlignmentNodeSize(Node);
            const UEdGraphNode_Comment* Box = Comments[0];
            TestFalse(TEXT("Annotation does not cover unrelated nodes"),
                Node->NodePosX < Box->NodePosX + Box->NodeWidth && Node->NodePosX + Size.X > Box->NodePosX
                && Node->NodePosY < Box->NodePosY + Box->NodeHeight && Node->NodePosY + Size.Y > Box->NodePosY);
        }
    TestFalse(TEXT("New comment has no duplicate floating bubble"), Comments[0]->bCommentBubbleVisible);
    TestFalse(TEXT("New comment does not pin a floating bubble"), Comments[0]->bCommentBubblePinned);
    TestTrue(TEXT("Long title has room above its content"), Rate->NodePosY - Comments[0]->NodePosY > 80);
    TestEqual(TEXT("Annotation preserves node position"), FIntPoint(Rate->NodePosX, Rate->NodePosY), RatePosition);
    const FString CommentList = Record(TEXT("ReadTurboMCPCommentList"), UTurboMCPReadToolset::ReadTurboMCPCommentList(Asset, GraphName));
    TestTrue(TEXT("Comment list identifies its approximation and actual title"), CommentList.Contains(TEXT("node origins")) && CommentList.Contains(TEXT("Convert discount percent to a fraction")));
    Record(TEXT("ReadTurboMCPGraphLoad final"), ReadGraph());
    Record(TEXT("LookTurboMCPCapture readable scale"), UTurboMCPLookToolset::LookTurboMCPCapture(Asset, GraphName, TEXT("graph"), TEXT(""), TEXT("4096x1600")));
    Record(TEXT("LookTurboMCPLayoutDoneCheck"), UTurboMCPLookToolset::LookTurboMCPLayoutDoneCheck(Asset, GraphName));
    TestEqual(TEXT("Unrelated function remains unchanged"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, Untouched->GetName(), TEXT("read"), TEXT("full")), UnrelatedBefore);

    // Append changes the body only. MinimumQuantity is an unused fixture input;
    // adding a parameter to an existing function is a separate unsupported step.
    const FString ReturnId = TurboMCPNodeId::MakeForNode(Result, true);
    const FString AffordableId = TurboMCPNodeId::MakeForNode(Affordable, true);
    auto DiscoverType = [&](const FString& Query, const FString& Prefix, const FString& ExpectedOutput = TEXT("ReturnValue"))
    {
        const FString Types = Record(TEXT("PreviewTurboMCPNodeType append discovery"), UTurboMCPPreviewToolset::PreviewTurboMCPNodeType(Asset, GraphName, Query, TEXT("20")));
        TArray<FString> Lines;
        Types.ParseIntoArrayLines(Lines);
        FString Type;
        for (const FString& Line : Lines)
            if (Line.StartsWith(Prefix)) { Type = Line.TrimStartAndEnd(); break; }
        TestFalse(TEXT("Discovery supplies an exact type ID"), Type.IsEmpty());
        if (Prefix == TEXT("Math|Boolean|"))
            TestFalse(TEXT("Boolean AND discovery agrees with pure pin inspection"), Types.Contains(TEXT("not pure")));

        if (!Type.IsEmpty())
        {
            const FString Pins = Record(TEXT("PreviewTurboMCPNodePins append discovery"), UTurboMCPPreviewToolset::PreviewTurboMCPNodePins(Asset, GraphName, Type));
            TestTrue(TEXT("Pin inspection exposes the expected output"), Pins.Contains(ExpectedOutput));
        }
        return Type;
    };
    const FString CompareType = DiscoverType(TEXT(">="), TEXT("Utilities|Operators|GreaterEqual("));
    const FString AndType = DiscoverType(TEXT("AND Boolean"), TEXT("Math|Boolean|"));
    if (CompareType.IsEmpty() || AndType.IsEmpty()) return false;
    const FString QuantityType = DiscoverType(TEXT("Quantity"), TEXT("FunctionInput|") + Asset + TEXT("|") + GraphName + TEXT("|Quantity"), TEXT("Quantity"));
    const FString MinimumType = DiscoverType(TEXT("MinimumQuantity"), TEXT("FunctionInput|") + Asset + TEXT("|") + GraphName + TEXT("|MinimumQuantity"), TEXT("MinimumQuantity"));
    if (QuantityType.IsEmpty() || MinimumType.IsEmpty()) return false;
    const FString AppendText = FString::Printf(TEXT("(%s) ; label QuantityValue\n(%s) ; label MinimumValue\n(%s\n A (-> QuantityValue:Quantity)\n B (-> MinimumValue:MinimumQuantity)\n) ; label QuantityAllowed\n(%s\n A (-> %s:ReturnValue)\n B (-> QuantityAllowed:ReturnValue)\n) ; label PurchaseAllowed\n(Function|Result ; %s\n CanAfford (-> PurchaseAllowed:ReturnValue)\n)"),
        *QuantityType, *MinimumType, *CompareType, *AndType, *AffordableId, *ReturnId);
    Transcript += TEXT("\n=== Submitted append text ===\n") + AppendText + TEXT("\n");
    const FString BeforeAppend = ReadGraph();
    const FString BeforeAppendPositions = UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName);
    const int32 BeforeAppendCount = Graph->Nodes.Num();
    const bool BeforeAppendDirty = Package->IsDirty();
    auto CheckAppendUnchanged = [&]()
    {
        TestEqual(TEXT("Append preview/refusal preserves exact read"), ReadGraph(), BeforeAppend);
        TestEqual(TEXT("Append preview/refusal preserves positions"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName), BeforeAppendPositions);
        TestEqual(TEXT("Append preview/refusal preserves node count"), Graph->Nodes.Num(), BeforeAppendCount);
        TestEqual(TEXT("Append preview/refusal preserves dirty state"), Package->IsDirty(), BeforeAppendDirty);
    };
    for (const FString& BadText : {AppendText.Replace(TEXT("CanAfford ("), TEXT("NoSuchOutput (")), AppendText.Left(AppendText.Len()-1)})
    {
        for (bool Edit : {false, true})
        {
            const FString Refusal = Edit ? UTurboMCPEditToolset::EditTurboMCPGraphAppend(Asset, GraphName, BadText)
                : UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Asset, GraphName, BadText);
            Transcript += TEXT("\n=== Append refusal ===\n") + Refusal + TEXT("\n");
            TestTrue(TEXT("Malformed append refuses explicitly"), Refusal.StartsWith(TEXT("error:")) || Refusal.StartsWith(TEXT("unchanged:")));
            CheckAppendUnchanged();
        }
    }
    const FString AppendPreview = Record(TEXT("PreviewTurboMCPGraphAppend purchase rule"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Asset, GraphName, AppendText));
    TestTrue(TEXT("Preview describes two operations and two input getters"), AppendPreview.Contains(TEXT("4 new node(s)")));
    TestTrue(TEXT("Preview identifies the replaced target"), AppendPreview.Contains(TEXT("CanAfford")));
    CheckAppendUnchanged();
    TMap<UEdGraphNode*, FIntPoint> ExistingPositions;
    TMap<UEdGraphPin*, FString> ExistingDefaults;
    TMap<UEdGraphPin*, TArray<UEdGraphPin*>> ExistingLinks;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        ExistingPositions.Add(Node, FIntPoint(Node->NodePosX, Node->NodePosY));
        for (UEdGraphPin* Pin : Node->Pins) if (Pin)
        {
            ExistingDefaults.Add(Pin, Pin->DefaultValue + TEXT("|") + Pin->DefaultTextValue.ToString() + TEXT("|") + GetPathNameSafe(Pin->DefaultObject));
            ExistingLinks.Add(Pin, Pin->LinkedTo);
        }
    }
    const FString Appended = Record(TEXT("EditTurboMCPGraphAppend purchase rule"), UTurboMCPEditToolset::EditTurboMCPGraphAppend(Asset, GraphName, AppendText));
    if (!TestTrue(TEXT("Append reports changed"), Appended.StartsWith(TEXT("changed:")))) return false;
    TestEqual(TEXT("Append creates exactly four planned nodes"), Graph->Nodes.Num(), BeforeAppendCount + 4);
    for (UEdGraphNode* Node : Graph->Nodes)
        if (!ExistingPositions.Contains(Node))
            TestTrue(TEXT("Appended pure nodes precede their existing consumer"), Node->NodePosX < Result->NodePosX);

    TestTrue(TEXT("Append explains compile and save state"), Appended.Contains(TEXT("compile")) && Appended.Contains(TEXT("save")));
    for (const auto& Pair : ExistingPositions)
    {
        TestTrue(TEXT("Every original node survives"), Graph->Nodes.Contains(Pair.Key));
        TestEqual(TEXT("Existing node position preserved"), FIntPoint(Pair.Key->NodePosX, Pair.Key->NodePosY), Pair.Value);
    }
    for (const auto& Pair : ExistingDefaults)
        TestEqual(TEXT("Existing default preserved"), Pair.Key->DefaultValue + TEXT("|") + Pair.Key->DefaultTextValue.ToString() + TEXT("|") + GetPathNameSafe(Pair.Key->DefaultObject), Pair.Value);
    for (const auto& Pair : ExistingLinks)
        for (UEdGraphPin* Linked : Pair.Value)
            if (Pair.Key != AffordableOutput && Linked != AffordableOutput)
                TestTrue(TEXT("Unrelated existing link preserved"), Pair.Key->LinkedTo.Contains(Linked));
    TestEqual(TEXT("CanAfford has one source after replacement"), AffordableOutput->LinkedTo.Num(), 1);
    TestFalse(TEXT("Old direct affordability connection removed"), AffordableOutput->LinkedTo.Contains(Affordable->FindPinChecked(TEXT("ReturnValue"))));
    // Append preserves existing placement. Complete the edit workflow with an
    // explicit tidy of the affected pure expression, including its old inputs.
    TSet<UEdGraphNode*> RuleNodes;
    TArray<UEdGraphNode*> PendingRule;
    PendingRule.Add(AffordableOutput->LinkedTo[0]->GetOwningNode());
    while (!PendingRule.IsEmpty())
    {
        UEdGraphNode* Node = PendingRule.Pop();
        if (RuleNodes.Contains(Node)) continue;
        RuleNodes.Add(Node);
        for (UEdGraphPin* Pin : Node->Pins) if (Pin && Pin->Direction == EGPD_Input)
            for (UEdGraphPin* Source : Pin->LinkedTo)
                PendingRule.Add(Source->GetOwningNode());
    }
    TArray<FString> RuleIds;
    for (UEdGraphNode* Node : Graph->Nodes) if (RuleNodes.Contains(Node))
        RuleIds.Add(TurboMCPNodeId::MakeForNode(Node, true));
    TMap<UEdGraphPin*, TArray<UEdGraphPin*>> RuleLinks;
    for (UEdGraphNode* Node : Graph->Nodes)
        for (UEdGraphPin* Pin : Node->Pins) if (Pin) RuleLinks.Add(Pin, Pin->LinkedTo);
    const FString BeforeRuleTidy = ReadGraph();
    const FString BeforeRulePositions = UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName);
    Record(TEXT("PreviewTurboMCPGraphTidy purchase rule"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(Asset, GraphName, false, RuleIds));
    TestEqual(TEXT("Rule tidy preview preserves content"), ReadGraph(), BeforeRuleTidy);
    TestEqual(TEXT("Rule tidy preview preserves positions"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName), BeforeRulePositions);
    const FString RuleTidy = Record(TEXT("EditTurboMCPGraphTidy purchase rule"), UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset, GraphName, false, RuleIds));
    TestFalse(TEXT("Rule tidy succeeds"), RuleTidy.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Rule tidy preserves node count"), Graph->Nodes.Num(), BeforeAppendCount + 4);
    for (const auto& Pair : RuleLinks)
        TestTrue(TEXT("Rule tidy preserves exact pin links"), Pair.Key->LinkedTo == Pair.Value);
    for (const auto& Pair : ExistingDefaults)
        TestEqual(TEXT("Rule tidy preserves defaults"), Pair.Key->DefaultValue + TEXT("|") + Pair.Key->DefaultTextValue.ToString() + TEXT("|") + GetPathNameSafe(Pair.Key->DefaultObject), Pair.Value);
    for (const auto& Pair : ExistingPositions) if (!RuleNodes.Contains(Pair.Key))
        TestEqual(TEXT("Rule tidy preserves unrelated positions"), FIntPoint(Pair.Key->NodePosX, Pair.Key->NodePosY), Pair.Value);
    for (UEdGraphNode* Node : RuleNodes)
        for (UEdGraphPin* Pin : Node->Pins) if (Pin && Pin->Direction == EGPD_Output)
            for (UEdGraphPin* Input : Pin->LinkedTo)
                TestTrue(TEXT("Rule data wires flow forward, including fixed Return"),
                    FTurboMCPGraphTidy::AlignmentPinPosition(Pin).X < FTurboMCPGraphTidy::AlignmentPinPosition(Input).X);
    Record(TEXT("ReadTurboMCPGraphLoad appended"), ReadGraph());
    Record(TEXT("EditTurboMCPCompile appended"), UTurboMCPEditToolset::EditTurboMCPCompile(Asset));
    UFunction* PurchaseFunction = Blueprint->GeneratedClass->FindFunctionByName(*GraphName);
    if (!TestNotNull(TEXT("Appended function is executable"), PurchaseFunction)) return false;
    for (int32 Case = 0; Case < 4; ++Case)
    {
        FStructOnScope Args(PurchaseFunction);
        const double Funds = Case == 2 ? 80.0 : 200.0;
        const double Quantity = Case == 3 ? 3.0 : 2.0;
        const double Minimum = Case == 1 ? 3.0 : 2.0;
        for (const auto& Pair : TMap<FName, double>{{TEXT("UnitPrice"),50.0},{TEXT("Quantity"),Quantity},{TEXT("DiscountPercent"),10.0},{TEXT("AvailableFunds"),Funds},{TEXT("MinimumQuantity"),Minimum}})
        {
            const FDoubleProperty* Property = FindFProperty<FDoubleProperty>(PurchaseFunction, Pair.Key);
            if (!TestNotNull(TEXT("Scenario input exists"), Property)) return false;
            Property->SetPropertyValue_InContainer(Args.GetStructMemory(), Pair.Value);
        }
        Blueprint->GeneratedClass->GetDefaultObject()->ProcessEvent(PurchaseFunction, Args.GetStructMemory());
        TestEqual(TEXT("Minimum quantity does not change price"), FindFProperty<FDoubleProperty>(PurchaseFunction,TEXT("FinalPrice"))->GetPropertyValue_InContainer(Args.GetStructMemory()),Quantity * 45.0);
        TestEqual(TEXT("Both quantity and funds must pass"), FindFProperty<FBoolProperty>(PurchaseFunction,TEXT("CanAfford"))->GetPropertyValue_InContainer(Args.GetStructMemory()),Case == 0 || Case == 3);
    }
    TestEqual(TEXT("Append leaves unrelated function unchanged"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, Untouched->GetName(), TEXT("read"), TEXT("full")), UnrelatedBefore);
    Record(TEXT("LookTurboMCPCapture appended"), UTurboMCPLookToolset::LookTurboMCPCapture(Asset, GraphName, TEXT("graph"), TEXT(""), TEXT("1600x900")));
    FString GeometryError;
    if (TestTrue(TEXT("Final native geometry is available"), FTurboMCPGraphTidy::PrepareAlignment(Blueprint, Graph, GeometryError)))
        for (UEdGraphNode* NewNode : Graph->Nodes)
            if (RuleNodes.Contains(NewNode))
            {
                const FVector2D Size = FTurboMCPGraphTidy::AlignmentNodeSize(NewNode);
                for (UEdGraphNode* Other : Graph->Nodes) if (Other && Other != NewNode)
                {
                    const FVector2D OtherSize = FTurboMCPGraphTidy::AlignmentNodeSize(Other);
                    TestFalse(TEXT("Tidied rule node has no native rectangle overlap"),
                        NewNode->NodePosX < Other->NodePosX + OtherSize.X && NewNode->NodePosX + Size.X > Other->NodePosX
                        && NewNode->NodePosY < Other->NodePosY + OtherSize.Y && NewNode->NodePosY + Size.Y > Other->NodePosY);
                }
            }

    return !HasAnyErrors();
}
#endif

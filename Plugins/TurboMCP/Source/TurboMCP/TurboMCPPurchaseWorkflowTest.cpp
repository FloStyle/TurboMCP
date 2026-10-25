// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "BlueprintEditorLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
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
    for (const TCHAR* Name : {TEXT("UnitPrice"), TEXT("Quantity"), TEXT("DiscountPercent"), TEXT("AvailableFunds")})
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
    const FString Before = Record(TEXT("ReadTurboMCPGraphLoad baseline"), ReadGraph());
    const FString Overview = Record(TEXT("ReadTurboMCPBlueprintLoad overview"), UTurboMCPReadToolset::ReadTurboMCPBlueprintLoad(Asset, TEXT("signatures"), TEXT(""), TEXT("read"), TEXT("60000")));
    TestTrue(TEXT("Overview names purchase function"), Overview.Contains(GraphName));
    TestTrue(TEXT("Overview names unrelated function"), Overview.Contains(Untouched->GetName()));
    TestTrue(TEXT("Detailed read includes supplied context"), Before.Contains(TEXT("UnitPrice")) && Before.Contains(TEXT("DiscountPercent")));
    TestEqual(TEXT("Overview does not change graph content"), ReadGraph(), Before);
    Record(TEXT("ReadTurboMCPNodeFind"), UTurboMCPReadToolset::ReadTurboMCPNodeFind(Asset, TEXT("Multiply"), GraphName));
    const FString RateId = TurboMCPNodeId::Make(Asset, GraphName, RateGuid, true);
    Record(TEXT("ReadTurboMCPNodeContext"), UTurboMCPReadToolset::ReadTurboMCPNodeContext(Asset, GraphName, RateId, 3));
    Record(TEXT("LookTurboMCPGraphPositions before"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, GraphName));
    Record(TEXT("LookTurboMCPLayoutReport before"), UTurboMCPLookToolset::LookTurboMCPLayoutReport(Asset, GraphName));
    Record(TEXT("LookTurboMCPCapture before"), UTurboMCPLookToolset::LookTurboMCPCapture(Asset, GraphName, TEXT("graph"), TEXT(""), TEXT("1600x900")));
    const FString MissingGraph = Record(TEXT("GraphLoad missing graph"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, TEXT("MissingGraph"), TEXT("read"), TEXT("full")));
    TestTrue(TEXT("Missing graph gives actual choices"), MissingGraph.StartsWith(TEXT("error:")) && MissingGraph.Contains(GraphName));
    const FString Page = Record(TEXT("GraphLoad first page"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, GraphName, TEXT("read"), TEXT("full"), 0, 256));
    TestTrue(TEXT("Page describes continuation"), Page.Contains(TEXT("revision=")) && Page.Contains(TEXT("nextOffset=")));
    TestTrue(TEXT("Stale continuation refuses"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, GraphName, TEXT("read"), TEXT("full"), 256, 256, TEXT("stale")).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Refused and paged reads preserve graph"), ReadGraph(), Before);
    const FString Fixed = Record(TEXT("EditTurboMCPPinDefault"), UTurboMCPEditToolset::EditTurboMCPPinDefault(Asset, GraphName, RateId, TEXT("B"), TEXT("0.01")));
    TestTrue(TEXT("Targeted default edit reports change"), Fixed.StartsWith(TEXT("changed:")));
    TestEqual(TEXT("Default edit preserves node count"), Graph->Nodes.Num(), OriginalCount);
    Record(TEXT("EditTurboMCPCompile corrected"), UTurboMCPEditToolset::EditTurboMCPCompile(Asset));
    if (!CheckValues(true)) return false;
    Record(TEXT("ReadTurboMCPGraphLoad corrected"), ReadGraph());
    Record(TEXT("PreviewTurboMCPLocalVars"), UTurboMCPPreviewToolset::PreviewTurboMCPLocalVars(Asset, GraphName));
    const FString Locals = Record(TEXT("EditTurboMCPLocalVars"), UTurboMCPEditToolset::EditTurboMCPLocalVars(Asset, GraphName));
    TestTrue(TEXT("Supported repeated arithmetic is promoted"), Locals.StartsWith(TEXT("changed:")));
    if (!CheckValues(true)) return false;
    Record(TEXT("ReadTurboMCPGraphLoad locals"), ReadGraph());
    const TArray<FString> WholeGraph;
    Record(TEXT("PreviewTurboMCPGraphTidy"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(Asset, GraphName, false, WholeGraph));
    const FString Tidy = Record(TEXT("EditTurboMCPGraphTidy"), UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset, GraphName, false, WholeGraph));
    TestFalse(TEXT("Tidy does not return an error"), Tidy.StartsWith(TEXT("error:")));
    if (!CheckValues(true)) return false;
    const FString NodeIds = TurboMCPNodeId::Make(Asset, GraphName, RateGuid, true);
    Record(TEXT("PreviewTurboMCPComment"), UTurboMCPPreviewToolset::PreviewTurboMCPComment(Asset, GraphName, TEXT("create"), NodeIds, TEXT(""), TEXT("Convert discount percent to a fraction"), TEXT(""), TEXT("")));
    Record(TEXT("EditTurboMCPComment"), UTurboMCPEditToolset::EditTurboMCPComment(Asset, GraphName, TEXT("create"), NodeIds, TEXT(""), TEXT("Convert discount percent to a fraction"), TEXT(""), TEXT("")));
    Record(TEXT("ReadTurboMCPCommentList"), UTurboMCPReadToolset::ReadTurboMCPCommentList(Asset, GraphName));
    Record(TEXT("ReadTurboMCPGraphLoad final"), ReadGraph());
    Record(TEXT("LookTurboMCPLayoutDoneCheck"), UTurboMCPLookToolset::LookTurboMCPLayoutDoneCheck(Asset, GraphName));
    TestEqual(TEXT("Unrelated function remains unchanged"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, Untouched->GetName(), TEXT("read"), TEXT("full")), UnrelatedBefore);
    return !HasAnyErrors();
}
#endif

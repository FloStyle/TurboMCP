// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_VariableGet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPVariableRemoveTest,
    "TurboMCP.GraphAppend.VariableRemove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPVariableRemoveTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/VariableRemove_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(UUserWidget::StaticClass(), Package, TEXT("WBP_Remove"), BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass());
    if (BP == nullptr) { AddError(TEXT("Widget fixture creation failed.")); return false; }
    FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Int;
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("UnusedValue"), Type, TEXT("7"));
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("KeepValue"), Type, TEXT("9"));
    FKismetEditorUtilities::CompileBlueprint(BP);
    const FString Asset = BP->GetPathName();
    auto Preview = [&]() { return UTurboMCPPreviewToolset::PreviewTurboMCPVariableRemove(Asset, TEXT("UnusedValue")); };
    auto Token = [](const FString& Text) { FString Before, After; Text.Split(TEXT("\nconfirm: "), &Before, &After); FString Result; After.Split(TEXT("\n"), &Result, nullptr); return Result; };
    Package->SetDirtyFlag(false);
    const FString First = Preview(); AddInfo(First);
    TestTrue(TEXT("Unused preview succeeds"), First.StartsWith(TEXT("preview:")));
    TestEqual(TEXT("Preview preserves declarations"), BP->NewVariables.Num(), 2);
    TestFalse(TEXT("Preview preserves clean package"), Package->IsDirty());
    TestTrue(TEXT("Wrong confirmation refuses"), UTurboMCPEditToolset::EditTurboMCPVariableRemove(Asset, TEXT("UnusedValue"), TEXT("wrong")).StartsWith(TEXT("error:")));
    BP->NewVariables[0].DefaultValue = TEXT("8");
    TestTrue(TEXT("Changed default invalidates confirmation"), UTurboMCPEditToolset::EditTurboMCPVariableRemove(Asset, TEXT("UnusedValue"), Token(First)).Contains(TEXT("stale")));
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("ReferenceCheck"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph);
    Getter->VariableReference.SetSelfMember(TEXT("UnusedValue"));
    Getter->CreateNewGuid(); Graph->AddNode(Getter, false, false); Getter->AllocateDefaultPins();
    TestTrue(TEXT("Disconnected getter prevents removal"), Preview().Contains(TEXT("referenced by node")));
    TestEqual(TEXT("Refusal retains getter"), Graph->Nodes.Num(), 1);
    TestEqual(TEXT("Refusal retains declarations"), BP->NewVariables.Num(), 2);
    Graph->RemoveNode(Getter);
    const FString FinalPreview = Preview(); AddInfo(FinalPreview);
    const FString Edited = UTurboMCPEditToolset::EditTurboMCPVariableRemove(Asset, TEXT("UnusedValue"), Token(FinalPreview)); AddInfo(Edited);
    TestTrue(TEXT("Confirmed unused removal succeeds"), Edited.StartsWith(TEXT("edited:")));
    TestEqual(TEXT("Only one declaration removed"), BP->NewVariables.Num(), 1);
    if (BP->NewVariables.Num() == 1) TestEqual(TEXT("Unrelated declaration retained"), BP->NewVariables[0].VarName, FName(TEXT("KeepValue")));
    TestTrue(TEXT("Missing variable refuses"), Preview().StartsWith(TEXT("error:")));
    return !HasAnyErrors();
}
#endif

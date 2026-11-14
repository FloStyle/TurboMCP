// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
namespace TurboMCPVariableRemoval { FString ReferenceLoadBlocker(IAssetRegistry& Registry, FName Seed); }
#include "Misc/ScopeExit.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "BlueprintEditorLibrary.h"
#include "K2Node_VariableSet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPVariableRemoveTest,
    "TurboMCP.GraphAppend.VariableRemove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPVariableRemoveTest::RunTest(const FString& Parameters)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TestTrue(TEXT("Unknown package refuses reference loading"),
        TurboMCPVariableRemoval::ReferenceLoadBlocker(Registry, FName(TEXT("/Game/TurboMCP_NoSuchReferencePackage_47B84"))).Contains(TEXT("metadata unavailable")));
    TestTrue(TEXT("Known fixture map refuses reference loading"),
        TurboMCPVariableRemoval::ReferenceLoadBlocker(Registry, FName(TEXT("/Game/Tests/TurboMCPTestMap"))).Contains(TEXT("map or redirector")));

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
    // Keep an unrelated data connection present across structural refresh.
    UK2Node_VariableGet* KeepGet = NewObject<UK2Node_VariableGet>(Graph);
    KeepGet->VariableReference.SetSelfMember(TEXT("KeepValue"));
    KeepGet->CreateNewGuid(); Graph->AddNode(KeepGet, false, false); KeepGet->AllocateDefaultPins();
    UK2Node_VariableSet* KeepSet = NewObject<UK2Node_VariableSet>(Graph);
    KeepSet->VariableReference.SetSelfMember(TEXT("KeepValue"));
    KeepSet->CreateNewGuid(); Graph->AddNode(KeepSet, false, false); KeepSet->AllocateDefaultPins();
    UEdGraphPin* Output = KeepGet->FindPin(TEXT("KeepValue"), EGPD_Output);
    UEdGraphPin* Input = KeepSet->FindPin(TEXT("KeepValue"), EGPD_Input);
    if (!Output || !Input) { AddError(TEXT("Unrelated fixture pins are missing.")); return false; }
    Output->MakeLinkTo(Input);
    const FGuid GetId = KeepGet->NodeGuid, SetId = KeepSet->NodeGuid;
    const FString FinalPreview = Preview(); AddInfo(FinalPreview);
    const FString Edited = UTurboMCPEditToolset::EditTurboMCPVariableRemove(Asset, TEXT("UnusedValue"), Token(FinalPreview)); AddInfo(Edited);
    TestTrue(TEXT("Confirmed unused removal succeeds"), Edited.StartsWith(TEXT("edited:")));
    TestEqual(TEXT("Only one declaration removed"), BP->NewVariables.Num(), 1);
    if (BP->NewVariables.Num() == 1) TestEqual(TEXT("Unrelated declaration retained"), BP->NewVariables[0].VarName, FName(TEXT("KeepValue")));
    TestTrue(TEXT("Missing variable refuses"), Preview().StartsWith(TEXT("error:")));
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    UK2Node_VariableGet* PreservedGet = nullptr;
    UK2Node_VariableSet* PreservedSet = nullptr;
    for (UEdGraph* Current : Graphs) if (Current) for (UEdGraphNode* Node : Current->Nodes) if (Node)
    {
        if (Node->NodeGuid == GetId) PreservedGet = Cast<UK2Node_VariableGet>(Node);
        if (Node->NodeGuid == SetId) PreservedSet = Cast<UK2Node_VariableSet>(Node);
    }
    if (!PreservedGet || !PreservedSet) { AddError(TEXT("Unrelated nodes disappeared.")); return false; }
    UEdGraphPin* PreservedOutput = PreservedGet->FindPin(TEXT("KeepValue"), EGPD_Output);
    UEdGraphPin* PreservedInput = PreservedSet->FindPin(TEXT("KeepValue"), EGPD_Input);
    TestTrue(TEXT("Unrelated link preserved"), PreservedOutput && PreservedInput && PreservedOutput->LinkedTo.Contains(PreservedInput));
    UBlueprint* Child = FKismetEditorUtilities::CreateBlueprint(BP->GeneratedClass, Package, TEXT("WBP_Child"), BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass());
    if (!Child) { AddError(TEXT("Inherited fixture creation failed.")); return false; }
    TestTrue(TEXT("Inherited member refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPVariableRemove(Child->GetPathName(), TEXT("KeepValue")).Contains(TEXT("Inherited")));
    const FString Help = UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCPEditToolset"), TEXT("EditTurboMCPVariableRemove"));
    AddInfo(Help);
    TestTrue(TEXT("Help explains confirmation"), Help.Contains(TEXT("Confirm")) && Help.Contains(TEXT("token")));

    // Same-name declarations in other scopes must survive local removal.
    UEdGraph* LocalGraph = UBlueprintEditorLibrary::AddFunctionGraph(BP, TEXT("LocalScope"));
    UEdGraph* OtherGraph = UBlueprintEditorLibrary::AddFunctionGraph(BP, TEXT("OtherScope"));
    if (!LocalGraph || !OtherGraph) { AddError(TEXT("Local function fixtures are missing.")); return false; }
    TArray<UK2Node_FunctionEntry*> LocalEntries, OtherEntries;
    LocalGraph->GetNodesOfClass(LocalEntries);
    OtherGraph->GetNodesOfClass(OtherEntries);
    if (LocalEntries.Num() != 1 || OtherEntries.Num() != 1) { AddError(TEXT("Fixture requires one entry per function.")); return false; }
    FBPVariableDescription Local;
    Local.VarName = TEXT("KeepValue"); Local.VarGuid = FGuid::NewGuid(); Local.VarType = Type; Local.DefaultValue = TEXT("3");
    LocalEntries[0]->LocalVariables.Add(Local);
    FBPVariableDescription Other = Local; Other.VarGuid = FGuid::NewGuid();
    OtherEntries[0]->LocalVariables.Add(Other);
    FKismetEditorUtilities::CompileBlueprint(BP);
    auto LocalPreview = [&]() { return UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableRemove(Asset, TEXT("LocalScope"), TEXT("KeepValue")); };
    const FString InitialLocalPreview = LocalPreview();
    TestTrue(TEXT("Unused local preview succeeds despite same-name member and other local"), InitialLocalPreview.StartsWith(TEXT("preview:")));
    LocalEntries[0]->LocalVariables[0].DefaultValue = TEXT("4");
    TestTrue(TEXT("Local stale token refuses"), UTurboMCPEditToolset::EditTurboMCPLocalVariableRemove(Asset, TEXT("LocalScope"), TEXT("KeepValue"), Token(InitialLocalPreview)).Contains(TEXT("stale")));
    UK2Node_VariableGet* LocalGetter = NewObject<UK2Node_VariableGet>(LocalGraph);
    LocalGetter->VariableReference.SetLocalMember(Local.VarName, LocalGraph->GetName(), Local.VarGuid);
    LocalGetter->CreateNewGuid(); LocalGraph->AddNode(LocalGetter, false, false); LocalGetter->AllocateDefaultPins();
    const int32 NodeCount = LocalGraph->Nodes.Num();
    TestTrue(TEXT("Disconnected local getter refuses"), LocalPreview().Contains(TEXT("referenced")));
    TestEqual(TEXT("Refusal leaves local graph nodes"), LocalGraph->Nodes.Num(), NodeCount);
    TestEqual(TEXT("Refusal leaves local declaration"), LocalEntries[0]->LocalVariables.Num(), 1);
    LocalGraph->RemoveNode(LocalGetter);
    const FString ReadyLocal = LocalPreview();
    const FString RemovedLocal = UTurboMCPEditToolset::EditTurboMCPLocalVariableRemove(Asset, TEXT("LocalScope"), TEXT("KeepValue"), Token(ReadyLocal));
    AddInfo(RemovedLocal);
    TestTrue(TEXT("Confirmed unused local removal succeeds"), RemovedLocal.StartsWith(TEXT("edited:")));
    TestEqual(TEXT("Local declaration removed"), LocalEntries[0]->LocalVariables.Num(), 0);
    TestEqual(TEXT("Other function local retained"), OtherEntries[0]->LocalVariables.Num(), 1);
    TestEqual(TEXT("Same-name member retained"), BP->NewVariables.Num(), 1);
    TestTrue(TEXT("Missing local refuses"), LocalPreview().StartsWith(TEXT("error:")));
    TestTrue(TEXT("Event scope refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableRemove(Asset, TEXT("EventGraph"), TEXT("KeepValue")).StartsWith(TEXT("error:")));
    const FString LocalHelp = UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCPEditToolset"), TEXT("EditTurboMCPLocalVariableRemove"));
    TestTrue(TEXT("Local help documents token"), LocalHelp.Contains(TEXT("Confirm")) && LocalHelp.Contains(TEXT("token")));

    LocalEntries[0]->LocalVariables.Add(Local);
    FBlueprintEditorUtils::AddMemberVariable(Child, TEXT("RenamedLocal"), Type, TEXT("12"));
    FKismetEditorUtilities::CompileBlueprint(Child);
    FKismetEditorUtilities::CompileBlueprint(BP);
    UK2Node_VariableGet* RenameGet = NewObject<UK2Node_VariableGet>(LocalGraph);
    UK2Node_VariableSet* RenameSet = NewObject<UK2Node_VariableSet>(LocalGraph);
    UK2Node_VariableGet* MemberGet = NewObject<UK2Node_VariableGet>(LocalGraph);
    RenameGet->VariableReference.SetLocalMember(Local.VarName, LocalGraph->GetName(), Local.VarGuid);
    RenameSet->VariableReference.SetLocalMember(Local.VarName, LocalGraph->GetName(), Local.VarGuid);
    MemberGet->VariableReference.SetSelfMember(Local.VarName);
    for (UK2Node_Variable* Node : {static_cast<UK2Node_Variable*>(RenameGet), static_cast<UK2Node_Variable*>(RenameSet), static_cast<UK2Node_Variable*>(MemberGet)})
    { Node->CreateNewGuid(); LocalGraph->AddNode(Node, false, false); Node->AllocateDefaultPins(); }
    UEdGraphPin* RenameOutput = RenameGet->FindPin(Local.VarName, EGPD_Output);
    UEdGraphPin* RenameInput = RenameSet->FindPin(Local.VarName, EGPD_Input);
    if (!RenameOutput || !RenameInput) { AddError(TEXT("Rename fixture pins are missing.")); return false; }
    RenameOutput->MakeLinkTo(RenameInput);
    RenameInput->DefaultValue = TEXT("17");
    const FGuid OutputId = RenameOutput->PinId, InputId = RenameInput->PinId;
    auto RenamePreview = [&]() { return UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableRename(Asset, TEXT("LocalScope"), TEXT("KeepValue"), TEXT("RenamedLocal")); };
    const FString FirstRename = RenamePreview();
    AddInfo(FirstRename);
    TestTrue(TEXT("Local rename preview succeeds"), FirstRename.StartsWith(TEXT("preview:")));
    RenameSet->NodePosX += 32;
    TestTrue(TEXT("Rename stale confirmation refuses"), UTurboMCPEditToolset::EditTurboMCPLocalVariableRename(Asset, TEXT("LocalScope"), TEXT("KeepValue"), TEXT("RenamedLocal"), Token(FirstRename)).Contains(TEXT("stale")));
    const FString FreshRename = RenamePreview();
    const FString RenameResult = UTurboMCPEditToolset::EditTurboMCPLocalVariableRename(Asset, TEXT("LocalScope"), TEXT("KeepValue"), TEXT("RenamedLocal"), Token(FreshRename));
    AddInfo(RenameResult);
    TestTrue(TEXT("Local rename succeeds"), RenameResult.StartsWith(TEXT("edited:")));
    TestEqual(TEXT("Local GUID preserved"), LocalEntries[0]->LocalVariables[0].VarGuid, Local.VarGuid);
    TestEqual(TEXT("Local declaration renamed"), LocalEntries[0]->LocalVariables[0].VarName, FName(TEXT("RenamedLocal")));
    TestEqual(TEXT("Local getter renamed"), RenameGet->VariableReference.GetMemberName(), FName(TEXT("RenamedLocal")));
    TestEqual(TEXT("Local setter renamed"), RenameSet->VariableReference.GetMemberName(), FName(TEXT("RenamedLocal")));
    TestEqual(TEXT("Same-name member getter untouched"), MemberGet->VariableReference.GetMemberName(), Local.VarName);
    TestEqual(TEXT("Other function local untouched"), OtherEntries[0]->LocalVariables[0].VarName, Local.VarName);
    TestEqual(TEXT("Child member untouched"), Child->NewVariables[0].VarName, FName(TEXT("RenamedLocal")));
    UEdGraphPin* RenamedOutput = RenameGet->FindPin(TEXT("RenamedLocal"), EGPD_Output);
    UEdGraphPin* RenamedInput = RenameSet->FindPin(TEXT("RenamedLocal"), EGPD_Input);
    TestTrue(TEXT("Renamed pins preserve identities, connection and literal"),
        RenamedOutput && RenamedInput && RenamedOutput->PinId == OutputId && RenamedInput->PinId == InputId
        && RenamedOutput->LinkedTo.Contains(RenamedInput) && RenamedInput->DefaultValue == TEXT("17"));
    TestTrue(TEXT("Invalid new name refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableRename(Asset, TEXT("LocalScope"), TEXT("RenamedLocal"), TEXT("")).StartsWith(TEXT("error:")));
    const FString RenameHelp = UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCPEditToolset"), TEXT("EditTurboMCPLocalVariableRename"));
    TestTrue(TEXT("Rename help documents scope and confirmation"), RenameHelp.Contains(TEXT("Function")) && RenameHelp.Contains(TEXT("Confirm")));
    return !HasAnyErrors();
}
#endif

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

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_GetClassDefaults.h"
#include "UObject/UnrealType.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPMemberRenameTest,
    "TurboMCP.Variables.MemberRename", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPMemberRenameTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/MemberRename_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* Owner = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_Owner"), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!Owner) { AddError(TEXT("Owner fixture creation failed.")); return false; }
    FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Int;
    FBlueprintEditorUtils::AddMemberVariable(Owner, TEXT("Value"), Type, TEXT("7"));
    UEdGraph* NotifyGraph = UBlueprintEditorLibrary::AddFunctionGraph(Owner, TEXT("OnRep_Value"));
    if (!NotifyGraph) { AddError(TEXT("RepNotify fixture graph is missing.")); return false; }
    Owner->NewVariables[0].PropertyFlags |= CPF_Net | CPF_RepNotify;
    Owner->NewVariables[0].RepNotifyFunc = TEXT("OnRep_Value");
    Owner->NewVariables[0].SetMetaData(TEXT("Tooltip"), TEXT("Preserve this member description."));
    FKismetEditorUtilities::CompileBlueprint(Owner);
    auto SetDefault = [](UBlueprint* BP, FName Name, int32 Value)
    {
        UObject* CDO = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr;
        FIntProperty* Property = CDO ? FindFProperty<FIntProperty>(CDO->GetClass(), Name) : nullptr;
        if (!Property) return false;
        Property->SetPropertyValue_InContainer(CDO, Value); return true;
    };
    // Simulate a default edited in the Details panel after the last declaration
    // export. The writer must preserve 42, not the stale declaration text "7".
    if (!SetDefault(Owner, TEXT("Value"), 42)) { AddError(TEXT("Compiled owner default is missing.")); return false; }
    UBlueprint* Child = FKismetEditorUtilities::CreateBlueprint(Owner->GeneratedClass, Package, TEXT("BP_Child"), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UBlueprint* Caller = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(), Package, TEXT("BP_Caller"), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!Child || !Caller) { AddError(TEXT("Dependent fixtures are missing.")); return false; }
    FBlueprintEditorUtils::AddMemberVariable(Caller, TEXT("Value"), Type, TEXT("19"));
    FKismetEditorUtilities::CompileBlueprint(Child); FKismetEditorUtilities::CompileBlueprint(Caller);
    const FGuid Guid = Owner->NewVariables[0].VarGuid;
    const auto Flags = Owner->NewVariables[0].PropertyFlags;
    auto Graph = [](UBlueprint* BP, const TCHAR* Name)
    {
        UEdGraph* Result = FBlueprintEditorUtils::CreateNewGraph(BP, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        BP->UbergraphPages.Add(Result); return Result;
    };
    UEdGraph* OwnerGraph = Graph(Owner, TEXT("References"));
    UEdGraph* ChildGraph = Graph(Child, TEXT("References"));
    UEdGraph* CallerGraph = Graph(Caller, TEXT("References"));
    auto Get = [](UEdGraph* InGraph, UClass* External, FGuid Identity)
    {
        auto* Node = NewObject<UK2Node_VariableGet>(InGraph);
        if (External) Node->VariableReference.SetExternalMember(TEXT("Value"), External, Identity);
        else Node->VariableReference.SetSelfMember(TEXT("Value"), Identity);
        Node->CreateNewGuid(); InGraph->AddNode(Node, false, false); Node->AllocateDefaultPins(); return Node;
    };
    UK2Node_VariableGet* OwnerGet = Get(OwnerGraph, nullptr, Guid);
    UK2Node_VariableGet* ChildGet = Get(ChildGraph, nullptr, Guid);
    UK2Node_VariableGet* CallerGet = Get(CallerGraph, Owner->GeneratedClass, Guid);
    UK2Node_VariableGet* UnrelatedGet = Get(CallerGraph, nullptr, Caller->NewVariables[0].VarGuid);
    auto* Setter = NewObject<UK2Node_VariableSet>(OwnerGraph);
    Setter->VariableReference.SetSelfMember(TEXT("Value"), Guid);
    Setter->CreateNewGuid(); OwnerGraph->AddNode(Setter, false, false); Setter->AllocateDefaultPins();
    UEdGraphPin* Out = OwnerGet->FindPin(TEXT("Value"), EGPD_Output);
    UEdGraphPin* In = Setter->FindPin(TEXT("Value"), EGPD_Input);
    if (!Out || !In) { AddError(TEXT("Member pins are missing.")); return false; }
    Out->MakeLinkTo(In); In->DefaultValue = TEXT("23");
    const FGuid OutId = Out->PinId, InId = In->PinId;
    OwnerGet->NodePosX = 144; Setter->NodePosX = 448;
    UEdGraph* LocalGraph = UBlueprintEditorLibrary::AddFunctionGraph(Owner, TEXT("LocalScope"));
    if (!LocalGraph) { AddError(TEXT("Local scope creation failed.")); return false; }
    TArray<UK2Node_FunctionEntry*> Entries; LocalGraph->GetNodesOfClass(Entries);
    if (Entries.Num() != 1) { AddError(TEXT("Local entry is missing.")); return false; }
    FBPVariableDescription Local; Local.VarName = TEXT("Value"); Local.VarGuid = FGuid::NewGuid(); Local.VarType = Type;
    Entries[0]->LocalVariables.Add(Local);
    auto* LocalGet = NewObject<UK2Node_VariableGet>(LocalGraph);
    LocalGet->VariableReference.SetLocalMember(Local.VarName, LocalGraph->GetName(), Local.VarGuid);
    LocalGet->CreateNewGuid(); LocalGraph->AddNode(LocalGet, false, false); LocalGet->AllocateDefaultPins();
    auto Token = [](const FString& Text)
    { FString Before, After, Result; Text.Split(TEXT("\nconfirm: "), &Before, &After); After.Split(TEXT("\n"), &Result, nullptr); return Result; };
    auto Preview = [&]() { return UTurboMCPPreviewToolset::PreviewTurboMCPVariableRename(Owner->GetPathName(), TEXT("Value"), TEXT("RenamedValue")); };
    auto Edit = [&](const FString& Confirm)
    { return UTurboMCPEditToolset::EditTurboMCPVariableRename(Owner->GetPathName(), TEXT("Value"), TEXT("RenamedValue"), Confirm); };
    Package->SetDirtyFlag(false);
    const FString First = Preview(); AddInfo(First);
    TestTrue(TEXT("Member preview succeeds"), First.StartsWith(TEXT("preview:")));
    TestTrue(TEXT("Preview lists owner, child and external caller"), First.Contains(Owner->GetPathName()) && First.Contains(Child->GetPathName()) && First.Contains(Caller->GetPathName()));
    TestFalse(TEXT("Preview leaves package clean"), Package->IsDirty());
    TestEqual(TEXT("Preview leaves declaration unchanged"), Owner->NewVariables[0].VarName, FName(TEXT("Value")));
    SetDefault(Owner, TEXT("Value"), 43); SetDefault(Child, TEXT("Value"), 43);
    TestTrue(TEXT("Changed CDO invalidates token"), Edit(Token(First)).Contains(TEXT("stale")));
    SetDefault(Owner, TEXT("Value"), 42); SetDefault(Child, TEXT("Value"), 42);
    FBPVariableDescription Collision; Collision.VarName = TEXT("RenamedValue"); Collision.VarGuid = FGuid::NewGuid(); Collision.VarType = Type;
    Child->NewVariables.Add(Collision);
    TestTrue(TEXT("Child collision refuses without renaming child"), Preview().Contains(TEXT("collision")));
    TestEqual(TEXT("Conflicting child declaration retained"), Child->NewVariables.Last().VarName, Collision.VarName);
    Child->NewVariables.Pop();
    SetDefault(Child, TEXT("Value"), 99);
    TestTrue(TEXT("Child override refuses explicit migration gap"), Preview().Contains(TEXT("Child default override")));
    SetDefault(Child, TEXT("Value"), 42);
    TestTrue(TEXT("Case-only rename refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPVariableRename(Owner->GetPathName(), TEXT("Value"), TEXT("value")).Contains(TEXT("Case-only")));
    auto* Defaults = NewObject<UK2Node_GetClassDefaults>(CallerGraph);
    Defaults->CreateNewGuid(); CallerGraph->AddNode(Defaults, false, false); Defaults->AllocateDefaultPins();
    UEdGraphPin* ClassPin = Defaults->FindPin(TEXT("Class"));
    if (!ClassPin) { AddError(TEXT("Class-defaults input is missing.")); return false; }
    ClassPin->DefaultObject = Owner->GeneratedClass; Defaults->PinDefaultValueChanged(ClassPin);
    TestTrue(TEXT("Class-defaults reference refuses"), Preview().Contains(TEXT("Unsupported member-reference")));
    CallerGraph->RemoveNode(Defaults);
    const FString Fresh = Preview(); AddInfo(Fresh);
    const FString Result = Edit(Token(Fresh)); AddInfo(Result);
    TestTrue(TEXT("Confirmed member rename succeeds"), Result.StartsWith(TEXT("edited:")));
    TestEqual(TEXT("GUID preserved"), Owner->NewVariables[0].VarGuid, Guid);
    TestEqual(TEXT("Flags preserved"), Owner->NewVariables[0].PropertyFlags, Flags);
    TestEqual(TEXT("RepNotify association preserved"), Owner->NewVariables[0].RepNotifyFunc, FName(TEXT("OnRep_Value")));
    TestEqual(TEXT("Metadata preserved"), Owner->NewVariables[0].GetMetaData(TEXT("Tooltip")), FString(TEXT("Preserve this member description.")));
    TestEqual(TEXT("Effective default captured"), Owner->NewVariables[0].DefaultValue, FString(TEXT("42")));
    for (UK2Node_VariableGet* Node : {OwnerGet, ChildGet, CallerGet})
    { TestEqual(TEXT("Member reference renamed"), Node->VariableReference.GetMemberName(), FName(TEXT("RenamedValue"))); TestEqual(TEXT("Reference identity preserved"), Node->VariableReference.GetMemberGuid(), Guid); }
    TestEqual(TEXT("Unrelated member untouched"), UnrelatedGet->VariableReference.GetMemberName(), FName(TEXT("Value")));
    TestEqual(TEXT("Local untouched"), LocalGet->VariableReference.GetMemberName(), FName(TEXT("Value")));
    TestEqual(TEXT("Node position preserved"), OwnerGet->NodePosX, 144);
    UEdGraphPin* RenamedOut = OwnerGet->FindPin(TEXT("RenamedValue"), EGPD_Output);
    UEdGraphPin* RenamedIn = Setter->FindPin(TEXT("RenamedValue"), EGPD_Input);
    TestTrue(TEXT("Pin identities, links and literal preserved"), RenamedOut && RenamedIn && RenamedOut->PinId == OutId
        && RenamedIn->PinId == InId && RenamedOut->LinkedTo.Contains(RenamedIn) && RenamedIn->DefaultValue == TEXT("23"));
    // Only compile after checking the edit delta. Never let a compile normalize
    // an incorrect edit and turn that into a passing preservation assertion.
    FKismetEditorUtilities::CompileBlueprint(Owner);
    FKismetEditorUtilities::CompileBlueprint(Child);
    FKismetEditorUtilities::CompileBlueprint(Caller);
    TestTrue(TEXT("Owner and dependents compile"), Owner->Status != BS_Error && Child->Status != BS_Error && Caller->Status != BS_Error);
    UObject* CDO = Owner->GeneratedClass ? Owner->GeneratedClass->GetDefaultObject(false) : nullptr;
    FIntProperty* Property = CDO ? FindFProperty<FIntProperty>(CDO->GetClass(), TEXT("RenamedValue")) : nullptr;
    TestTrue(TEXT("Compiled renamed default survives"), Property && Property->GetPropertyValue_InContainer(CDO) == 42);
    UBlueprint* VectorOwner = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(), Package, TEXT("BP_Vector"), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!VectorOwner) { AddError(TEXT("Split-pin fixture creation failed.")); return false; }
    FEdGraphPinType VectorType; VectorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    VectorType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    FBlueprintEditorUtils::AddMemberVariable(VectorOwner, TEXT("Value"), VectorType);
    FKismetEditorUtilities::CompileBlueprint(VectorOwner);
    UEdGraph* VectorGraph = Graph(VectorOwner, TEXT("VectorReferences"));
    auto* VectorGet = Get(VectorGraph, nullptr, VectorOwner->NewVariables[0].VarGuid);
    auto* VectorSet = NewObject<UK2Node_VariableSet>(VectorGraph);
    VectorSet->VariableReference.SetSelfMember(TEXT("Value"), VectorOwner->NewVariables[0].VarGuid);
    VectorSet->CreateNewGuid(); VectorGraph->AddNode(VectorSet, false, false); VectorSet->AllocateDefaultPins();
    UEdGraphPin* VectorOut = VectorGet->FindPin(TEXT("Value"), EGPD_Output);
    UEdGraphPin* VectorIn = VectorSet->FindPin(TEXT("Value"), EGPD_Input);
    if (!VectorOut || !VectorIn) { AddError(TEXT("Vector pins are missing.")); return false; }
    GetDefault<UEdGraphSchema_K2>()->SplitPin(VectorOut, false);
    GetDefault<UEdGraphSchema_K2>()->SplitPin(VectorIn, false);
    if (VectorOut->SubPins.Num() != 3 || VectorIn->SubPins.Num() != 3)
    { AddError(TEXT("Vector pins did not split.")); return false; }
    UEdGraphPin* XOut = VectorOut->SubPins[0]; UEdGraphPin* XIn = VectorIn->SubPins[0];
    XOut->MakeLinkTo(XIn); VectorIn->SubPins[1]->DefaultValue = TEXT("17.0");
    const FGuid XIdentity = XIn->PinId;
    const FString VectorPreview = UTurboMCPPreviewToolset::PreviewTurboMCPVariableRename(VectorOwner->GetPathName(), TEXT("Value"), TEXT("Offset"));
    const FString VectorEdit = UTurboMCPEditToolset::EditTurboMCPVariableRename(VectorOwner->GetPathName(), TEXT("Value"), TEXT("Offset"), Token(VectorPreview));
    AddInfo(VectorEdit);
    TestTrue(TEXT("Split member rename succeeds"), VectorEdit.StartsWith(TEXT("edited:")));
    TestTrue(TEXT("Split identities and links preserved before compile"), XIn->PinId == XIdentity && XIn->LinkedTo.Contains(XOut)
        && XIn->PinName == TEXT("Offset_X") && XOut->PinName == TEXT("Offset_X")
        && VectorIn->SubPins[1]->DefaultValue == TEXT("17.0"));
    FKismetEditorUtilities::CompileBlueprint(VectorOwner);
    TestTrue(TEXT("Split member compiles"), VectorOwner->Status != BS_Error);
    const FString Help = UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCPEditToolset"), TEXT("EditTurboMCPVariableRename"));
    AddInfo(Help);
    TestTrue(TEXT("Help explains confirmation and compilation"), Help.Contains(TEXT("Confirm")) && Help.Contains(TEXT("compile")));
    return !HasAnyErrors();
}
#endif

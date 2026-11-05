// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "BlueprintEditorLibrary.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Interface.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPInterfaceLocalTest,
    "TurboMCP.GraphAppend.InterfaceLocal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPInterfaceLocalTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/InterfaceLocal_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* Interface = FKismetEditorUtilities::CreateBlueprint(UInterface::StaticClass(), Package, TEXT("BPI_Local"), BPTYPE_Interface, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_Local"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Interface fixture"), Interface) || !TestNotNull(TEXT("Implementation fixture"), BP)) return false;
    UEdGraph* Declaration = UBlueprintEditorLibrary::AddFunctionGraph(Interface, TEXT("ReadState"));
    TArray<UK2Node_FunctionEntry*> Entries;
    Declaration->GetNodesOfClass(Entries);
    if (!TestEqual(TEXT("One declaration entry"), Entries.Num(), 1)) return false;
    auto* DeclarationResult = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entries[0]);
    FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Int;
    DeclarationResult->CreateUserDefinedPin(TEXT("State"), Type, EGPD_Input, false);
    FKismetEditorUtilities::CompileBlueprint(Interface);
    const FString Asset = BP->GetPathName();
    const FString Added = UTurboMCPEditToolset::EditTurboMCPInterfaceAdd(Asset, Interface->GetPathName());
    if (!TestTrue(TEXT("Interface added"), Added.StartsWith(TEXT("edited:")))) return false;
    int32 Matches = 0;
    UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(BP, TEXT("ReadState"), Matches);
    if (Graph == nullptr)
    {
        AddError(TEXT("Reader lookup did not find the interface implementation."));
        return false;
    }
    TestEqual(TEXT("Implementation unique"), Matches, 1);
    Package->SetDirtyFlag(false);
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableAdd(Asset, TEXT("ReadState"), TEXT("CachedState"), TEXT("int"), TEXT("7"));
    AddInfo(Preview);
    TestTrue(TEXT("Preview accepts implemented interface function"), Preview.StartsWith(TEXT("preview:")));
    TestFalse(TEXT("Preview preserves clean package"), Package->IsDirty());
    Entries.Reset(); Graph->GetNodesOfClass(Entries);
    if (!TestEqual(TEXT("One implementation entry"), Entries.Num(), 1)) return false;
    TestEqual(TEXT("Preview adds no locals"), Entries[0]->LocalVariables.Num(), 0);
    const FString Edit = UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(Asset, TEXT("ReadState"), TEXT("CachedState"), TEXT("int"), TEXT("7"));
    AddInfo(Edit);
    TestTrue(TEXT("Edit accepts implemented interface function"), Edit.StartsWith(TEXT("edited:")));
    // Structural refresh can replace objects; reacquire the function before inspection.
    Graph = TurboMCPNodeId::FindGraphByName(BP, TEXT("ReadState"), Matches);
    if (!TestNotNull(TEXT("Implementation remains available"), Graph)) return false;
    Entries.Reset(); Graph->GetNodesOfClass(Entries);
    if (!TestEqual(TEXT("One refreshed implementation entry"), Entries.Num(), 1)) return false;
    if (TestEqual(TEXT("One local declared"), Entries[0]->LocalVariables.Num(), 1))
    {
        const FBPVariableDescription& Local = Entries[0]->LocalVariables[0];
        TestEqual(TEXT("Local name"), Local.VarName, FName(TEXT("CachedState")));
        TestEqual(TEXT("Local type"), Local.VarType.PinCategory, UEdGraphSchema_K2::PC_Int);
        TestEqual(TEXT("Local default"), Local.DefaultValue, FString(TEXT("7")));
    }
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Identical declaration is unchanged"), UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(Asset, TEXT("ReadState"), TEXT("CachedState"), TEXT("int"), TEXT("7")).StartsWith(TEXT("unchanged:")));
    TestTrue(TEXT("Conflicting default refuses"), UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(Asset, TEXT("ReadState"), TEXT("CachedState"), TEXT("int"), TEXT("8")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Return parameter conflict refuses"), UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(Asset, TEXT("ReadState"), TEXT("State"), TEXT("int"), TEXT("7")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Event graph refuses"), UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(Asset, TEXT("EventGraph"), TEXT("OtherLocal"), TEXT("int"), TEXT("7")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Missing function refuses"), UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(Asset, TEXT("MissingFunction"), TEXT("OtherLocal"), TEXT("int"), TEXT("7")).StartsWith(TEXT("error:")));
    UEdGraph* Duplicate = FBlueprintEditorUtils::CreateNewGraph(Entries[0], TEXT("ReadState"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    TestEqual(TEXT("Duplicate fixture has the same graph name"), Duplicate->GetName(), Graph->GetName());
    BP->FunctionGraphs.Add(Duplicate);
    TestTrue(TEXT("Ambiguous graph refuses"), UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(Asset, TEXT("ReadState"), TEXT("OtherLocal"), TEXT("int"), TEXT("7")).Contains(TEXT("ambiguous")));
    BP->FunctionGraphs.Remove(Duplicate);
    TestFalse(TEXT("Refusals and no-op preserve clean package"), Package->IsDirty());
    TestEqual(TEXT("Refusals preserve declaration count"), Entries[0]->LocalVariables.Num(), 1);
    return !HasAnyErrors();
}
#endif

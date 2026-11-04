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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPInterfaceTidyTest,
    "TurboMCP.GraphAppend.InterfaceTidy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPInterfaceTidyTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/InterfaceTidy_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* Interface = FKismetEditorUtilities::CreateBlueprint(UInterface::StaticClass(), Package, TEXT("BPI_Tidy"), BPTYPE_Interface, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_Tidy"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
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
    if (!TestNotNull(TEXT("Reader lookup finds implementation"), Graph)) return false;
    TestEqual(TEXT("Implementation unique"), Matches, 1);
    const FString Read = UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset, Graph->GetName());
    TestFalse(TEXT("Reader accepts implementation"), Read.StartsWith(TEXT("error:")));
    Entries.Reset(); Graph->GetNodesOfClass(Entries);
    TArray<UK2Node_FunctionResult*> Results; Graph->GetNodesOfClass(Results);
    if (Entries.Num()!=1 || Results.Num()!=1) { AddError(TEXT("Expected entry and return in implementation.")); return false; }
    auto* Entry = Entries[0]; auto* Result = Results[0];
    Entry->NodePosX=0; Entry->NodePosY=0; Result->NodePosX=-300; Result->NodePosY=200;
    const int32 NodeCount=Graph->Nodes.Num();
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(Asset,TEXT(" ReadState "),true,{});
    TestFalse(TEXT("Tidy preview accepts interface graph"),Preview.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Preview preserves return X"),Result->NodePosX,-300);
    TestEqual(TEXT("Preview preserves return Y"),Result->NodePosY,200);
    const FString Edit=UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset,Graph->GetName(),true,{});
    TestTrue(TEXT("Tidy applies interface layout"),Edit.StartsWith(TEXT("changed")));
    TestEqual(TEXT("Tidy preserves nodes"),Graph->Nodes.Num(),NodeCount);
    const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(Asset,Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));
    TestTrue(TEXT("Native graph image produced"),Capture.Contains(TEXT("renderer: native Slate")) && Capture.Contains(TEXT("image:")));
    AddInfo(Capture);
    const int32 X=Entry->NodePosX, Y=Entry->NodePosY;
    const FString Positions=FString::Printf(TEXT("[{\"node\":\"%s\",\"x\":%d,\"y\":%d},{\"node\":\"%s\",\"x\":%d,\"y\":%d}]"),*TurboMCPNodeId::MakeForNode(Entry),X,Y,*TurboMCPNodeId::MakeForNode(Result),Result->NodePosX+32,Result->NodePosY);
    const FString Positioned=UTurboMCPEditToolset::EditTurboMCPGraphPositions(Asset,Graph->GetName(),Positions);
    TestTrue(TEXT("Mixed position counts"),Positioned.Contains(TEXT("requested=2; moved=1; already_matching=1")));
    TestTrue(TEXT("Repeated position counts"),UTurboMCPEditToolset::EditTurboMCPGraphPositions(Asset,Graph->GetName(),Positions).Contains(TEXT("requested=2; moved=0; already_matching=2")));
    const FString Missing=UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset,TEXT("MissingGraph"),true,{});
    TestTrue(TEXT("Missing graph refuses"),Missing.StartsWith(TEXT("error:")));
    UEdGraph* Duplicate=FBlueprintEditorUtils::CreateNewGraph(Entry,Graph->GetFName(),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    BP->FunctionGraphs.Add(Duplicate);
    TestTrue(TEXT("Duplicate graph refuses"),UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset,Graph->GetName(),true,{}).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Refusal preserves entry X"),Entry->NodePosX,X);
    TestEqual(TEXT("Refusal preserves entry Y"),Entry->NodePosY,Y);
    BP->FunctionGraphs.Remove(Duplicate);
    return !HasAnyErrors();
}
#endif

#include "Tools/TurboMCPToolsets.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "TurboMCPNodeId.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCollapseFunctionTest,"TurboMCP.Workflow.CollapseFunction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPCollapseFunctionTest::RunTest(const FString&)
{
    const FString Suffix=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/Collapse_")+Suffix));
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);
        IFileManager::Get().MakeDirectory(*Folder,true);
        FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("collapse-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false); // Owned synthetic fixture only.
    };
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_CollapseReview"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture"),BP)) return false;
    const FString Asset=BP->GetPathName();
    Transcript+=UTurboMCPEditToolset::EditTurboMCPFunctionAdd(Asset,TEXT("FormatLabel"),TEXT("Value:string"),TEXT("ReturnValue:string"));
    UEdGraph* Graph=nullptr;
    for (UEdGraph* G:BP->FunctionGraphs) if (G->GetName()==TEXT("FormatLabel")) Graph=G;
    if (!TestNotNull(TEXT("Source function"),Graph)) return false;
    UK2Node_FunctionEntry* Entry=nullptr;UK2Node_FunctionResult* Result=nullptr;
    for (UEdGraphNode* Node:Graph->Nodes)
    {
        if (auto* Found=Cast<UK2Node_FunctionEntry>(Node)) Entry=Found;
        if (auto* Found=Cast<UK2Node_FunctionResult>(Node)) Result=Found;
    }
    if (!Entry || !Result) return false;
    Entry->NodePosX=0;Entry->NodePosY=0;Result->NodePosX=1500;Result->NodePosY=0;
    const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();
    UEdGraphPin* Previous=Entry->FindPin(TEXT("Value"));
    TArray<FString> Ids;
    for (int32 Index=0;Index<3;++Index)
    {
        UK2Node_CallFunction* Node=NewObject<UK2Node_CallFunction>(Graph);
        Node->SetFromFunction(UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("Concat_StrStr")));
        Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->AllocateDefaultPins();
        Node->NodePosX=320+Index*320;Node->NodePosY=160;
        Node->FindPin(TEXT("B"))->DefaultValue=FString::FromInt(Index+1);
        if (!TestTrue(TEXT("Chain connection"),Schema->TryCreateConnection(Previous,Node->FindPin(TEXT("A"))))) return false;
        Previous=Node->FindPin(TEXT("ReturnValue"));Ids.Add(TurboMCPNodeId::MakeForNode(Node,true));
    }
    if (!TestTrue(TEXT("External consumer"),Schema->TryCreateConnection(Previous,Result->FindPin(TEXT("ReturnValue"))))) return false;
    auto Evaluate=[&]()
    {
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (!TestTrue(TEXT("Compile"),BP->Status!=BS_Error)) return FString(TEXT("COMPILE_FAILED"));
        UFunction* Function=BP->GeneratedClass->FindFunctionByName(TEXT("FormatLabel"));
        if (!Function) return FString(TEXT("FUNCTION_MISSING"));
        FStructOnScope Parameters(Function);
        FStrProperty* Input=FindFProperty<FStrProperty>(Function,TEXT("Value"));
        FStrProperty* Output=FindFProperty<FStrProperty>(Function,TEXT("ReturnValue"));
        if (!Input || !Output) return FString(TEXT("PARAMETER_MISSING"));
        Input->SetPropertyValue_InContainer(Parameters.GetStructMemory(),TEXT("Order"));
        BP->GeneratedClass->GetDefaultObject()->ProcessEvent(Function,Parameters.GetStructMemory());
        return Output->GetPropertyValue_InContainer(Parameters.GetStructMemory());
    };
    TestEqual(TEXT("Baseline runtime"),Evaluate(),FString(TEXT("Order123")));
    Package->SetDirtyFlag(false);
    const int32 Count=Graph->Nodes.Num();
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),Ids,TEXT("AppendSuffix"),TEXT("function"));
    Transcript+=TEXT("\n")+Preview;
    TestTrue(TEXT("Preview accepted"),Preview.StartsWith(TEXT("would change")));
    TestTrue(TEXT("Signature shown"),Preview.Contains(TEXT("Input_1"))&&Preview.Contains(TEXT("Output_2")));
    TestEqual(TEXT("Preview node count"),Graph->Nodes.Num(),Count);TestFalse(TEXT("Preview clean"),Package->IsDirty());
    const FString Edit=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),Ids,TEXT("AppendSuffix"),TEXT("function"));
    Transcript+=TEXT("\n")+Edit;
    if (!TestTrue(TEXT("Collapse applied"),Edit.StartsWith(TEXT("changed:")))) return false;
    TestEqual(TEXT("One replacement for three nodes"),Graph->Nodes.Num(),Count-2);
    TestEqual(TEXT("Runtime preserved"),Evaluate(),FString(TEXT("Order123")));
    TestEqual(TEXT("Entry position preserved"),Entry->NodePosX,0);TestEqual(TEXT("Return position preserved"),Result->NodePosX,1500);
    for (const FString& Name:{FString(TEXT("FormatLabel")),FString(TEXT("AppendSuffix"))})
        Transcript+=TEXT("\n")+UTurboMCPLookToolset::LookTurboMCPCapture(Asset,Name,TEXT("graph"),TEXT(""),TEXT("1200x700"));
    return true;
}
#endif

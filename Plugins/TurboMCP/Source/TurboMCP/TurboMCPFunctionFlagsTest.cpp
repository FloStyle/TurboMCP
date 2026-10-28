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
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPFunctionFlagsTest,"TurboMCP.Workflow.FunctionFlags",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPFunctionFlagsTest::RunTest(const FString&)
{
    const FString Suffix=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/FunctionFlags_")+Suffix));
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);
        IFileManager::Get().MakeDirectory(*Folder,true);
        FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("function-flags-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false); // Only this owned, never-saved fixture.
    };
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_FunctionFlags"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture"),BP)) return false;
    const FString Asset=BP->GetPathName();
    Transcript+=UTurboMCPEditToolset::EditTurboMCPFunctionAdd(Asset,TEXT("GetLabel"),TEXT(""),TEXT("ReturnValue:string"));
    UEdGraph* Graph=nullptr;
    UK2Node_FunctionEntry* Entry=nullptr;
    for (UEdGraph* Candidate:BP->FunctionGraphs) if (Candidate && Candidate->GetName()==TEXT("GetLabel")) Graph=Candidate;
    if (!TestNotNull(TEXT("Function created"),Graph)) return false;
    for (UEdGraphNode* Node:Graph->Nodes)
    {
        if (auto* Found=Cast<UK2Node_FunctionEntry>(Node)) Entry=Found;
        if (auto* Result=Cast<UK2Node_FunctionResult>(Node))
            if (UEdGraphPin* Pin=Result->FindPin(TEXT("ReturnValue"))) Pin->DefaultValue=TEXT("Purchase ready");
    }
    if (!TestNotNull(TEXT("Entry"),Entry)) return false;
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (!TestTrue(TEXT("Baseline compile"),BP->Status!=BS_Error)) return false;
    auto Run=[&](bool Apply,const FString& Pure,const FString& Const)
    {
        FString Out=Apply?UTurboMCPEditToolset::EditTurboMCPFunctionFlags(Asset,TEXT("GetLabel"),Pure,Const):UTurboMCPPreviewToolset::PreviewTurboMCPFunctionFlags(Asset,TEXT("GetLabel"),Pure,Const);
        Transcript+=TEXT("\n")+Out; return Out;
    };
    Package->SetDirtyFlag(false);
    const int32 BeforeFlags=Entry->GetFunctionFlags();
    TestTrue(TEXT("Preview accepted"),Run(false,TEXT("true"),TEXT("true")).StartsWith(TEXT("preview:")));
    TestEqual(TEXT("Preview flags unchanged"),Entry->GetFunctionFlags(),BeforeFlags);
    TestFalse(TEXT("Preview does not dirty fixture"),Package->IsDirty());
    TestTrue(TEXT("Explicit values required"),Run(true,TEXT("yes"),TEXT("keep")).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Refusal flags unchanged"),Entry->GetFunctionFlags(),BeforeFlags);
    TestTrue(TEXT("Edit accepted"),Run(true,TEXT("true"),TEXT("true")).StartsWith(TEXT("edited:")));
    TestTrue(TEXT("Entry pure"),(Entry->GetFunctionFlags()&FUNC_BlueprintPure)!=0);
    TestTrue(TEXT("Entry const"),(Entry->GetFunctionFlags()&FUNC_Const)!=0);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (!TestTrue(TEXT("Changed function compiles"),BP->Status!=BS_Error)) return false;
    UFunction* Function=BP->GeneratedClass->FindFunctionByName(TEXT("GetLabel"));
    if (!TestNotNull(TEXT("Reflected helper"),Function)) return false;
    TestTrue(TEXT("Reflected flags"),Function->HasAllFunctionFlags(FUNC_BlueprintPure|FUNC_Const));
    FStructOnScope Parameters(Function);
    BP->GeneratedClass->GetDefaultObject()->ProcessEvent(Function,Parameters.GetStructMemory());
    FStrProperty* Return=FindFProperty<FStrProperty>(Function,TEXT("ReturnValue"));
    if (TestNotNull(TEXT("Return property"),Return)) TestEqual(TEXT("Runtime string preserved"),Return->GetPropertyValue_InContainer(Parameters.GetStructMemory()),FString(TEXT("Purchase ready")));
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("No-op"),Run(true,TEXT("keep"),TEXT("keep")).StartsWith(TEXT("unchanged:")));
    TestFalse(TEXT("No-op stays clean"),Package->IsDirty());
    Transcript+=TEXT("\n")+UTurboMCPLookToolset::LookTurboMCPCapture(Asset,TEXT("GetLabel"),TEXT("graph"),TEXT(""),TEXT("1200x700"));
    return true;
}
#endif

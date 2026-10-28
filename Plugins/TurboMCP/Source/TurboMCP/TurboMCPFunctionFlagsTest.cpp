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
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
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

    Transcript+=UTurboMCPEditToolset::EditTurboMCPFunctionAdd(Asset,TEXT("Caller"),TEXT(""),TEXT("ReturnValue:string"));
    UEdGraph* CallerGraph=nullptr;
    for (UEdGraph* Candidate:BP->FunctionGraphs) if (Candidate && Candidate->GetName()==TEXT("Caller")) CallerGraph=Candidate;
    if (!TestNotNull(TEXT("Caller graph"),CallerGraph)) return false;
    UK2Node_FunctionEntry* CallerEntry=nullptr;
    UK2Node_FunctionResult* CallerResult=nullptr;
    for (UEdGraphNode* Node:CallerGraph->Nodes)
    {
        if (auto* Found=Cast<UK2Node_FunctionEntry>(Node)) CallerEntry=Found;
        if (auto* Found=Cast<UK2Node_FunctionResult>(Node)) CallerResult=Found;
    }
    if (!TestNotNull(TEXT("Caller entry"),CallerEntry) || !TestNotNull(TEXT("Caller return"),CallerResult)) return false;
    UK2Node_CallFunction* Call=NewObject<UK2Node_CallFunction>(CallerGraph);
    Call->FunctionReference.SetSelfMember(TEXT("GetLabel"));
    CallerGraph->AddNode(Call,false,false); Call->CreateNewGuid(); Call->AllocateDefaultPins();
    Call->NodePosX=256; Call->NodePosY=160;
    const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();
    if (!TestTrue(TEXT("Connect pure consumer"),Schema->TryCreateConnection(Call->FindPin(TEXT("ReturnValue")),CallerResult->FindPin(TEXT("ReturnValue"))))) return false;
    FKismetEditorUtilities::CompileBlueprint(BP);
    TestTrue(TEXT("Caller compiles"),BP->Status!=BS_Error);
    TestTrue(TEXT("Connected pure-to-impure refused"),Run(true,TEXT("false"),TEXT("keep")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Refused consumer remains connected"),Call->FindPin(TEXT("ReturnValue"))->LinkedTo.Contains(CallerResult->FindPin(TEXT("ReturnValue"))));
    TestTrue(TEXT("Const-only conversion preserves caller"),Run(true,TEXT("keep"),TEXT("false")).StartsWith(TEXT("edited:")));
    TestTrue(TEXT("Consumer survives const change"),Call->FindPin(TEXT("ReturnValue"))->LinkedTo.Contains(CallerResult->FindPin(TEXT("ReturnValue"))));
    TestEqual(TEXT("Caller remains positioned"),FIntPoint(Call->NodePosX,Call->NodePosY),FIntPoint(256,160));
    Call->FindPin(TEXT("ReturnValue"))->BreakAllPinLinks();
    TestTrue(TEXT("Disconnected conversion accepted"),Run(true,TEXT("false"),TEXT("keep")).StartsWith(TEXT("edited:")));
    UEdGraphPin* Execute=Call->FindPin(UEdGraphSchema_K2::PN_Execute);
    UEdGraphPin* Then=Call->FindPin(UEdGraphSchema_K2::PN_Then);
    if (!TestNotNull(TEXT("Impure execute added"),Execute) || !TestNotNull(TEXT("Impure then added"),Then)) return false;
    CallerEntry->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    TestTrue(TEXT("Connect execution input"),Schema->TryCreateConnection(CallerEntry->FindPin(UEdGraphSchema_K2::PN_Then),Execute));
    TestTrue(TEXT("Connect execution output"),Schema->TryCreateConnection(Then,CallerResult->FindPin(UEdGraphSchema_K2::PN_Execute)));
    TestTrue(TEXT("Connected impure-to-pure refused"),Run(true,TEXT("true"),TEXT("keep")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Execution input retained"),Execute->LinkedTo.Contains(CallerEntry->FindPin(UEdGraphSchema_K2::PN_Then)));
    TestTrue(TEXT("Execution output retained"),Then->LinkedTo.Contains(CallerResult->FindPin(UEdGraphSchema_K2::PN_Execute)));
    FKismetEditorUtilities::CompileBlueprint(BP);
    TestTrue(TEXT("Preserved caller compiles"),BP->Status!=BS_Error);
    Transcript+=TEXT("\n")+UTurboMCPLookToolset::LookTurboMCPCapture(Asset,TEXT("GetLabel"),TEXT("graph"),TEXT(""),TEXT("1200x700"));
    return true;
}
#endif

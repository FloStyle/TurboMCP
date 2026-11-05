// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPVariableFlagsEditTest,"TurboMCP.GraphAppend.VariableFlags",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPVariableFlagsEditTest::RunTest(const FString&)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/VariableFlags_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("VariableFlags"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"),BP)) return false;
    FEdGraphPinType Type; Type.PinCategory=UEdGraphSchema_K2::PC_Int;
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("Quantity"),Type,TEXT("7"));
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("Other"),Type,TEXT("9"));
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph=BP->UbergraphPages[0];
    auto* Getter=NewObject<UK2Node_VariableGet>(Graph,NAME_None,RF_Transactional);
    Getter->VariableReference.SetSelfMember(TEXT("Quantity")); Graph->AddNode(Getter,false,false); Getter->CreateNewGuid(); Getter->AllocateDefaultPins();
    auto* Setter=NewObject<UK2Node_VariableSet>(Graph,NAME_None,RF_Transactional);
    Setter->VariableReference.SetSelfMember(TEXT("Other")); Graph->AddNode(Setter,false,false); Setter->CreateNewGuid(); Setter->AllocateDefaultPins();
    UEdGraphPin* Source=Getter->FindPin(TEXT("Quantity"),EGPD_Output);
    UEdGraphPin* Target=Setter->FindPin(TEXT("Other"),EGPD_Input);
    if (!TestNotNull(TEXT("Getter pin"),Source) || !TestNotNull(TEXT("Setter pin"),Target)) return false;
    TestTrue(TEXT("Fixture link connected"),Graph->GetSchema()->TryCreateConnection(Source,Target));
    const FBPVariableDescription Other=BP->NewVariables[1];
    const int32 NodeCount=Graph->Nodes.Num();
    const FBPVariableDescription Before=BP->NewVariables[0];
    const FString Asset=BP->GetPathName();
    auto Edit=[&](const FString& Name,const FString& Editable,const FString& Spawn,const FString& Private)
    {
        const FString Result=UTurboMCPEditToolset::EditTurboMCPVariableFlags(Asset,Name,Editable,Spawn,Private);
        AddInfo(Result); return Result;
    };
    Package->SetDirtyFlag(false);
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPVariableFlags(Asset,TEXT("Quantity"),TEXT("true"),TEXT("true"),TEXT("true"));
    TestTrue(TEXT("Preview describes declaration changes"),Preview.Contains(TEXT("preview: declaration")));
    TestFalse(TEXT("Preview leaves package clean"),Package->IsDirty());
    TestTrue(TEXT("Preview preserves flags"),Before.PropertyFlags==BP->NewVariables[0].PropertyFlags);
    TestTrue(TEXT("Edit changes flags"),Edit(TEXT("Quantity"),TEXT("true"),TEXT("true"),TEXT("true")).StartsWith(TEXT("changed")));
    TestTrue(TEXT("GUID preserved"),Before.VarGuid==BP->NewVariables[0].VarGuid);
    TestTrue(TEXT("Type preserved"),Before.VarType==BP->NewVariables[0].VarType);
    TestEqual(TEXT("Default preserved"),BP->NewVariables[0].DefaultValue,Before.DefaultValue);
    TestTrue(TEXT("Existing graph link preserved"),Source->LinkedTo.Contains(Target) && Target->LinkedTo.Contains(Source));
    TestEqual(TEXT("Graph node count preserved"),Graph->Nodes.Num(),NodeCount);
    TestTrue(TEXT("Other declaration preserved"),Other.VarGuid==BP->NewVariables[1].VarGuid && Other.PropertyFlags==BP->NewVariables[1].PropertyFlags && Other.DefaultValue==BP->NewVariables[1].DefaultValue);
    TestTrue(TEXT("Undo available before compile"),GEditor->UndoTransaction(false));
    TestTrue(TEXT("Undo restores flags"),Before.PropertyFlags==BP->NewVariables[0].PropertyFlags);
    TestEqual(TEXT("Undo restores metadata count"),BP->NewVariables[0].MetaDataArray.Num(),Before.MetaDataArray.Num());
    Edit(TEXT("Quantity"),TEXT("true"),TEXT("true"),TEXT("true"));
    FKismetEditorUtilities::CompileBlueprint(BP);
    FProperty* Property=FindFProperty<FProperty>(BP->GeneratedClass,TEXT("Quantity"));
    if (!TestNotNull(TEXT("Compiled property"),Property)) return false;
    TestFalse(TEXT("Instance editable after compile"),Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
    TestTrue(TEXT("Expose on spawn after compile"),Property->HasAnyPropertyFlags(CPF_ExposeOnSpawn));
    TestEqual(TEXT("Private metadata after compile"),Property->GetMetaData(FBlueprintMetadata::MD_Private),FString(TEXT("true")));
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Repeated request is no-op"),Edit(TEXT("Quantity"),TEXT("true"),TEXT("true"),TEXT("true")).StartsWith(TEXT("unchanged")));
    TestFalse(TEXT("No-op leaves package clean"),Package->IsDirty());
    TestTrue(TEXT("Invalid boolean refused"),Edit(TEXT("Quantity"),TEXT("yes"),TEXT(""),TEXT("")).Contains(TEXT("refused:")));
    TestTrue(TEXT("Missing variable refused"),Edit(TEXT("Missing"),TEXT("true"),TEXT(""),TEXT("")).Contains(TEXT("refused:")));
    TestTrue(TEXT("Native member refused"),Edit(TEXT("bHidden"),TEXT("true"),TEXT(""),TEXT("")).Contains(TEXT("refused:")));
    TestTrue(TEXT("Empty request refused"),Edit(TEXT("Quantity"),TEXT(""),TEXT(""),TEXT("")).Contains(TEXT("refused:")));
    TestFalse(TEXT("Refusals leave package clean"),Package->IsDirty());
    TestTrue(TEXT("Single flag changes"),Edit(TEXT("Quantity"),TEXT("false"),TEXT(""),TEXT("")).StartsWith(TEXT("changed")));
    TestTrue(TEXT("Blank spawn preserved"),(BP->NewVariables[0].PropertyFlags&CPF_ExposeOnSpawn)!=0);
    TestTrue(TEXT("Disable remaining flags"),Edit(TEXT("Quantity"),TEXT(""),TEXT("false"),TEXT("false")).StartsWith(TEXT("changed")));
    FKismetEditorUtilities::CompileBlueprint(BP);
    Property=FindFProperty<FProperty>(BP->GeneratedClass,TEXT("Quantity"));
    if (!TestNotNull(TEXT("Recompiled property"),Property)) return false;
    TestTrue(TEXT("Instance editing disabled"),Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
    TestFalse(TEXT("Spawn disabled"),Property->HasAnyPropertyFlags(CPF_ExposeOnSpawn));
    TestFalse(TEXT("Private removed"),Property->HasMetaData(FBlueprintMetadata::MD_Private));
    for (const TCHAR* Name:{TEXT("PreviewTurboMCPVariableFlags"),TEXT("EditTurboMCPVariableFlags")})
    {
        const FString Help=UTurboMCPReadToolset::ReadTurboMCPToolHelp(FString(Name).StartsWith(TEXT("Preview"))?TEXT("TurboMCP.TurboMCPPreviewToolset"):TEXT("TurboMCP.TurboMCPEditToolset"),Name);
        AddInfo(Help);
        TestTrue(TEXT("Help covers exact booleans and preservation"),Help.Contains(TEXT("true/false")) && Help.Contains(TEXT("empty preserves")));
        TestTrue(TEXT("Help covers owned declarations and compilation"),Help.Contains(TEXT("owned")) && Help.Contains(TEXT("Compile separately")));
    }
    return true;
}
#endif

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "TurboMCPVarsAdd.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPVariableFlagsTest,
    "TurboMCP.Variables.PrivateFlags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPVariableFlagsTest::RunTest(const FString& Parameters)
{
    const FString Leaf = TEXT("BP_VariableFlags_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Temp/TurboMCP/") + Leaf));
    Package->SetFlags(RF_Transient);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(), Package, FName(*Leaf), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("VariableFlagsTest"));
    ON_SCOPE_EXIT
    {
        if (Blueprint)
        {
            if (Blueprint->GeneratedClass) Blueprint->GeneratedClass->MarkAsGarbage();
            if (Blueprint->SkeletonGeneratedClass) Blueprint->SkeletonGeneratedClass->MarkAsGarbage();
            Blueprint->ClearFlags(RF_Standalone);
            Blueprint->MarkAsGarbage();
        }
        Package->SetDirtyFlag(false);
        Package->MarkAsGarbage();
    };
    if (!TestNotNull(TEXT("Disposable Blueprint"), Blueprint)) return false;
    Blueprint->SetFlags(RF_Transient);
    FTurboMCPVarsAdd::FPlan Plan;
    FString Error;
    if (!TestTrue(TEXT("Production plan accepts private and public variables"),
        FTurboMCPVarsAdd::Plan(Blueprint->GetPathName(),
            TEXT("PrivateProbe: int private=true instanceEditable=true, PublicProbe: int private=false instanceEditable=true"),
            Plan, Error))) { AddError(Error); return false; }
    const FString Result = FTurboMCPVarsAdd::Apply(Plan);
    TestTrue(TEXT("Fresh creation reports two variables"), Result.StartsWith(TEXT("changed: added 2 variable(s)")));
    FString PrivateValue;
    TestTrue(TEXT("Private metadata exists on Blueprint description"),
        FBlueprintEditorUtils::GetBlueprintVariableMetaData(Blueprint, TEXT("PrivateProbe"), nullptr,
            FBlueprintMetadata::MD_Private, PrivateValue));
    TestEqual(TEXT("Private metadata value"), PrivateValue, FString(TEXT("true")));
    FString PublicValue;
    TestFalse(TEXT("Public variable has no private metadata"),
        FBlueprintEditorUtils::GetBlueprintVariableMetaData(Blueprint, TEXT("PublicProbe"), nullptr,
            FBlueprintMetadata::MD_Private, PublicValue));
    for (const TCHAR* Name : {TEXT("PrivateProbe"), TEXT("PublicProbe")})
    {
        FProperty* Property = FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, Name);
        if (!TestNotNull(FString::Printf(TEXT("Skeleton property %s"), Name), Property)) continue;
        TestEqual(FString::Printf(TEXT("Private access %s"), Name),
            Property->GetBoolMetaData(FBlueprintMetadata::MD_Private), FString(Name) == TEXT("PrivateProbe"));
        TestFalse(FString::Printf(TEXT("Private option does not force EditConst: %s"), Name),
            Property->HasAnyPropertyFlags(CPF_EditConst));
        TestFalse(FString::Printf(TEXT("Instance editability remains enabled: %s"), Name),
            Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
    }
    return !HasAnyErrors();
}
#endif

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TurboMCPBlueprintRead.h"
#include "TurboMCPBlueprintJson.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPBlueprintReferenceDefaultTest,
    "TurboMCP.Read.BlueprintReferenceDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPBlueprintReferenceDefaultTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Temp/TurboMCP/ReferenceDefault_")
        + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    Package->SetFlags(RF_Transient);
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); Package->MarkAsGarbage(); };
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),
        Package, TEXT("BP_ReferenceDefault"), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Blueprint fixture"), Blueprint)) return false;

    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    Type.PinSubCategoryObject = UObject::StaticClass();
    if (!TestTrue(TEXT("Soft member created"),
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetAsset"), Type))) return false;
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    UClass* Class = Blueprint->GeneratedClass;
    UObject* Defaults = Class ? Class->GetDefaultObject(false) : nullptr;
    FSoftObjectProperty* Property = Class
        ? FindFProperty<FSoftObjectProperty>(Class, TEXT("TargetAsset")) : nullptr;
    if (!TestNotNull(TEXT("Generated default object"), Defaults)
        || !TestNotNull(TEXT("Generated soft property"), Property)) return false;

    const FString CurrentPath = TEXT("/Game/Tests/T_ReferenceTarget.T_ReferenceTarget");
    Property->SetPropertyValue(Property->ContainerPtrToValuePtr<void>(Defaults),
        FSoftObjectPtr(FSoftObjectPath(CurrentPath)));
    Package->SetDirtyFlag(false);
    FTurboMCPBlueprintRead::FRequest Request;
    Request.Detail = FTurboMCPBlueprintRead::EDetail::Signatures;
    const FString Text = FTurboMCPBlueprintRead::Read(Blueprint, Request).Text;
    TestTrue(TEXT("Current soft path appears in text"), Text.Contains(CurrentPath));
    TestTrue(TEXT("Current path source is explicit"), Text.Contains(TEXT("current class default")));
    TestTrue(TEXT("Static scan names the asset boundary"),
        Text.Contains(TEXT("in this Blueprint's graphs")));
    FString Error;
    FTurboMCPBlueprintJson::FRequest JsonRequest;
    JsonRequest.DetailLevel = 1;
    const FString Json = FTurboMCPBlueprintJson::Write(Blueprint, JsonRequest, Error);
    TestTrue(TEXT("JSON read succeeds"), Error.IsEmpty());
    TestTrue(TEXT("JSON carries effective path"), Json.Contains(CurrentPath));
    TestTrue(TEXT("JSON carries default provenance"),
        Json.Contains(TEXT("default_status")) && Json.Contains(TEXT("current class default")));
    TestFalse(TEXT("Reader leaves package clean"), Package->IsDirty());

    Property->SetPropertyValue(Property->ContainerPtrToValuePtr<void>(Defaults), FSoftObjectPtr());
    const auto Unset = FTurboMCPBlueprintRead::DescribeVariableDefault(Blueprint, Blueprint->NewVariables[0]);
    TestEqual(TEXT("Empty generated path is unset"), Unset.Status, FString(TEXT("unset class default")));
    UBlueprint* Uncompiled = NewObject<UBlueprint>(Package, TEXT("BP_Uncompiled"), RF_Transient);
    const auto Unavailable = FTurboMCPBlueprintRead::DescribeVariableDefault(Uncompiled, Blueprint->NewVariables[0]);
    TestEqual(TEXT("No generated class is unavailable"), Unavailable.Status,
        FString(TEXT("class default unavailable")));
    return !HasAnyErrors();
}
#endif

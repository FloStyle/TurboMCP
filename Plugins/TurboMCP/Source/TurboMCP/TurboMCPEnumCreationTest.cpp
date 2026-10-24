// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "TurboMCPEnumAdd.h"
#include "Tools/TurboMCPToolsets.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/UserDefinedEnum.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPEnumCreationTest,
    "TurboMCP.Enums.FreshCreation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPEnumCreationTest::RunTest(const FString& Parameters)
{
    const FString Leaf = TEXT("E_Quality_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Path = TEXT("/Game/Tests/") + Leaf;
    const FString ObjectPath = Path + TEXT(".") + Leaf;
    if (!TestFalse(TEXT("Unique path is not on disk"), FPackageName::DoesPackageExist(Path))) return false;
    UUserDefinedEnum* Created = nullptr;
    ON_SCOPE_EXIT
    {
        if (Created)
        {
            FAssetRegistryModule::AssetDeleted(Created);
            UPackage* Package = Created->GetOutermost();
            Created->ClearFlags(RF_Standalone | RF_Public);
            Created->MarkAsGarbage();
            Package->SetDirtyFlag(false);
            Package->MarkAsGarbage();
        }
    };
    FTurboMCPEnumAdd::FPlan Plan;
    FString Error;
    if (!TestTrue(TEXT("Production plan accepts values"), FTurboMCPEnumAdd::Plan(
        Path, TEXT("Red,Green=Dark Green,Blue"), Plan, Error)))
    { AddError(Error); return false; }
    const FString Preview = FTurboMCPEnumAdd::RenderPreview(Plan);
    TestTrue(TEXT("Preview reports absent asset"), Preview.Contains(TEXT("does not exist yet")));
    TestNull(TEXT("Preview did not create an object"), FindObject<UObject>(nullptr, *ObjectPath));
    const FString Result = FTurboMCPEnumAdd::Apply(Plan);
    Created = FindObject<UUserDefinedEnum>(nullptr, *ObjectPath);
    if (!TestNotNull(TEXT("Fresh enum exists"), Created)) { AddError(Result); return false; }
    TestTrue(TEXT("Creation reports changed"), Result.StartsWith(TEXT("changed:")));
    TestEqual(TEXT("Three values plus sentinel"), Created->NumEnums(), 4);
    const TCHAR* Names[] = {TEXT("Red"), TEXT("Green"), TEXT("Blue")};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        TestEqual(FString::Printf(TEXT("Name %d"), Index), Created->GetNameStringByIndex(Index), FString(Names[Index]));
        TestEqual(FString::Printf(TEXT("Numeric value %d"), Index), Created->GetValueByIndex(Index), static_cast<int64>(Index));
    }
    TestEqual(TEXT("Requested display text"), Created->GetDisplayNameTextByIndex(1).ToString(), FString(TEXT("Dark Green")));
    TestTrue(TEXT("New package is dirty"), Created->GetOutermost()->IsDirty());
    TestFalse(TEXT("Creation did not save"), FPackageName::DoesPackageExist(Path));
    UTurboMCPReadToolset* Reader = GetMutableDefault<UTurboMCPReadToolset>();
    FString Description = Reader->ReadTurboMCPAssetDescribe(ObjectPath);
    TestTrue(TEXT("Dirty unsaved asset has no disk package"), Description.Contains(TEXT("on disk: no")));
    TestTrue(TEXT("Dirty state reported independently"), Description.Contains(TEXT("unsaved changes: yes")));
    Created->GetOutermost()->SetDirtyFlag(false); // Disposable fixture only; deliberately test clean-but-unsaved.
    Description = Reader->ReadTurboMCPAssetDescribe(ObjectPath);
    TestTrue(TEXT("Clean unsaved asset still has no disk package"), Description.Contains(TEXT("on disk: no")));
    TestTrue(TEXT("Clean state reported independently"), Description.Contains(TEXT("unsaved changes: no")));
    Created->GetOutermost()->SetDirtyFlag(true);
    FTurboMCPEnumAdd::FPlan Collision;
    if (TestTrue(TEXT("Collision input parses"), FTurboMCPEnumAdd::Plan(Path, TEXT("Replacement"), Collision, Error)))
    {
        TestTrue(TEXT("Existing enum is refused"), FTurboMCPEnumAdd::Apply(Collision).StartsWith(TEXT("unchanged:")));
        TestEqual(TEXT("Collision preserves count"), Created->NumEnums(), 4);
        TestEqual(TEXT("Collision preserves first name"), Created->GetNameStringByIndex(0), FString(TEXT("Red")));
        TestEqual(TEXT("Collision preserves display"), Created->GetDisplayNameTextByIndex(1).ToString(), FString(TEXT("Dark Green")));
    }
    return !HasAnyErrors();
}
#endif

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TurboMCPDataAssetValues.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDataAssetCreateTest,
    "TurboMCP.DataAsset.Creation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPDataAssetCreateTest::RunTest(const FString& Parameters)
{
    const FString ClassPath = UInputAction::StaticClass()->GetPathName();
    UInputAction::StaticClass()->GetDefaultObject();
    const FString PackagePath = TEXT("/Game/Tests/TurboMCPCreate_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
    auto Preview = [&](const FString& Fields)
    {
        return UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetCreate(ClassPath, PackagePath, Fields, false, false);
    };
    TestTrue(TEXT("preview accepts an initialized native Data Asset class"), Preview(TEXT("{}")).Contains(TEXT("preview: create")));
    TestNull(TEXT("preview allocates no package"), FindPackage(nullptr, *PackagePath));
    TestTrue(TEXT("invalid field refused before creation"), Preview(TEXT("{\"NotAProperty\":\"1\"}")).Contains(TEXT("refused:")));
    TestNull(TEXT("refusal allocates no package"), FindPackage(nullptr, *PackagePath));
    const FString Created = UTurboMCPEditToolset::EditTurboMCPDataAssetCreate(
        ClassPath, PackagePath, TEXT("{\"bConsumeInput\":\"False\"}"), false, false);
    AddInfo(Created);
    UInputAction* Asset = FindObject<UInputAction>(nullptr, *ObjectPath);
    if (!TestNotNull(TEXT("created typed asset"), Asset)) return false;
    TestFalse(TEXT("initial value applied"), Asset->bConsumeInput);
    TestEqual(TEXT("unmentioned default preserved"), Asset->ValueType, GetDefault<UInputAction>()->ValueType);
    TestTrue(TEXT("reader-compatible revision returned"), Created.Contains(TurboMCPDataAssetValues::Revision(Asset)));
    TestTrue(TEXT("created package dirty"), Asset->GetPackage()->IsDirty());
    TestFalse(TEXT("memory creation does not save"), FPackageName::DoesPackageExist(PackagePath));
    TestTrue(TEXT("occupied destination refused"), UTurboMCPEditToolset::EditTurboMCPDataAssetCreate(
        ClassPath, PackagePath, TEXT("{}"), false, false).Contains(TEXT("refused:")));
    GEditor->UndoTransaction();
    TestFalse(TEXT("undo removes live created object"), IsValid(Asset));
    GEditor->RedoTransaction();
    TestTrue(TEXT("redo restores created object"), IsValid(Asset));
    if (IsValid(Asset))
    {
        TestFalse(TEXT("redo restores initialized value"), Asset->bConsumeInput);
        // Only this disposable memory fixture is removed; no saved asset is touched.
        FAssetRegistryModule::AssetDeleted(Asset);
        Asset->GetPackage()->SetDirtyFlag(false);
        Asset->ClearFlags(RF_Public | RF_Standalone);
        Asset->MarkAsGarbage();
    }
    return true;
}
#endif

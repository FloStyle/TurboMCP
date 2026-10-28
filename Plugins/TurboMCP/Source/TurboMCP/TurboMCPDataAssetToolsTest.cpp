// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPDataAssetSafetyFixture.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDataAssetToolsTest,
    "TurboMCP.DataAsset.PropertyWorkflow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPDataAssetToolsTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/DataAssetTools_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> Asset(NewObject<UTurboMCPDataAssetSafetyFixture>(Package, TEXT("Fixture"), RF_Transient));
    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> KeyA(NewObject<UTurboMCPDataAssetSafetyFixture>());
    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> KeyB(NewObject<UTurboMCPDataAssetSafetyFixture>());
    auto* ChildA = NewObject<UTurboMCPDataAssetSafetyChild>(Asset.Get());
    auto* ChildB = NewObject<UTurboMCPDataAssetSafetyChild>(Asset.Get());
    Asset->Entries.Add(KeyA.Get(), ChildA);
    Asset->Child = ChildA;
    Package->SetDirtyFlag(false);
    // The class is transient for safety by default. This owned unsaved fixture must
    // explicitly allow normal package dirty tracking for the public edit check.
    Asset->ClearFlags(RF_Transient);
    const FString Path = Asset->GetPathName();
    auto ReadRevision = [&]()
    {
        const FString Read = UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(Path, TEXT("Entries"));
        AddInfo(Read);
        TArray<FString> Lines; Read.ParseIntoArrayLines(Lines);
        for (const FString& Line : Lines) if (Line.StartsWith(TEXT("revision: "))) return Line.Mid(10);
        AddError(TEXT("Reader did not return a revision.")); return FString();
    };
    const FString OriginalRevision = ReadRevision();
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("map_upsert"), KeyB->GetPathName(), ChildB->GetPathName(), OriginalRevision);
    AddInfo(Preview);
    TestTrue(TEXT("public preview accepts valid map entry"), Preview.StartsWith(TEXT("preview:")));
    TestTrue(TEXT("preview preserves map and clean package"), Asset->Entries.Num() == 1 && Asset->Entries.FindRef(KeyA.Get()) == ChildA && !Package->IsDirty());
    const FString Edit = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("map_upsert"), KeyB->GetPathName(), ChildB->GetPathName(), OriginalRevision);
    AddInfo(Edit);
    TestTrue(TEXT("public edit reports verified result"), Edit.StartsWith(TEXT("edited:")));
    TestTrue(TEXT("entry added without losing original identities"), Asset->Entries.Num() == 2 && Asset->Entries.FindRef(KeyA.Get()) == ChildA && Asset->Entries.FindRef(KeyB.Get()) == ChildB);
    TestTrue(TEXT("successful edit marks package dirty"), Package->IsDirty());
    const FString Stale = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("map_remove"), KeyA->GetPathName(), TEXT(""), OriginalRevision);
    TestTrue(TEXT("stale revision refuses"), Stale.StartsWith(TEXT("refused:")));
    TestEqual(TEXT("stale refusal preserves both entries"), Asset->Entries.Num(), 2);
    const FString Missing = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("map_upsert"), TEXT("/Game/Missing.Missing"), ChildA->GetPathName(), ReadRevision());
    TestTrue(TEXT("invalid map key refuses"), Missing.StartsWith(TEXT("refused:")));
    TestEqual(TEXT("invalid map key preserves both entries"), Asset->Entries.Num(), 2);
    const FString Scalar = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Child.Number"), TEXT("set"), TEXT(""), TEXT("42"), ReadRevision());
    AddInfo(Scalar);
    TestTrue(TEXT("owned child scalar edit succeeds"), Scalar.StartsWith(TEXT("edited:")));
    TestEqual(TEXT("scalar value independently read back"), ChildA->Number, 42);
    Package->SetDirtyFlag(false);
    ChildA->bChangeAfterEdit = true;
    const FString Mismatch = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Child.Number"), TEXT("set"), TEXT(""), TEXT("43"), ReadRevision());
    AddInfo(Mismatch);
    TestTrue(TEXT("notification mismatch does not claim success"), Mismatch.StartsWith(TEXT("failed:")));
    TestEqual(TEXT("notification mismatch restores prior child value"), ChildA->Number, 42);
    TestFalse(TEXT("verified rollback restores clean package flag"), Package->IsDirty());
    TestTrue(TEXT("rollback preserves map identities"), Asset->Entries.Num() == 2 && Asset->Entries.FindRef(KeyA.Get()) == ChildA);
    ChildA->bChangeAfterEdit = false;
    Package->SetDirtyFlag(false);
    Asset->MarkAsGarbage();
    return true;
}
#endif

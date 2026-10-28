// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPDataAssetSafetyFixture.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "InputAction.h"
#include "PlayerMappableKeySettings.h"

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
    Asset->Child = nullptr;
    Package->SetDirtyFlag(false);
    const FString ChildSpec = TEXT("{\"class\":\"/Script/TurboMCP.TurboMCPDataAssetSafetyChild\",\"fields\":{\"Number\":\"17\"}}");
    const FString ChildRevision = ReadRevision();
    const FString ChildPreview = UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetProperty(Path, TEXT("Child"), TEXT("create_child"), TEXT(""), ChildSpec, ChildRevision);
    AddInfo(ChildPreview);
    TestTrue(TEXT("child creation preview accepted"), ChildPreview.StartsWith(TEXT("preview:")));
    TestTrue(TEXT("child preview leaves destination and dirty flag unchanged"), !Asset->Child && !Package->IsDirty());
    const FString Create = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Child"), TEXT("create_child"), TEXT(""), ChildSpec, ChildRevision);
    AddInfo(Create);
    TestTrue(TEXT("child creation edit accepted"), Create.StartsWith(TEXT("edited:")));
    TestTrue(TEXT("new child has correct outer and initial value"), Asset->Child && Asset->Child->GetOuter() == Asset.Get() && Asset->Child->Number == 17);
    const FString Overwrite = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Child"), TEXT("create_child"), TEXT(""), ChildSpec, ReadRevision());
    TestTrue(TEXT("create_child never overwrites existing child"), Overwrite.StartsWith(TEXT("refused:")));

    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> KeyC(NewObject<UTurboMCPDataAssetSafetyFixture>());
    const FString MapCreate = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("create_child"), KeyC->GetPathName(), ChildSpec, ReadRevision());
    AddInfo(MapCreate);
    auto* ChildC = Asset->Entries.FindRef(KeyC.Get());
    TestTrue(TEXT("map child creation accepted"), MapCreate.StartsWith(TEXT("edited:")));
    TestTrue(TEXT("map child has correct owner and preserves other entries"), ChildC && ChildC->GetOuter() == Asset.Get() && ChildC->Number == 17 && Asset->Entries.Num() == 3 && Asset->Entries.FindRef(KeyA.Get()) == ChildA);
    const FString ChildEdit = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("child_set"), KeyC->GetPathName(), TEXT("{\"property\":\"Number\",\"value\":\"23\"}"), ReadRevision());
    AddInfo(ChildEdit);
    TestTrue(TEXT("map child field edit accepted"), ChildEdit.StartsWith(TEXT("edited:")));
    TestTrue(TEXT("map child identity retained during edit"), Asset->Entries.FindRef(KeyC.Get()) == ChildC && ChildC && ChildC->Number == 23);
    const FString InvalidChild = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("child_set"), KeyC->GetPathName(), TEXT("{\"property\":\"Number\",\"value\":\"23 trailing\"}"), ReadRevision());
    TestTrue(TEXT("invalid map child value refuses"), InvalidChild.StartsWith(TEXT("refused:")));
    TestTrue(TEXT("invalid map child value preserves identity and value"), ChildC && ChildC->Number == 23 && Asset->Entries.FindRef(KeyC.Get()) == ChildC);
    const FString Remove = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("map_remove"), KeyC->GetPathName(), TEXT(""), ReadRevision());
    TestTrue(TEXT("exact entry removal accepted"), Remove.StartsWith(TEXT("edited:")));
    TestTrue(TEXT("removal preserves unrelated map entries"), Asset->Entries.Num() == 2 && Asset->Entries.FindRef(KeyA.Get()) == ChildA && Asset->Entries.FindRef(KeyB.Get()) == ChildB);
    const FString RemoveAgain = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("map_remove"), KeyC->GetPathName(), TEXT(""), ReadRevision());
    TestTrue(TEXT("absent entry removal is an explicit no-op"), RemoveAgain.StartsWith(TEXT("unchanged:")));
    if (Asset->Child)
    {
        Asset->Child->Label = TEXT("\u4e00");
        const FString UnicodeRevision = ReadRevision();
        Asset->Child->Label = TEXT("\u4e01");
        TestTrue(TEXT("revision distinguishes non-ASCII values"), ReadRevision() != UnicodeRevision);
    }

    TStrongObjectPtr<UInputAction> Action(NewObject<UInputAction>(Package, TEXT("InputAction")));
    UPlayerMappableKeySettings::StaticClass();
    const FString ActionRead = UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(Action->GetPathName(), TEXT("PlayerMappableKeySettings"));
    FString ActionRevision;
    TArray<FString> ActionLines; ActionRead.ParseIntoArrayLines(ActionLines);
    for (const FString& Line : ActionLines) if (Line.StartsWith(TEXT("revision: "))) ActionRevision = Line.Mid(10);
    const FString SettingsSpec = TEXT("{\"class\":\"/Script/EnhancedInput.PlayerMappableKeySettings\",\"fields\":{\"Name\":\"TurboMCPJump\"}}");
    const FString SettingsCreate = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Action->GetPathName(), TEXT("PlayerMappableKeySettings"), TEXT("create_child"), TEXT(""), SettingsSpec, ActionRevision);
    AddInfo(SettingsCreate);
    TestTrue(TEXT("InputAction key settings creation accepted"), SettingsCreate.StartsWith(TEXT("edited:")));
    const auto* Settings = Action->GetPlayerMappableKeySettings().Get();
    TestTrue(TEXT("real InputAction settings independently verified"), Settings && Settings->GetOuter() == Action.Get() && Settings->GetMappingName() == FName(TEXT("TurboMCPJump")));
    Action->MarkAsGarbage();
    Package->SetDirtyFlag(false);
    Asset->MarkAsGarbage();
    return true;
}
#endif

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPDataAssetSafetyFixture.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Dom/JsonObject.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Serialization/JsonSerializer.h"
#include "PlayerMappableKeySettings.h"

namespace
{
TSharedPtr<FJsonObject> ParsePropertyPage(FAutomationTestBase& Test, const FString& Response)
{
    const int32 Marker = Response.Find(TEXT("\n; tmcp-result-v1: "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    TSharedPtr<FJsonObject> Body;
    Test.TestTrue(TEXT("Paged read returns a JSON body and result envelope"), Marker != INDEX_NONE &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response.Left(Marker)), Body) && Body.IsValid());
    return Body;
}

void CheckPagedProperties(FAutomationTestBase& Test, UPackage* Package, UTurboMCPDataAssetSafetyFixture* Asset, UObject* MapKey)
{
    using FRead = UTurboMCPReadToolset;
    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> Second(NewObject<UTurboMCPDataAssetSafetyFixture>(Package, TEXT("PagedFixture")));
    Second->Child = NewObject<UTurboMCPDataAssetSafetyChild>(Second.Get());
    Second->Child->Label = FString::ChrN(9000, TEXT('x'));
    for (int32 Index = 0; Index < 65; ++Index)
        Second->Numbers.Add(100 + Index);
    const FString Path = Second->GetPathName();
    const FString Missing = Package->GetName() + TEXT(".MissingPagedAsset");
    for (const int32 Offset : {0, 24, 48})
    {
        const TSharedPtr<FJsonObject> Body = ParsePropertyPage(Test,
            FRead::ReadTurboMCPDataAssetProperty(Path, TEXT("Numbers"), TEXT("values"), TEXT("asset"), Offset, 24));
        if (!Body)
            return;
        const int32 Expected = FMath::Min(24, 65 - Offset);
        Test.TestEqual(TEXT("Array total is repeated on each page"), Body->GetIntegerField(TEXT("total")), 65);
        Test.TestEqual(TEXT("Page count matches the requested range"), Body->GetIntegerField(TEXT("returned")), Expected);
        Test.TestEqual(TEXT("Continuation advances by the returned rows"), Body->GetIntegerField(TEXT("nextOffset")), Offset < 48 ? Offset + 24 : -1);
        Test.TestEqual(TEXT("Truncation indicates remaining rows"), Body->GetBoolField(TEXT("truncated")), Offset < 48);
        Test.TestFalse(TEXT("A slice alone does not cover the request"), Body->GetBoolField(TEXT("complete")));
        const auto& Rows = Body->GetArrayField(TEXT("rows"));
        Test.TestEqual(TEXT("Array values are returned as complete rows"), Rows.Num(), Expected);
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            const auto& Row = Rows[Index]->AsObject();
            Test.TestEqual(TEXT("Array index is exact"), Row->GetIntegerField(TEXT("index")), Offset + Index);
            Test.TestEqual(TEXT("Array value is exact"), Row->GetStringField(TEXT("value")), FString::FromInt(100 + Offset + Index));
        }
    }

    const TSharedPtr<FJsonObject> Keys = ParsePropertyPage(Test, FRead::ReadTurboMCPDataAssetProperty(
        Asset->GetPathName() + TEXT("|") + Path + TEXT("|") + Missing, TEXT("Entries|Numbers"), TEXT("keys"), TEXT("asset"), 0, 3));
    if (Keys)
    {
        Test.TestEqual(TEXT("Combined total includes empty and unavailable targets"), Keys->GetIntegerField(TEXT("total")), 69);
        Test.TestEqual(TEXT("A missing asset is counted even beyond the first page"), Keys->GetIntegerField(TEXT("knownErrors")), 1);
        const auto& Rows = Keys->GetArrayField(TEXT("rows"));
        if (Test.TestEqual(TEXT("First key page has three complete rows"), Rows.Num(), 3))
        {
            Test.TestEqual(TEXT("Map key is an exact object path"), Rows[0]->AsObject()->GetStringField(TEXT("key")), MapKey->GetPathName());
            Test.TestFalse(TEXT("Keys-only map row has no value"), Rows[0]->AsObject()->HasField(TEXT("value")));
            Test.TestEqual(TEXT("Empty collection is explicit"), Rows[1]->AsObject()->GetIntegerField(TEXT("collectionTotal")), 0);
            Test.TestEqual(TEXT("The second asset is identified"), Rows[2]->AsObject()->GetStringField(TEXT("asset")), Path);
        }
    }
    const TSharedPtr<FJsonObject> Tail = ParsePropertyPage(Test, FRead::ReadTurboMCPDataAssetProperty(
        Asset->GetPathName() + TEXT("|") + Path + TEXT("|") + Missing, TEXT("Entries|Numbers"), TEXT("keys"), TEXT("asset"), 67, 2));
    if (Tail)
    {
        const auto& Rows = Tail->GetArrayField(TEXT("rows"));
        if (Test.TestEqual(TEXT("The final key page includes the missing target"), Rows.Num(), 2))
        {
            Test.TestEqual(TEXT("Final array key is its index"), Rows[0]->AsObject()->GetIntegerField(TEXT("index")), 64);
            Test.TestFalse(TEXT("Keys-only array row has no value"), Rows[0]->AsObject()->HasField(TEXT("value")));
            Test.TestEqual(TEXT("Missing asset error names the target"), Rows[1]->AsObject()->GetStringField(TEXT("asset")), Missing);
        }
    }
    const TSharedPtr<FJsonObject> Large = ParsePropertyPage(Test, FRead::ReadTurboMCPDataAssetProperty(
        Path, TEXT("Child.Label|Child.Number|MissingProperty"), TEXT("values")));
    if (Large)
    {
        const auto& Rows = Large->GetArrayField(TEXT("rows"));
        if (Test.TestEqual(TEXT("Oversized value does not block later rows"), Rows.Num(), 3))
        {
            Test.TestEqual(TEXT("Oversized row is diagnosed"), Rows[0]->AsObject()->GetStringField(TEXT("code")), FString(TEXT("record_too_large")));
            Test.TestFalse(TEXT("Oversized record contains no clipped value"), Rows[0]->AsObject()->HasField(TEXT("value")));
            Test.TestEqual(TEXT("Following scalar remains exact"), Rows[1]->AsObject()->GetStringField(TEXT("value")), FString(TEXT("7")));
            Test.TestEqual(TEXT("Missing property has a per-property error"), Rows[2]->AsObject()->GetStringField(TEXT("code")), FString(TEXT("property_unavailable")));
        }
    }
    Test.TestTrue(TEXT("Invalid continuation refuses"), FRead::ReadTurboMCPDataAssetProperty(
        Path, TEXT("Numbers"), TEXT("values"), TEXT("asset"), 65).StartsWith(TEXT("refused:")));
    Test.TestTrue(TEXT("Paged property reads preserve fixture values and package state"),
        Second->Numbers.Num() == 65 && Second->Child->Label.Len() == 9000 && !Package->IsDirty());

    Second->Child->Label = FString::ChrN(7000, TEXT('y'));
    const FString Properties = TEXT("Child.Label|Child.Label|Child.Label|Child.Label|Child.Number");
    const TSharedPtr<FJsonObject> Budget = ParsePropertyPage(Test, FRead::ReadTurboMCPDataAssetProperty(
        Path, Properties, TEXT("values")));
    if (Budget)
    {
        const int32 NextOffset = Budget->GetIntegerField(TEXT("nextOffset"));
        if (Test.TestEqual(TEXT("The response budget advances past exactly the emitted rows"), NextOffset, 3))
        {
            const TSharedPtr<FJsonObject> Rest = ParsePropertyPage(Test, FRead::ReadTurboMCPDataAssetProperty(
                Path, Properties, TEXT("values"), TEXT("asset"), NextOffset));
            if (Rest)
            {
                const auto& Rows = Rest->GetArrayField(TEXT("rows"));
                Test.TestEqual(TEXT("The budget continuation preserves the total"), Rest->GetIntegerField(TEXT("total")), 5);
                Test.TestEqual(TEXT("The budget continuation reaches the end"), Rest->GetIntegerField(TEXT("nextOffset")), -1);
                if (Test.TestEqual(TEXT("The continuation returns the two remaining rows"), Rows.Num(), 2))
                {
                    Test.TestEqual(TEXT("Deferred value arrives intact"), Rows[0]->AsObject()->GetStringField(TEXT("value")), Second->Child->Label);
                    Test.TestEqual(TEXT("Following scalar arrives intact"), Rows[1]->AsObject()->GetStringField(TEXT("value")), FString(TEXT("7")));
                }
            }
        }
    }
    // The field workflow selects 45 assets and three properties in one paged request.
    TArray<TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture>> BatchAssets;
    TArray<FString> BatchPaths;
    for (int32 Index = 0; Index < 45; ++Index)
    {
        auto* Item = NewObject<UTurboMCPDataAssetSafetyFixture>(Package, *FString::Printf(TEXT("BatchAsset_%d"), Index));
        Item->Child = NewObject<UTurboMCPDataAssetSafetyChild>(Item);
        Item->Child->Number = Index;
        Item->Child->Label = FString::Printf(TEXT("Item %d"), Index);
        Item->Numbers.Add(Index);
        BatchPaths.Add(Item->GetPathName());
        BatchAssets.Emplace(Item);
    }
    const FString BatchSelector = FString::Join(BatchPaths, TEXT("|"));
    int32 ReadCount = 0;
    for (int32 PageIndex = 0; PageIndex < 10 && ReadCount < 135; ++PageIndex)
    {
        const auto Batch = ParsePropertyPage(Test, FRead::ReadTurboMCPDataAssetProperty(
            BatchSelector, TEXT("Child.Number|Child.Label|Numbers"), TEXT("values"), TEXT("asset"), ReadCount, 100));
        if (!Batch)
            break;
        Test.TestEqual(TEXT("45 assets by three properties has 135 rows"), Batch->GetIntegerField(TEXT("total")), 135);
        Test.TestEqual(TEXT("Every batch target resolves"), Batch->GetIntegerField(TEXT("knownErrors")), 0);
        const auto& Rows = Batch->GetArrayField(TEXT("rows"));
        if (!Test.TestTrue(TEXT("Batch continuation makes progress"), Rows.Num() > 0))
            break;
        for (const auto& Value : Rows)
        {
            const auto Row = Value->AsObject();
            const int32 AssetIndex = ReadCount / 3;
            const int32 PropertyIndex = ReadCount % 3;
            if (AssetIndex >= BatchPaths.Num())
            {
                Test.AddError(TEXT("Batch returned unexpected extra rows"));
                break;
            }
            Test.TestEqual(TEXT("Batch row retains exact asset identity"), Row->GetStringField(TEXT("asset")), BatchPaths[AssetIndex]);
            Test.TestEqual(TEXT("Batch row retains exact value"), Row->GetStringField(TEXT("value")),
                PropertyIndex == 1 ? FString::Printf(TEXT("Item %d"), AssetIndex) : FString::FromInt(AssetIndex));
            ++ReadCount;
        }
        Test.TestEqual(TEXT("Batch continuation matches consumed rows"), Batch->GetIntegerField(TEXT("nextOffset")), ReadCount == 135 ? -1 : ReadCount);
    }
    Test.TestEqual(TEXT("All 45 assets and three properties are retrieved"), ReadCount, 135);
    TArray<FString> TooManyAssets;
    for (int32 Index = 0; Index < 65; ++Index)
        TooManyAssets.Add(Path);
    Test.TestTrue(TEXT("Asset selector limit remains bounded"), FRead::ReadTurboMCPDataAssetProperty(
        FString::Join(TooManyAssets, TEXT("|")), TEXT("Numbers"), TEXT("values")).StartsWith(TEXT("refused:")));
    Test.TestFalse(TEXT("Batch reads leave the package clean"), Package->IsDirty());
    for (const auto& Item : BatchAssets)
        Item->MarkAsGarbage();
    Second->MarkAsGarbage();
}

void CheckBlueprintDefaults(FAutomationTestBase& Test, UPackage* Package)
{
    TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_ReaderDefaults"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()));
    if (!Test.TestNotNull(TEXT("Blueprint defaults fixture"), Blueprint.Get()))
        return;
    ON_SCOPE_EXIT
    {
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false);
    };
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Int;
    if (!Test.TestTrue(TEXT("Blueprint defaults variable created"),
        FBlueprintEditorUtils::AddMemberVariable(Blueprint.Get(), TEXT("Quantity"), Type, TEXT("17"))))
        return;
    FKismetEditorUtilities::CompileBlueprint(Blueprint.Get(), EBlueprintCompileOptions::SkipSave);
    UClass* Class = Blueprint->GeneratedClass;
    UObject* Defaults = Class ? Class->GetDefaultObject(false) : nullptr;
    if (!Test.TestNotNull(TEXT("Fixture has an existing CDO"), Defaults))
        return;
    Package->SetDirtyFlag(false);
    for (const FString& Selector : {Blueprint->GetPathName(), Class->GetPathName()})
    {
        const TSharedPtr<FJsonObject> Body = ParsePropertyPage(Test, UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(
            Selector, TEXT("Quantity"), TEXT("values"), TEXT("blueprint_defaults")));
        if (!Body)
            return;
        const auto& Rows = Body->GetArrayField(TEXT("rows"));
        if (Test.TestEqual(TEXT("One requested CDO field is returned"), Rows.Num(), 1))
        {
            Test.TestEqual(TEXT("Read returns the compiled default"), Rows[0]->AsObject()->GetStringField(TEXT("value")), FString(TEXT("17")));
            Test.TestEqual(TEXT("Read identifies the generated class"), Rows[0]->AsObject()->GetStringField(TEXT("class")), Class->GetPathName());
        }
    }
    const FString Missing = Package->GetName() + TEXT(".MissingBlueprint");
    const FString Unavailable = UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(Missing, TEXT("Quantity"),
        TEXT("values"), TEXT("blueprint_defaults"));
    Test.TestTrue(TEXT("Unavailable Blueprint reports a loaded-only failure"), Unavailable.Contains(TEXT("not fully loaded")));
    Test.TestTrue(TEXT("Defaults read preserves generated class, CDO and clean package"),
        Blueprint->GeneratedClass == Class && Class->GetDefaultObject(false) == Defaults && !Package->IsDirty());
}
}

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
    const FString ReaderDetails = UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(Path, TEXT("Entries"));
    TestTrue(TEXT("map reader gives exact reusable key"), ReaderDetails.Contains(TEXT("entry_key_input: ") + KeyA->GetPathName()));
    TestTrue(TEXT("map reader describes owned child fields"), ReaderDetails.Contains(TEXT("child_field: Number")));
    CheckPagedProperties(*this, Package, Asset.Get(), KeyA.Get());
    const FString Help = UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCP.TurboMCPPreviewToolset"), TEXT("PreviewTurboMCPDataAssetProperty"));
    AddInfo(Help);
    TestTrue(TEXT("live help describes child creation and revision"), Help.Contains(TEXT("create_child")) && Help.Contains(TEXT("Revision")) && Help.Contains(TEXT("property")));
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
    GEditor->UndoTransaction();
    TestTrue(TEXT("undo restores only the removed entry"), Asset->Entries.Num() == 3 && Asset->Entries.FindRef(KeyC.Get()) == ChildC && Asset->Entries.FindRef(KeyA.Get()) == ChildA);
    const FString RemoveAfterUndo = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("map_remove"), KeyC->GetPathName(), TEXT(""), ReadRevision());
    TestTrue(TEXT("entry can be removed again after undo"), RemoveAfterUndo.StartsWith(TEXT("edited:")));
    const FString RemoveAgain = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("map_remove"), KeyC->GetPathName(), TEXT(""), ReadRevision());
    TestTrue(TEXT("absent entry removal is an explicit no-op"), RemoveAgain.StartsWith(TEXT("unchanged:")));
    if (Asset->Child)
    {
        Asset->Child->Label = TEXT("\u4e00");
        const FString UnicodeRevision = ReadRevision();
        Asset->Child->Label = TEXT("\u4e01");
        TestTrue(TEXT("revision distinguishes non-ASCII values"), ReadRevision() != UnicodeRevision);
    }

    const FString BeforeInvalidCreate = ReadRevision();
    const FString InvalidCreate = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("create_child"), KeyC->GetPathName(), TEXT("{\"class\":\"/Script/Engine.DataAsset\"}"), BeforeInvalidCreate);
    TestTrue(TEXT("incompatible child class refuses"), InvalidCreate.StartsWith(TEXT("refused:")));
    TestEqual(TEXT("incompatible class leaves state unchanged"), ReadRevision(), BeforeInvalidCreate);
    const FString InvalidInitial = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("create_child"), KeyC->GetPathName(), TEXT("{\"class\":\"/Script/TurboMCP.TurboMCPDataAssetSafetyChild\",\"fields\":{\"Missing\":\"7\"}}"), BeforeInvalidCreate);
    TestTrue(TEXT("invalid initial field refuses"), InvalidInitial.StartsWith(TEXT("refused:")));
    TestEqual(TEXT("invalid initial field leaves state unchanged"), ReadRevision(), BeforeInvalidCreate);

    UTurboMCPDataAssetAbstractChild::StaticClass();
    const FString AbstractCreate = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("create_child"), KeyC->GetPathName(), TEXT("{\"class\":\"/Script/TurboMCP.TurboMCPDataAssetAbstractChild\"}"), ReadRevision());
    TestTrue(TEXT("compatible abstract child refuses"), AbstractCreate.StartsWith(TEXT("refused:")));
    TestEqual(TEXT("abstract refusal leaves state unchanged"), ReadRevision(), BeforeInvalidCreate);
    const FString MissingCreate = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Entries"), TEXT("create_child"), KeyC->GetPathName(), TEXT("{\"class\":\"/Script/Missing.NoSuchChild\"}"), ReadRevision());
    TestTrue(TEXT("missing child class refuses"), MissingCreate.StartsWith(TEXT("refused:")));
    TestEqual(TEXT("missing class leaves state unchanged"), ReadRevision(), BeforeInvalidCreate);
    if (Asset->Child)
    {
        const int32 PriorNumber = Asset->Child->Number;
        Package->SetDirtyFlag(false);
        Asset->Child->bSpawnAfterEdit = true;
        const FString IncompleteRestore = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path, TEXT("Child.Number"), TEXT("set"), TEXT(""), TEXT("24"), ReadRevision());
        AddInfo(IncompleteRestore);
        const FString Marker = TEXT("; tmcp-result-v1: ");
        const int32 ResultOffset = IncompleteRestore.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        TSharedPtr<FJsonObject> Metadata;
        const bool Parsed = ResultOffset != INDEX_NONE && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(IncompleteRestore.Mid(ResultOffset + Marker.Len())), Metadata) && Metadata.IsValid();
        FString Rollback;
        if (Parsed) Metadata->TryGetStringField(TEXT("rollback"), Rollback);
        const TSharedPtr<FJsonValue> Changed = Parsed ? Metadata->TryGetField(TEXT("changed")) : nullptr;
        TestTrue(TEXT("unrestored callback object reports failure"), IncompleteRestore.StartsWith(TEXT("failed:")) && Rollback == TEXT("failed"));
        TestTrue(TEXT("incomplete restoration reports unknown changed state"), Changed.IsValid() && Changed->Type == EJson::Null);
        TestTrue(TEXT("incomplete restoration does not clear dirty state"), Package->IsDirty());
        TestEqual(TEXT("known prior field restored despite extra callback object"), Asset->Child->Number, PriorNumber);
        Asset->Child->bSpawnAfterEdit = false;
        if (Asset->Child->ExtraChild) Asset->Child->ExtraChild->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
        Asset->Child->ExtraChild = nullptr;
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
    TStrongObjectPtr<UInputMappingContext> Context(NewObject<UInputMappingContext>(Package, TEXT("ArrayShrinkContext"), RF_Transactional));
    for (int32 Index=0;Index<65;++Index) Context->MapKey(Action.Get(),Index%2==0?EKeys::A:EKeys::B);
    const auto OriginalMappings=Context->GetMappings();
    const FString MappingPath=TEXT("DefaultKeyMappings.Mappings");
    auto MappingRevision=[&]()
    {
        const FString Read=UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(Context->GetPathName(),MappingPath);
        TArray<FString> Lines;Read.ParseIntoArrayLines(Lines);
        for (const FString& Line:Lines) if (Line.StartsWith(TEXT("revision: "))) return Line.Mid(10);
        return FString();
    };
    const FString MappingStart=MappingRevision();
    TestFalse(TEXT("Mapping reader returns revision"),MappingStart.IsEmpty());
    Package->SetDirtyFlag(false);
    const FString ArrayPreview=UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetProperty(Context->GetPathName(),MappingPath,TEXT("array_remove"),TEXT("64"),TEXT(""),MappingStart);
    TestTrue(TEXT("Array preview reports 65 to 64"),ArrayPreview.StartsWith(TEXT("preview:")) && ArrayPreview.Contains(TEXT("array_count: 65 -> 64")));
    TestEqual(TEXT("Preview preserves mapping count"),Context->GetMappings().Num(),65);
    TestFalse(TEXT("Preview preserves clean package"),Package->IsDirty());
    for (const FString& Index:TArray<FString>{TEXT("65"),TEXT("-1"),TEXT("1.5"),TEXT(" 1"),TEXT("9999999999")})
    {
        const FString Refusal=UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Context->GetPathName(),MappingPath,TEXT("array_remove"),Index,TEXT(""),MappingStart);
        TestTrue(TEXT("Invalid index refuses"),Refusal.StartsWith(TEXT("refused:")));
        TestEqual(TEXT("Invalid index preserves revision"),MappingRevision(),MappingStart);
        TestFalse(TEXT("Invalid index preserves clean package"),Package->IsDirty());
    }
    const FString ArrayEdit=UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Context->GetPathName(),MappingPath,TEXT("array_remove"),TEXT("64"),TEXT(""),MappingStart);
    AddInfo(ArrayEdit);
    TestTrue(TEXT("Array removal reports verified count"),ArrayEdit.StartsWith(TEXT("edited:")) && ArrayEdit.Contains(TEXT("array_count: 65 -> 64")));
    TestEqual(TEXT("Last mapping removed"),Context->GetMappings().Num(),64);
    for (int32 Index=0;Index<Context->GetMappings().Num() && Index<64;++Index)
        TestTrue(TEXT("Retained mapping preserved"),Context->GetMappings()[Index]==OriginalMappings[Index]);
    TestTrue(TEXT("Stale revision refuses second removal"),UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Context->GetPathName(),MappingPath,TEXT("array_remove"),TEXT("0"),TEXT(""),MappingStart).StartsWith(TEXT("refused:")));
    TestEqual(TEXT("Stale request preserves count"),Context->GetMappings().Num(),64);
    GEditor->UndoTransaction();
    TestEqual(TEXT("Undo restores all mappings"),Context->GetMappings().Num(),65);
    TestEqual(TEXT("Undo restores complete asset revision"),MappingRevision(),MappingStart);
    Package->SetDirtyFlag(false);
    const FString InsertPreview=UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetProperty(Context->GetPathName(),MappingPath,TEXT("array_insert"),TEXT("32"),TEXT(""),MappingStart);
    TestTrue(TEXT("Mapping insertion preview reports count"),InsertPreview.StartsWith(TEXT("preview:")) && InsertPreview.Contains(TEXT("array_count: 65 -> 66")));
    TestTrue(TEXT("Mapping insertion preview preserves state"),MappingRevision()==MappingStart && !Package->IsDirty());
    const FString InsertMapping=UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Context->GetPathName(),MappingPath,TEXT("array_insert"),TEXT("32"),TEXT(""),MappingStart);
    AddInfo(InsertMapping);
    TestTrue(TEXT("Default mapping inserted"),InsertMapping.StartsWith(TEXT("edited:")) && Context->GetMappings().Num()==66);
    if (Context->GetMappings().Num()==66)
    {
        TestTrue(TEXT("New mapping is default initialized"),Context->GetMappings()[32]==FEnhancedActionKeyMapping());
        for (int32 Index=0;Index<65;++Index)
            TestTrue(TEXT("Mapping insertion preserves every old row"),Context->GetMappings()[Index<32?Index:Index+1]==OriginalMappings[Index]);
    }
    GEditor->UndoTransaction();
    TestEqual(TEXT("Mapping insertion undo restores revision"),MappingRevision(),MappingStart);
    Asset->Numbers={10,20};
    auto NumberRevision=[&]()
    {
        TArray<FString> Lines;
        UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(Path,TEXT("Numbers")).ParseIntoArrayLines(Lines);
        for (const FString& Line:Lines) if (Line.StartsWith(TEXT("revision: "))) return Line.Mid(10);
        return FString();
    };
    for (int32 At : {0,1,2})
    {
        Package->SetDirtyFlag(false);
        const FString Before=NumberRevision();
        const FString Index=FString::FromInt(At);
        const FString PreviewInsert=UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetProperty(Path,TEXT("Numbers"),TEXT("array_insert"),Index,TEXT("42"),Before);
        TestTrue(TEXT("Scalar insertion preview preserves state"),PreviewInsert.StartsWith(TEXT("preview:")) && NumberRevision()==Before && !Package->IsDirty());
        const FString Added=UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path,TEXT("Numbers"),TEXT("array_insert"),Index,TEXT("42"),Before);
        TestTrue(TEXT("Scalar insertion succeeds"),Added.StartsWith(TEXT("edited:")));
        TArray<int32> Expected={10,20}; Expected.Insert(42,At);
        TestTrue(TEXT("Exact inserted order"),Asset->Numbers==Expected);
        TestTrue(TEXT("Stale insertion refuses"),UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path,TEXT("Numbers"),TEXT("array_insert"),Index,TEXT("42"),Before).StartsWith(TEXT("refused:")));
        TestTrue(TEXT("Stale insertion preserves values"),Asset->Numbers==Expected);
        GEditor->UndoTransaction();
        TestEqual(TEXT("Insertion undo restores revision"),NumberRevision(),Before);
    }
    Package->SetDirtyFlag(false);
    const FString BeforeInvalid=NumberRevision();
    for (const FString& Index:TArray<FString>{TEXT("3"),TEXT("-1"),TEXT("1.5"),TEXT(" 1"),TEXT("9999999999")})
        TestTrue(TEXT("Invalid insertion index refuses"),UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(Path,TEXT("Numbers"),TEXT("array_insert"),Index,TEXT("42"),BeforeInvalid).StartsWith(TEXT("refused:")));
    const FString InvalidValue = UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(
        Path, TEXT("Numbers"), TEXT("array_insert"), TEXT("1"), TEXT("not_an_integer"), BeforeInvalid);
    TestTrue(TEXT("Invalid inserted value refuses"), InvalidValue.StartsWith(TEXT("refused:")));
    TestTrue(TEXT("Insertion refusals preserve revision and clean state"),NumberRevision()==BeforeInvalid && !Package->IsDirty());
    const FString HexPreview = UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetProperty(
        Path, TEXT("Numbers"), TEXT("array_insert"), TEXT("1"), TEXT("0x2A"), BeforeInvalid);
    TestTrue(TEXT("Hexadecimal integer remains valid in preview"), HexPreview.StartsWith(TEXT("preview:")));
    TestTrue(TEXT("Hexadecimal preview preserves revision and clean state"), NumberRevision() == BeforeInvalid && !Package->IsDirty());
    Context->MarkAsGarbage();
    Action->MarkAsGarbage();
    Package->SetDirtyFlag(false);
    Asset->MarkAsGarbage();
    CheckBlueprintDefaults(*this, Package);
    return true;
}
#endif

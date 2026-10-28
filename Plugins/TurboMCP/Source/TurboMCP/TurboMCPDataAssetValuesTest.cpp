// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "TurboMCPDataAssetValues.h"
#include "TurboMCPDataAssetSafetyFixture.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDataAssetValuesTest,
    "TurboMCP.DataAsset.ValuePlanning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPDataAssetValuesTest::RunTest(const FString& Parameters)
{
    using namespace TurboMCPDataAssetValues;
    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> Asset(NewObject<UTurboMCPDataAssetSafetyFixture>());
    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> FirstKey(NewObject<UTurboMCPDataAssetSafetyFixture>());
    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> SecondKey(NewObject<UTurboMCPDataAssetSafetyFixture>());
    auto* FirstChild = NewObject<UTurboMCPDataAssetSafetyChild>(Asset.Get());
    auto* SecondChild = NewObject<UTurboMCPDataAssetSafetyChild>(Asset.Get());
    Asset->Entries.Add(FirstKey.Get(), FirstChild);
    auto* MapProperty = FindFProperty<FMapProperty>(Asset->GetClass(), TEXT("Entries"));
    FValue Key(MapProperty->KeyProp), Value(MapProperty->ValueProp), Map(MapProperty);
    TStrongObjectPtr<UObject> HeldKey, HeldValue;
    FString Error;
    TestTrue(TEXT("external key is not treated as an instanced child"), ResolveReference(
        MapProperty->KeyProp, SecondKey->GetPathName(), Asset.Get(), true, Key.Get(), HeldKey, Error));
    TestTrue(TEXT("owned child reference resolves"), ResolveReference(
        MapProperty->ValueProp, SecondChild->GetPathName(), Asset.Get(), false, Value.Get(), HeldValue, Error));
    PlanMapEntry(MapProperty, &Asset->Entries, Key.Get(), Value.Get(), false, Map.Get());
    FScriptMapHelper Planned(MapProperty, Map.Get());
    TestEqual(TEXT("planned map contains both entries"), Planned.Num(), 2);
    TestEqual(TEXT("planning leaves live map count unchanged"), Asset->Entries.Num(), 1);
    TestTrue(TEXT("planning preserves live key and child identity"), Asset->Entries.FindRef(FirstKey.Get()) == FirstChild);
    PlanMapEntry(MapProperty, &Asset->Entries, Key.Get(), nullptr, true, Map.Get());
    TestEqual(TEXT("removing an absent key is a no-op"), Planned.Num(), 1);
    TestFalse(TEXT("missing object path refuses"), ResolveReference(MapProperty->KeyProp,
        TEXT("/Game/DoesNotExist.Object"), Asset.Get(), true, Key.Get(), HeldKey, Error));
    auto* ForeignChild = NewObject<UTurboMCPDataAssetSafetyChild>(FirstKey.Get());
    TestFalse(TEXT("foreign instanced value refuses"), ResolveReference(MapProperty->ValueProp,
        ForeignChild->GetPathName(), Asset.Get(), false, Value.Get(), HeldValue, Error));
    TestTrue(TEXT("refusals preserve live map"), Asset->Entries.Num() == 1 && Asset->Entries.FindRef(FirstKey.Get()) == FirstChild);
    auto* Number = FindFProperty<FProperty>(FirstChild->GetClass(), TEXT("Number"));
    FValue Numeric(Number, Number->ContainerPtrToValuePtr<void>(FirstChild));
    TestTrue(TEXT("valid scalar parses into temporary storage"), ParseValue(Number, TEXT("42"), Numeric.Get(), Error));
    TestEqual(TEXT("scalar parsing leaves live value unchanged"), FirstChild->Number, 7);
    TestFalse(TEXT("trailing scalar text refuses"), ParseValue(Number, TEXT("42 trailing"), Numeric.Get(), Error));
    TestEqual(TEXT("invalid parsing leaves live value unchanged"), FirstChild->Number, 7);
    return true;
}
#endif

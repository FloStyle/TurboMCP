// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.


#include "TurboMCPDataAssetSafetyFixture.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ToolsetRegistry/ToolsetLibrary.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDataAssetSafetyTest,
    "TurboMCP.DataAsset.StockWriterSafety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPDataAssetSafetyTest::RunTest(const FString& Parameters)
{
    // The fixture owns its package and objects. No save, unload or workplace asset is involved.
    const FString PackageName = TEXT("/Game/Tests/TurboMCPDataSafety_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    TStrongObjectPtr<UTurboMCPDataAssetSafetyFixture> Asset(NewObject<UTurboMCPDataAssetSafetyFixture>(Package, TEXT("Fixture"), RF_Transient));
    TStrongObjectPtr<UObject> Key(NewObject<UObject>(GetTransientPackage(), NAME_None, RF_Transient));
    auto* Value = NewObject<UTurboMCPDataAssetSafetyChild>(Asset.Get());
    Asset->Entries.Add(Key.Get(), Value);
    Package->SetDirtyFlag(false);
    const FString Before = UToolsetLibrary::GetObjectProperties(Asset.Get(), {TEXT("Entries")});
    // Reimport the reader's own representation: a no-op must retain key/value identities.
    const bool bMapAccepted = UToolsetLibrary::SetObjectProperties(Asset.Get(), Before, EBypassContainerCheck::Yes);
    const FString After = UToolsetLibrary::GetObjectProperties(Asset.Get(), {TEXT("Entries")});
    const bool bMapPreserved = Asset->Entries.Num() == 1 && Asset->Entries.FindRef(Key.Get()) == Value;
    FString Evidence = FString::Printf(TEXT("map accepted=%d preserved=%d dirty=%d\nbefore=%s\nafter=%s\n"),
        bMapAccepted, bMapPreserved, Package->IsDirty(), *Before, *After);
    TestTrue(TEXT("reader representation preserves the original object-keyed map"), bMapPreserved);

    const FString Create = FString::Printf(TEXT("{\"Child\":\"%s\"}"), *UTurboMCPDataAssetSafetyChild::StaticClass()->GetPathName());
    const bool bChildAccepted = UToolsetLibrary::SetObjectProperties(Asset.Get(), Create, EBypassContainerCheck::Yes);
    const bool bChildPresent = Asset->Child && Asset->Child->GetOuter() == Asset.Get();
    Evidence += FString::Printf(TEXT("child accepted=%d exists_with_correct_outer=%d\nreadback=%s\n"),
        bChildAccepted, bChildPresent, *UToolsetLibrary::GetObjectProperties(Asset.Get(), {TEXT("Child")}));
    TestTrue(TEXT("successful instanced write creates an owned object"), !bChildAccepted || bChildPresent);
    TestTrue(TEXT("supported class-path instantiation succeeds"), bChildAccepted && bChildPresent);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("TurboMCP/Workflow");
    IFileManager::Get().MakeDirectory(*Directory, true);
    TestTrue(TEXT("write reproduction evidence"), FFileHelper::SaveStringToFile(Evidence, *(Directory / TEXT("data-asset-stock-writer.txt"))));
    AddInfo(Evidence);
    Package->SetDirtyFlag(false); // Owned unsaved fixture only; never hides a production mutation.
    Asset->MarkAsGarbage();
    return true;
}
#endif

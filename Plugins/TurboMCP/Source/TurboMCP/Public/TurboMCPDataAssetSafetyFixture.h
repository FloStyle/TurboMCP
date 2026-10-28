// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.


#pragma once
#include "Engine/DataAsset.h"
#include "TurboMCPDataAssetSafetyFixture.generated.h"

/** Disposable native fixture; never used by production tools or saved as project content. */
UCLASS(EditInlineNew, DefaultToInstanced, Transient)
class UTurboMCPDataAssetSafetyChild : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Test")
    int32 Number = 7;
#if WITH_EDITOR
    // Deliberate synchronous notification mismatch for owned rollback tests only.
    bool bChangeAfterEdit = false;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override
    {
        Super::PostEditChangeProperty(Event);
        if (bChangeAfterEdit) Number = 99;
    }
#endif
};

/** Exercises external map keys alongside owned instanced values. */
UCLASS(Transient)
class UTurboMCPDataAssetSafetyFixture : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Instanced, Category="Test")
    TMap<UObject*, UTurboMCPDataAssetSafetyChild*> Entries;
    UPROPERTY(EditAnywhere, Instanced, Category="Test")
    TObjectPtr<UTurboMCPDataAssetSafetyChild> Child;
};

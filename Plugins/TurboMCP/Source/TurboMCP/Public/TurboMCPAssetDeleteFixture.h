// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "Engine/DataAsset.h"
#include "TurboMCPAssetDeleteFixture.generated.h"
/** Owned saved fixture for hard and soft registry dependency checks. */
UCLASS()
class UTurboMCPAssetDeleteFixture : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY()
    TObjectPtr<UObject> HardReference;
    UPROPERTY()
    TSoftObjectPtr<UObject> SoftReference;
};

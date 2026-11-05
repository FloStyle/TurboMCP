// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "Subsystems/LocalPlayerSubsystem.h"
#include "TurboMCPPlayInvokeFixture.generated.h"

/** Created explicitly by the owned PIE test, never installed in user sessions. */
UCLASS(Transient)
class UTurboMCPPlayInvokeFixture : public ULocalPlayerSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return false; }
    int32 Calls=0;
    int32 Total=0;
    UFUNCTION(BlueprintCallable, Category="TurboMCP Test")
    int32 RecordEvent(int32 Amount,const FString& Label)
    {
        ++Calls;Total+=Amount;return Total;
    }
};

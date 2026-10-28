// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "CoreMinimal.h"
class UTurboMCPSettings;
/** Immutable session policy shared by discovery, dispatch and diagnostics. */
namespace TurboMCPToolPolicy
{
    void Initialize(const UTurboMCPSettings& Settings, const TArray<FString>& ToolNames);
    bool IsAllowed(const FString& ToolName, FString& Reason);
    FString Domain(const FString& ToolName);
    FString Describe(const UTurboMCPSettings& CurrentSettings);
    TArray<FString> DisabledNames();
}

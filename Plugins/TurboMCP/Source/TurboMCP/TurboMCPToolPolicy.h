// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "CoreMinimal.h"
class UTurboMCPSettings;
/** Session preferences and recovery safety checks shared by dispatch and diagnostics. */
namespace TurboMCPToolPolicy
{
    void Initialize(const UTurboMCPSettings& Settings, const TArray<FString>& ToolNames);
    bool IsAllowed(const FString& ToolName, FString& Reason);
    /** Fail closed after unverified restoration. Only a new editor session clears this in production. */
    void RequireRecovery(const FString& Asset);
    bool IsRecoveryRequired(FString& Reason);
#if WITH_DEV_AUTOMATION_TESTS
    /** Isolated automation cleanup only; not reflected or callable through MCP. */
    void ResetRecoveryForTest();
#endif
    FString Domain(const FString& ToolName);
    FString Describe(const UTurboMCPSettings& CurrentSettings);
    TArray<FString> DisabledNames();
}

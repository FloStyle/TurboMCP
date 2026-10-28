// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPToolPolicy.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TurboMCPSettings.h"
#include "TurboMCPToolsetsInternal.h"
#include "Tools/TurboMCPToolsets.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/FieldIterator.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPToolPolicyTest,"TurboMCP.Workflow.ToolControls",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPToolPolicyTest::RunTest(const FString&)
{
    TArray<FString> Names;
    for (const auto& Class:TurboMCPToolsets::GetToolsetClasses())
        for (TFieldIterator<UFunction> Function(Class.Get());Function;++Function)
            if (Function->HasMetaData(TEXT("AICallable"))) Names.AddUnique(Function->GetName());
    ON_SCOPE_EXIT {TurboMCPToolPolicy::Initialize(*UTurboMCPSettings::Get(),Names);};
    UTurboMCPSettings* TestSettings=NewObject<UTurboMCPSettings>();
    // This object is never saved: personal preferences are not test scratch space.
    TestSettings->bEnableTurboMCP=true;TestSettings->DisabledToolDomains={TEXT("Blueprint")};TestSettings->ToolOverrides.Reset();
    TurboMCPToolPolicy::Initialize(*TestSettings,Names);
    FString Reason;
    TestFalse(TEXT("Domain blocks graph reader"),TurboMCPToolPolicy::IsAllowed(TEXT("ReadTurboMCPGraphLoad"),Reason));
    TestTrue(TEXT("Recovery status remains available"),TurboMCPToolPolicy::IsAllowed(TEXT("ReadTurboMCPStatus"),Reason));
    TestSettings->ToolOverrides.Add(TEXT("ReadTurboMCPGraphLoad"),true);
    TestFalse(TEXT("Preference change is not live"),TurboMCPToolPolicy::IsAllowed(TEXT("ReadTurboMCPGraphLoad"),Reason));
    TestTrue(TEXT("Pending restart diagnosed"),TurboMCPToolPolicy::Describe(*TestSettings).Contains(TEXT("restart: yes")));
    TurboMCPToolPolicy::Initialize(*TestSettings,Names);
    TestTrue(TEXT("Explicit enable overrides domain"),TurboMCPToolPolicy::IsAllowed(TEXT("ReadTurboMCPGraphLoad"),Reason));
    TestSettings->DisabledToolDomains.Reset();
    TestSettings->ToolOverrides.Add(TEXT("PreviewTurboMCPGraphCollapse"),false);
    TestSettings->ToolOverrides.Add(TEXT("EditTurboMCPGraphCollapse"),true);
    TestSettings->ToolOverrides.Add(TEXT("ReadTurboMCPStatus"),false);
    TurboMCPToolPolicy::Initialize(*TestSettings,Names);
    TestFalse(TEXT("Disabled preview blocks explicitly enabled edit"),TurboMCPToolPolicy::IsAllowed(TEXT("EditTurboMCPGraphCollapse"),Reason));
    TestTrue(TEXT("Dependency reason is clear"),Reason.Contains(TEXT("matching preview")));
    TestTrue(TEXT("Status override cannot lock out recovery"),TurboMCPToolPolicy::IsAllowed(TEXT("ReadTurboMCPStatus"),Reason));
    TestSettings->ToolOverrides.Add(TEXT("NoSuchTurboTool"),true);
    TestSettings->DisabledToolDomains.Add(TEXT("UnknownDomain"));
    TestTrue(TEXT("Unknown override diagnosed"),TurboMCPToolPolicy::Describe(*TestSettings).Contains(TEXT("Unknown tool override")));
    TestTrue(TEXT("Unknown domain diagnosed"),TurboMCPToolPolicy::Describe(*TestSettings).Contains(TEXT("Unknown tool domain")));
    TestSettings->bEnableTurboMCP=false;
    TurboMCPToolPolicy::Initialize(*TestSettings,Names);
    TestFalse(TEXT("Master off wins over recovery"),TurboMCPToolPolicy::IsAllowed(TEXT("ReadTurboMCPStatus"),Reason));
    return true;
}
#endif

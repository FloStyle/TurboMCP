#include "Tools/TurboMCPToolsets.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
DEFINE_LOG_CATEGORY_STATIC(LogTurboMCPConsoleFixture, Log, All);
namespace
{
FString ConsoleToken(const FString& Text)
{
    FString Before, After;
    return Text.Split(TEXT("Confirmation: "), &Before, &After) ? After.TrimStartAndEnd() : FString();
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDiagnosticConsoleTest, "TurboMCP.Diagnostics.Console", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPDiagnosticConsoleTest::RunTest(const FString&)
{
    auto Preview = [](const FString& Command) { return UTurboMCPPreviewToolset::PreviewTurboMCPDiagnosticConsole(TEXT("editor"), Command); };
    auto Edit = [](const FString& Command, const FString& Token = FString()) { return UTurboMCPEditToolset::EditTurboMCPDiagnosticConsole(TEXT("editor"), Command, Token); };
    IConsoleManager& Manager = IConsoleManager::Get();
    const TCHAR* Name = TEXT("TurboMCP.ConsoleFixture");
    if (Manager.FindConsoleObject(Name)) { AddError(TEXT("Fixture CVar already exists; refusing to replace it.")); return false; }
    IConsoleVariable* Variable = Manager.RegisterConsoleVariable(Name, 7, TEXT("Owned diagnostic console fixture."), ECVF_Default);
    struct FCleanup
    {
        ~FCleanup() { IConsoleManager::Get().UnregisterConsoleObject(TEXT("TurboMCP.ConsoleFixture"), false); }
    } Cleanup;
    TestTrue(TEXT("Query identifies actual value"), Edit(Name).Contains(TEXT("Actual value: 7")));
    const FString Command = FString(Name) + TEXT(" 12");
    const FString First = ConsoleToken(Preview(Command));
    TestFalse(TEXT("Preview has token"), First.IsEmpty());
    TestEqual(TEXT("Preview preserves value"), Variable->GetInt(), 7);
    TestTrue(TEXT("Missing confirmation refuses"), Edit(Command).StartsWith(TEXT("REFUSED:")));
    Variable->Set(8, ECVF_SetByConsole);
    TestTrue(TEXT("Changed value invalidates confirmation"), Edit(Command, First).StartsWith(TEXT("REFUSED:")));
    const FString Token = ConsoleToken(Preview(Command));
    TestTrue(TEXT("Confirmed write reports actual value"), Edit(Command, Token).Contains(TEXT("Actual value: 12")));
    TestEqual(TEXT("Actual variable changed"), Variable->GetInt(), 12);
    TestTrue(TEXT("Token cannot be replayed"), Edit(Command, Token).StartsWith(TEXT("REFUSED:")));
    Variable->SetFlags(ECVF_ReadOnly);
    TestTrue(TEXT("Read-only assignment refused"), Preview(Command).StartsWith(TEXT("REFUSED:")));
    Variable->ClearFlags(ECVF_ReadOnly);
    for (const FString& Invalid : {FString(TEXT("quit")), FString(TEXT("open map")), FString(TEXT("Log global verbose")), FString(TEXT("Log LogTurboMCPConsoleFixture verbose;quit")), FString(TEXT("Log list\nquit")), FString(TEXT("not.a.registered.cvar"))})
        TestTrue(TEXT("Unsupported or chained command refused: ") + Invalid, Preview(Invalid).StartsWith(TEXT("REFUSED:")));
    TestTrue(TEXT("Missing target refused"), UTurboMCPPreviewToolset::PreviewTurboMCPDiagnosticConsole(TEXT(""), TEXT("Log list")).StartsWith(TEXT("REFUSED:")));
    TestTrue(TEXT("Absent PIE target refused"), UTurboMCPPreviewToolset::PreviewTurboMCPDiagnosticConsole(TEXT("pie:99999"), TEXT("Log list")).StartsWith(TEXT("REFUSED:")));
    const ELogVerbosity::Type OldLevel = LogTurboMCPConsoleFixture.GetVerbosity();
    const FString LogCommand = TEXT("Log LogTurboMCPConsoleFixture verbose");
    const FString LogToken = ConsoleToken(Preview(LogCommand));
    const FString LogResult = Edit(LogCommand, LogToken);
    TestTrue(TEXT("Category level read back"), LogResult.Contains(TEXT("Actual category verbosity: Verbose")));
    UE_LOG(LogTurboMCPConsoleFixture, Verbose, TEXT("console-verbosity-live-marker"));
    const FString History = UTurboMCPReadToolset::ReadTurboMCPEditorLog(TEXT("LogTurboMCPConsoleFixture"), TEXT("console-verbosity-live-marker"), TEXT(""), TEXT(""), 10);
    TestTrue(TEXT("Verbosity effect visible in output history"), History.Contains(TEXT("console-verbosity-live-marker")));
    LogTurboMCPConsoleFixture.SetVerbosity(OldLevel);
    if (Manager.FindConsoleObject(TEXT("SlateDebugger.Event.Start")))
    {
        const FString Start = TEXT("SlateDebugger.Event.Start"), Stop = TEXT("SlateDebugger.Event.Stop");
        TestTrue(TEXT("Debugger dispatch is handled"), Edit(Start, ConsoleToken(Preview(Start))).Contains(TEXT("Command handled: yes")));
        TestTrue(TEXT("Debugger stop is handled"), Edit(Stop, ConsoleToken(Preview(Stop))).Contains(TEXT("Command handled: yes")));
    }
    else AddError(TEXT("Stock editor Slate debugger commands unavailable."));
    AddInfo(LogResult);
    return true;
}
#endif

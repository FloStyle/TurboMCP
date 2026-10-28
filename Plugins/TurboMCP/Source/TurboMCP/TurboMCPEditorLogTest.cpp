#include "TurboMCPEditorLog.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tools/TurboMCPToolsets.h"
#include "Misc/AutomationTest.h"
#include "Logging/LogMacros.h"
#include "Async/Async.h"
DEFINE_LOG_CATEGORY_STATIC(LogTurboMCPEditorLogFixture, Log, All);
namespace
{
FString NextCursor(const FString& Text)
{
    FString Before, After;
    if (!Text.Split(TEXT("nextCursor: "), &Before, &After)) return FString();
    FString Cursor, Rest;
    return After.Split(TEXT("\n"), &Cursor, &Rest) ? Cursor : After;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPEditorLogTest, "TurboMCP.Diagnostics.EditorLog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPEditorLogTest::RunTest(const FString&)
{
    FTurboMCPEditorLog Local;
    Local.Serialize(TEXT("marker Alpha"), ELogVerbosity::Log, TEXT("Fixture"));
    Local.Serialize(TEXT("marker Beta"), ELogVerbosity::Display, TEXT("Fixture"));
    Local.Serialize(TEXT("other"), ELogVerbosity::Log, TEXT("Other"));
    const FString First = Local.Read(TEXT("fixture"), TEXT("MARKER"), TEXT(""), TEXT(""), 1);
    TestTrue(TEXT("Case-insensitive filter and whole record page"), First.Contains(TEXT("marker Alpha")) && !First.Contains(TEXT("marker Beta")) && First.Contains(TEXT("remaining matches=1")));
    const FString Cursor = NextCursor(First);
    TestFalse(TEXT("Cursor emitted"), Cursor.IsEmpty());
    const FString Second = Local.Read(TEXT("fixture"), TEXT("marker"), TEXT(""), Cursor, 1);
    TestTrue(TEXT("Exclusive cursor reaches next match without duplicate"), Second.Contains(TEXT("marker Beta")) && !Second.Contains(TEXT("marker Alpha")));
    const FString Empty = Local.Read(TEXT("fixture"), TEXT("marker"), TEXT(""), NextCursor(Second), 10);
    TestTrue(TEXT("No new records is explicit"), Empty.Contains(TEXT("shown=0")));
    TestTrue(TEXT("Future UTC filter returns empty"), Local.Read(TEXT(""), TEXT(""), TEXT("2099-01-01T00:00:00Z"), TEXT(""), 10).Contains(TEXT("shown=0")));
    TestTrue(TEXT("Malformed time refuses"), Local.Read(TEXT(""),TEXT(""),TEXT("yesterday"),TEXT(""),10).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Invalid limit refuses"), Local.Read(TEXT(""),TEXT(""),TEXT(""),TEXT(""),0).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Malformed cursor refuses"), Local.Read(TEXT(""),TEXT(""),TEXT(""),TEXT("oops"),10).StartsWith(TEXT("error:")));
    FTurboMCPEditorLog OtherSession;
    TestTrue(TEXT("Other session cursor refuses"), OtherSession.Read(TEXT(""),TEXT(""),TEXT(""),Cursor,10).Contains(TEXT("another log session")));
    FString Id, Number; Cursor.Split(TEXT(":"), &Id, &Number);
    TestTrue(TEXT("Future cursor refuses"), Local.Read(TEXT(""),TEXT(""),TEXT(""),Id+TEXT(":999999"),10).Contains(TEXT("ahead")));
    TestTrue(TEXT("Overflow cursor refuses"), Local.Read(TEXT(""),TEXT(""),TEXT(""),Id+TEXT(":18446744073709551616"),10).StartsWith(TEXT("error:")));
    FTurboMCPEditorLog Retention;
    const FString Start = NextCursor(Retention.Read(TEXT(""),TEXT(""),TEXT(""),TEXT(""),10));
    for (int32 I=0; I<2049; ++I) Retention.Serialize(*FString::Printf(TEXT("record %d"),I), ELogVerbosity::Log, TEXT("Fixture"));
    const FString Lost = Retention.Read(TEXT(""),TEXT(""),TEXT(""),Start,1);
    TestTrue(TEXT("Ring reports exact retention loss"), Lost.Contains(TEXT("retained: 2048")) && Lost.Contains(TEXT("lost since requested cursor: 1")) && Lost.Contains(TEXT("record 1")));
    FTurboMCPEditorLog Long;
    Long.Serialize(*(FString::ChrN(2050,TCHAR('x'))+TEXT("hidden-tail")), ELogVerbosity::Log,TEXT("Fixture"));
    const FString Truncated=Long.Read(TEXT(""),TEXT(""),TEXT(""),TEXT(""),10);
    TestTrue(TEXT("Truncated stored message is labelled"), Truncated.Contains(TEXT("truncated=yes")) && !Truncated.Contains(TEXT("hidden-tail")) && Truncated.Len()<=24000);
    TestTrue(TEXT("Discarded tail does not match filter"), Long.Read(TEXT(""),TEXT("hidden-tail"),TEXT(""),TEXT(""),10).Contains(TEXT("shown=0")));
    FTurboMCPEditorLog Budget;
    for (int32 I=0; I<30; ++I) Budget.Serialize(*(FString::Printf(TEXT("budget-marker-%d "),I)+FString::ChrN(2000,TCHAR('x'))),ELogVerbosity::Log,TEXT("Fixture"));
    FString BudgetCursor;
    int32 Seen = 0;
    for (int32 Page=0; Page<10; ++Page)
    {
        const FString Text=Budget.Read(TEXT("Fixture"),TEXT("budget-marker-"),TEXT(""),BudgetCursor,100);
        TestTrue(TEXT("Whole-record response budget respected"),Text.Len()<=24000);
        FString Before, After;
        Text.Split(TEXT("shown="),&Before,&After);
        const int32 Count=FCString::Atoi(*After);
        if (!Count) break;
        for (int32 I=Seen; I<Seen+Count; ++I)
            TestTrue(TEXT("Every expected record appears in cursor order"),Text.Contains(FString::Printf(TEXT("budget-marker-%d "),I)));
        Seen+=Count;
        const FString Next=NextCursor(Text);
        TestTrue(TEXT("Nonempty page advances cursor"),Next!=BudgetCursor);
        BudgetCursor=Next;
    }
    TestEqual(TEXT("Character-budget pagination loses no records"),Seen,30);
    FTurboMCPEditorLog Concurrent;
    auto Producer = [&Concurrent]() {for (int32 I=0; I<100; ++I) Concurrent.Serialize(TEXT("parallel"),ELogVerbosity::Log,TEXT("Fixture"));};
    auto A=Async(EAsyncExecution::ThreadPool,Producer); auto B=Async(EAsyncExecution::ThreadPool,Producer); A.Wait(); B.Wait();
    TestTrue(TEXT("Concurrent output retains exact record count"), Concurrent.Read(TEXT(""),TEXT(""),TEXT(""),TEXT(""),100).Contains(TEXT("retained: 200")));
    const FString Marker=TEXT("editor-log-live-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UE_LOG(LogTurboMCPEditorLogFixture,Display,TEXT("%s"),*Marker);
    const FString Live=UTurboMCPReadToolset::ReadTurboMCPEditorLog(TEXT("LogTurboMCPEditorLogFixture"),Marker,TEXT(""),TEXT(""),10);
    TestTrue(TEXT("Actual GLog marker reaches public reader"),Live.Contains(Marker) && Live.Contains(TEXT("shown=1")));
    const FString Help=UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCP.TurboMCPReadToolset"),TEXT("ReadTurboMCPEditorLog"));
    for(const TCHAR* Parameter:{TEXT("Category"),TEXT("Contains"),TEXT("Since"),TEXT("Cursor"),TEXT("Limit")})
        TestTrue(*(FString(TEXT("Live help documents "))+Parameter),Help.Contains(Parameter));
    AddInfo(Live); AddInfo(First); AddInfo(Lost);
    return !HasAnyErrors();
}
#endif

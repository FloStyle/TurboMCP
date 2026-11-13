// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Bulk export keeps the reader body and its authoritative result trailer together.
// A trailer is metadata, never another page of search results.
namespace TurboMCPBulkExport
{
static bool ParseObject(const FString& Text, TSharedPtr<FJsonObject>& Object)
{
    return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) && Object.IsValid();
}

static bool ParsePage(const FString& Text, bool RequireResult,
    TSharedPtr<FJsonObject>& Envelope, FString& Error)
{
    Envelope.Reset();
    Error.Reset();
    const FString Marker = TEXT("\n; tmcp-result-v1:");
    const int32 Split = Text.Find(Marker, ESearchCase::CaseSensitive);
    TSharedPtr<FJsonObject> Body;
    if (!ParseObject(Split == INDEX_NONE ? Text : Text.Left(Split), Body))
    {
        Error = TEXT("The reader did not return a valid JSON page. No page was accepted.");
        return false;
    }
    TSharedPtr<FJsonObject> Result;
    if (Split != INDEX_NONE)
    {
        if (!ParseObject(Text.Mid(Split + Marker.Len()), Result))
        {
            Error = TEXT("The reader result metadata is malformed. No page was accepted.");
            return false;
        }
    }
    else if (RequireResult)
    {
        Error = TEXT("The reader result metadata is missing. Completion cannot be established.");
        return false;
    }
    Envelope = MakeShared<FJsonObject>();
    Envelope->SetObjectField(TEXT("data"), Body);
    if (Result) Envelope->SetObjectField(TEXT("result"), Result);
    return true;
}
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPBulkExportFramingTest,
    "TurboMCP.Read.BulkExportFraming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPBulkExportFramingTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> Page;
    FString Error;
    TestTrue(TEXT("Plain JSON page"), TurboMCPBulkExport::ParsePage(TEXT("{\"records\":[]}"), false, Page, Error));
    TestTrue(TEXT("Widget result preserved"), TurboMCPBulkExport::ParsePage(
        TEXT("{\"matches\":[]}\n; tmcp-result-v1: {\"complete\":false,\"outcome\":\"partial\"}"), true, Page, Error));
    if (Page) TestFalse(TEXT("Incomplete metadata remains incomplete"), Page->GetObjectField(TEXT("result"))->GetBoolField(TEXT("complete")));
    TestFalse(TEXT("Missing required result refused"), TurboMCPBulkExport::ParsePage(TEXT("{}"), true, Page, Error));
    TestFalse(TEXT("Malformed result refused"), TurboMCPBulkExport::ParsePage(TEXT("{}\n; tmcp-result-v1: broken"), true, Page, Error));
    TestFalse(TEXT("Reader text refusal is not a page"), TurboMCPBulkExport::ParsePage(TEXT("error: query refused"), false, Page, Error));
    return true;
}
#endif

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPToolPolicy.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Bulk export keeps the reader body and its authoritative result trailer together.
// A trailer is metadata, never another page of search results.
namespace TurboMCPBulkExport
{
static bool ParseObject(const FString& Text, TSharedPtr<FJsonObject>& Object)
{
    // Reject concatenated documents and any trailing material, including a second result.
    const FString Trimmed = Text.TrimStartAndEnd();
    if (!Trimmed.StartsWith(TEXT("{"))) return false;
    int32 Depth = 0;
    bool InString = false, Escaped = false;
    for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
    {
        const TCHAR C = Trimmed[Index];
        if (InString)
        {
            if (Escaped) Escaped = false;
            else if (C == TEXT('\\')) Escaped = true;
            else if (C == TEXT('"')) InString = false;
        }
        else if (C == TEXT('"')) InString = true;
        else if (C == TEXT('{') || C == TEXT('[')) ++Depth;
        else if (C == TEXT('}') || C == TEXT(']'))
        {
            --Depth;
            if (Depth == 0 && Index != Trimmed.Len() - 1) return false;
            if (Depth < 0) return false;
        }
    }
    return Depth == 0 && !InString && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Trimmed), Object) && Object.IsValid();
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

static FString Json(const TSharedRef<FJsonObject>& Object)
{
    FString Text;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    return Text;
}

// Query names are intentionally exact. Silently ignoring a typo can export a different scope.
static bool ValidateQuery(const TSharedPtr<FJsonObject>& Query, const FString& Reader, FString& Error)
{
    FString Strings, Bools, Numbers;
    if (Reader == TEXT("ReadTurboMCPRefsBatch"))
    {
        Strings = TEXT("|assets|folder|classFilter|direction|kinds|");
        Bools = TEXT("|recursive|compact|stopAtRoot|includeSeeds|"); Numbers = TEXT("|depth|pageSize|");
    }
    else if (Reader == TEXT("ReadTurboMCPNodeFind"))
    {
        Strings = TEXT("|asset|words|graphName|matchMode|nodeClass|scope|");
        Bools = TEXT("|includeLiterals|recursive|countOnly|"); Numbers = TEXT("|pageSize|");
    }
    else
    {
        Strings = TEXT("|rules|folder|matchMode|"); Bools = TEXT("|recursive|loadUnloaded|"); Numbers = TEXT("|pageSize|");
    }
    for (const auto& Field : Query->Values)
    {
        const FString Key = TEXT("|") + FString(*Field.Key) + TEXT("|");
        const EJson Expected = Strings.Contains(Key, ESearchCase::CaseSensitive) ? EJson::String : Bools.Contains(Key, ESearchCase::CaseSensitive) ? EJson::Boolean : Numbers.Contains(Key, ESearchCase::CaseSensitive) ? EJson::Number : EJson::None;
        if (Expected == EJson::None || !Field.Value.IsValid() || Field.Value->Type != Expected)
        {
            Error = TEXT("Unknown query field or incorrect JSON type: ") + FString(*Field.Key);
            return false;
        }
        if (Expected == EJson::Number)
        {
            const double Number = Field.Value->AsNumber();
            if (!FMath::IsFinite(Number) || Number < 1 || Number > 1000 || Number != FMath::FloorToDouble(Number))
            {
                Error = TEXT("Query numeric fields must be integers from 1 to 1000; reader limits also apply.");
                return false;
            }
        }
    }
    return true;
}
}

FString UTurboMCPReadToolset::ReadTurboMCPBulkExport(const FString& Reader, const FString& QueryJSON,
    int32 MaxPages, int32 MaxSeconds, int32 MaxMegabytes)
{
    using namespace TurboMCPBulkExport;
    if (Reader != TEXT("ReadTurboMCPRefsBatch") && Reader != TEXT("ReadTurboMCPNodeFind") && Reader != TEXT("ReadTurboMCPWidgetInventory"))
        return TEXT("error: Reader must be ReadTurboMCPRefsBatch, ReadTurboMCPNodeFind or ReadTurboMCPWidgetInventory.");
    if (MaxPages < 1 || MaxPages > 1000 || MaxSeconds < 1 || MaxSeconds > 120 || MaxMegabytes < 1 || MaxMegabytes > 64)
        return TEXT("error: Limits must be MaxPages 1..1000, MaxSeconds 1..120 and MaxMegabytes 1..64.");
    FString Reason;
    if (!TurboMCPToolPolicy::IsAllowed(Reader, Reason)) return TEXT("error: ") + Reader + TEXT(": ") + Reason;
    TSharedPtr<FJsonObject> Query;
    if (QueryJSON.Len() > 65536 || !ParseObject(QueryJSON, Query)) return TEXT("error: QueryJSON must be one JSON object, at most 65536 characters.");
    if (!ValidateQuery(Query, Reader, Reason)) return TEXT("error: ") + Reason;
    const auto S = [&Query](const TCHAR* Key, const TCHAR* Default) { FString V; return Query->TryGetStringField(Key,V) ? V : FString(Default); };
    const auto B = [&Query](const TCHAR* Key, bool Default) { bool V; return Query->TryGetBoolField(Key,V) ? V : Default; };
    const auto N = [&Query](const TCHAR* Key, int32 Default) { double V; return Query->TryGetNumberField(Key,V) ? static_cast<int32>(V) : Default; };
    const bool IsWidget = Reader == TEXT("ReadTurboMCPWidgetInventory");
    const bool IsNode = Reader == TEXT("ReadTurboMCPNodeFind");
    const double Started = FPlatformTime::Seconds();
    const int64 LimitBytes = static_cast<int64>(MaxMegabytes) * 1024 * 1024;
    TArray<TSharedPtr<FJsonValue>> Pages;
    TSet<FString> Seen;
    FString Cursor = TEXT("first");
    bool Finished = false, Coverage = true;
    int64 PageBytes = 0;
    Reason = TEXT("page_limit");
    for (int32 PageIndex = 0; PageIndex < MaxPages; ++PageIndex)
    {
        if (FPlatformTime::Seconds() - Started >= MaxSeconds) { Reason = TEXT("time_limit"); break; }
        if (Seen.Contains(Cursor)) { Reason = TEXT("repeated_cursor"); break; }
        Seen.Add(Cursor);
        FString Raw;
        if (IsNode)
            Raw = ReadTurboMCPNodeFind(S(TEXT("asset"),TEXT("")), S(TEXT("words"),TEXT("")), S(TEXT("graphName"),TEXT("")),
                S(TEXT("matchMode"),TEXT("all")), B(TEXT("includeLiterals"),false), S(TEXT("nodeClass"),TEXT("any")),
                S(TEXT("scope"),TEXT("asset")), B(TEXT("recursive"),true), N(TEXT("pageSize"),40), Cursor, B(TEXT("countOnly"),false), TEXT("json"));
        else if (IsWidget)
            Raw = ReadTurboMCPWidgetInventory(S(TEXT("rules"),TEXT("")), S(TEXT("folder"),TEXT("project")), B(TEXT("recursive"),true),
                S(TEXT("matchMode"),TEXT("all")), N(TEXT("pageSize"),8), Cursor, B(TEXT("loadUnloaded"),true));
        else
            Raw = ReadTurboMCPRefsBatch(S(TEXT("assets"),TEXT("<none>")),S(TEXT("folder"),TEXT("<none>")),B(TEXT("recursive"),false),
                S(TEXT("classFilter"),TEXT("<none>")),S(TEXT("direction"),TEXT("both")),S(TEXT("kinds"),TEXT("all")),N(TEXT("depth"),1),
                B(TEXT("compact"),true),B(TEXT("stopAtRoot"),false),N(TEXT("pageSize"),100),Cursor,B(TEXT("includeSeeds"),true));
        TSharedPtr<FJsonObject> Page;
        if (!ParsePage(Raw, IsWidget, Page, Reason)) { Reason += TEXT(" Reader response: ") + Raw.Left(4096); break; }
        const auto Body = Page->GetObjectField(TEXT("data"));
        FString Status;
        if (Body->TryGetStringField(TEXT("status"),Status) && Status == TEXT("error")) { Reason = TEXT("reader_error: ") + Raw.Left(4096); break; }
        FString Next;
        const TCHAR* CursorKey = IsNode ? TEXT("next_cursor") : TEXT("nextCursor");
        if (!Body->HasField(CursorKey)) { Reason = TEXT("missing_cursor"); break; }
        Body->TryGetStringField(CursorKey,Next);
        bool PageComplete = false;
        if (IsWidget)
        {
            const auto Result = Page->GetObjectField(TEXT("result"));
            bool Truncated = false; Result->TryGetBoolField(TEXT("truncated"),Truncated);
            double Skipped = 0, Errors = 0;
            Body->TryGetNumberField(TEXT("skipped"),Skipped); Body->TryGetNumberField(TEXT("errors"),Errors);
            PageComplete = !Truncated && Skipped == 0 && Errors == 0;
            if (Next.IsEmpty()) { bool FinalComplete = false; Result->TryGetBoolField(TEXT("complete"),FinalComplete); PageComplete &= FinalComplete; }
        }
        else if (IsNode)
        {
            double Skipped = 0; Body->TryGetNumberField(TEXT("skipped_assets_so_far"),Skipped);
            PageComplete = Skipped == 0;
            if (Next.IsEmpty()) Body->TryGetBoolField(TEXT("complete"),PageComplete);
        }
        else Body->TryGetBoolField(TEXT("complete"),PageComplete);
        const FString PageText = Json(Page.ToSharedRef());
        const int64 Bytes = FTCHARToUTF8(*PageText).Length();
        if (PageBytes + Bytes + 131072 > LimitBytes) { Reason = TEXT("size_limit"); break; }
        PageBytes += Bytes; Pages.Add(MakeShared<FJsonValueObject>(Page)); Coverage &= PageComplete;
        Cursor = Next;
        if (Cursor.IsEmpty()) { Finished = true; Reason = Coverage ? TEXT("finished") : TEXT("reader_coverage_incomplete"); break; }
        Reason = TEXT("page_limit");
    }
    const auto Document = MakeShared<FJsonObject>();
    Document->SetNumberField(TEXT("schema"),1);
    Document->SetStringField(TEXT("reader"),Reader); Document->SetObjectField(TEXT("query"),Query);
    Document->SetBoolField(TEXT("traversalFinished"),Finished); Document->SetBoolField(TEXT("complete"),Finished && Coverage);
    Document->SetStringField(TEXT("reason"),Reason); Document->SetStringField(TEXT("nextCursor"),Cursor);
    Document->SetArrayField(TEXT("pages"),Pages);
    const FString Text = Json(Document);
    if (FTCHARToUTF8(*Text).Length() > LimitBytes) return TEXT("error: Export metadata exceeded the output limit. No artifact was written.");
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("TurboMCP/Exports"));
    IFileManager& Files = IFileManager::Get();
    if (!Files.MakeDirectory(*Directory,true)) return TEXT("error: Could not create the report directory. No artifact was written.");
    const FString Path = Directory / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json"));
    const FString Temporary = Path + TEXT(".tmp");
    if (Files.FileExists(*Path) || Files.FileExists(*Temporary)) return TEXT("error: Report path collision. No existing file was overwritten.");
    if (!FFileHelper::SaveStringToFile(Text,*Temporary,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !Files.Move(*Path,*Temporary,false,false,false,true))
    {
        Files.Delete(*Temporary,false,true);
        return TEXT("error: Report write failed. No completed artifact is available.");
    }
    Document->RemoveField(TEXT("pages")); Document->RemoveField(TEXT("query"));
    Document->SetStringField(TEXT("path"),Path); Document->SetNumberField(TEXT("pageCount"),Pages.Num());
    Document->SetNumberField(TEXT("bytes"),FTCHARToUTF8(*Text).Length());
    Document->SetStringField(TEXT("effects"),TEXT("Writes a report file, not project assets. Reader package loads may execute PostLoad; per-page effects are retained in the file."));
    return Json(Document);
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
    TestFalse(TEXT("Concatenated documents refused"), TurboMCPBulkExport::ParsePage(TEXT("{}{}"), false, Page, Error));
    TestFalse(TEXT("Missing required result refused"), TurboMCPBulkExport::ParsePage(TEXT("{}"), true, Page, Error));
    TestFalse(TEXT("Malformed result refused"), TurboMCPBulkExport::ParsePage(TEXT("{}\n; tmcp-result-v1: broken"), true, Page, Error));
    TestFalse(TEXT("Reader text refusal is not a page"), TurboMCPBulkExport::ParsePage(TEXT("error: query refused"), false, Page, Error));
    return true;
}
#endif

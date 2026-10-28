#include "TurboMCPEditorLog.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "String/LexFromString.h"

namespace
{
constexpr int32 MaxRecords = 2048;
constexpr int32 MaxMessageChars = 2048;
constexpr int32 MaxOutputChars = 24000;
}

FTurboMCPEditorLog& FTurboMCPEditorLog::Get()
{
    static FTurboMCPEditorLog Instance;
    return Instance;
}

void FTurboMCPEditorLog::Start()
{
    // Module lifetime owns registration. Never hold the record mutex while
    // attaching/detaching: redirector callbacks can be concurrent.
    if (Attached || !GLog) return;
    {
        FScopeLock Lock(&Mutex);
        Session = FGuid::NewGuid(); Sequence = 0; Head = 0; Records.Reset();
    }
    GLog->AddOutputDevice(this);
    Attached = true;
}

void FTurboMCPEditorLog::Stop()
{
    if (Attached && GLog) GLog->RemoveOutputDevice(this);
    Attached = false;
}

void FTurboMCPEditorLog::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
    const auto Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
    if (Level == ELogVerbosity::SetColor || Level == ELogVerbosity::NoLogging) return;
    FRecord Entry;
    Entry.Time = FDateTime::UtcNow(); Entry.Category = Category.ToString(); Entry.Verbosity = Level;
    const TCHAR* Text = Message ? Message : TEXT("");
    const int32 OriginalLength = FCString::Strlen(Text);
    int32 Keep = FMath::Min(OriginalLength, MaxMessageChars);
    // Do not leave an unmatched UTF-16 high surrogate at the truncation edge.
    if (Keep < OriginalLength && Keep > 0 && Text[Keep - 1] >= 0xD800 && Text[Keep - 1] <= 0xDBFF) --Keep;
    Entry.Message = FString(Keep, Text); Entry.Truncated = Keep < OriginalLength;
    FScopeLock Lock(&Mutex);
    Entry.Sequence = ++Sequence;
    if (Records.Num() < MaxRecords) Records.Add(MoveTemp(Entry));
    else { Records[Head] = MoveTemp(Entry); Head = (Head + 1) % MaxRecords; }
}

FString FTurboMCPEditorLog::Read(const FString& Category, const FString& Contains, const FString& Since, const FString& Cursor, int32 Limit)
{
    if (Limit < 1 || Limit > 100) return TEXT("error: Limit must be between 1 and 100.");
    if (Category.Len() > 256 || Contains.Len() > 2048) return TEXT("error: Category is limited to 256 characters and Contains to 2048 characters.");
    FDateTime SinceTime = FDateTime::MinValue();
    if (!Since.IsEmpty() && (!Since.EndsWith(TEXT("Z")) || !FDateTime::ParseIso8601(*Since, SinceTime)))
        return TEXT("error: Since must be an ISO-8601 UTC timestamp ending in Z, or empty.");
    FGuid RequestedSession;
    uint64 After = 0;
    if (Cursor.Len() > 53) return TEXT("error: Cursor is too long; copy a returned session:sequence value.");
    if (!Cursor.IsEmpty())
    {
        FString Id, Number;
        if (!Cursor.Split(TEXT(":"), &Id, &Number) || Id.Len() != 32 || !FGuid::ParseExact(Id, EGuidFormats::Digits, RequestedSession)
            || Number.IsEmpty() || Number.Len() > 20)
            return TEXT("error: Cursor must be the returned session:sequence value, or empty.");
        for (TCHAR Char : Number) if (Char < '0' || Char > '9') return TEXT("error: Cursor sequence must contain decimal digits only.");
        if (!LexTryParseString(After, *Number) || FString::Printf(TEXT("%llu"), After) != Number)
            return TEXT("error: Cursor sequence is invalid or out of range.");
    }
    TArray<FRecord> Snapshot;
    FGuid CurrentSession;
    uint64 Latest = 0;
    {
        FScopeLock Lock(&Mutex);
        CurrentSession = Session; Latest = Sequence;
        if (!Cursor.IsEmpty() && RequestedSession != Session)
            return TEXT("error: Cursor belongs to another log session. Restart with an empty Cursor; earlier session history is unavailable.");
        if (After > Latest) return TEXT("error: Cursor is ahead of this log session. Use the last returned cursor or restart with an empty Cursor.");
        Snapshot.Reserve(Records.Num());
        for (int32 I = 0; I < Records.Num(); ++I) Snapshot.Add(Records[(Head + I) % Records.Num()]);
    }
    const uint64 Oldest = Snapshot.IsEmpty() ? Latest + 1 : Snapshot[0].Sequence;
    const uint64 Lost = !Cursor.IsEmpty() && After < Oldest - 1 ? Oldest - 1 - After : 0;
    uint64 Next = FMath::Max(After, Oldest - 1);
    auto Matches = [&](const FRecord& Record) {
        return Record.Sequence > After && (Category.IsEmpty() || Record.Category.Equals(Category, ESearchCase::IgnoreCase))
            && (Contains.IsEmpty() || Record.Message.Contains(Contains, ESearchCase::IgnoreCase)) && Record.Time >= SinceTime;
    };
    int32 Matched = 0;
    for (const auto& Record : Snapshot) if (Matches(Record)) ++Matched;
    FString Out = FString::Printf(TEXT("editor output log (not tool-call history)\nsession: %s\nretained: %d; capacity: %d; discarded since session start: %llu; lost since requested cursor: %llu\nOnly output captured while TurboMCP is enabled is available. Editor and PIE records share this stream; no world attribution is inferred.\n"),
        *CurrentSession.ToString(EGuidFormats::Digits), Snapshot.Num(), MaxRecords, Latest - Snapshot.Num(), Lost);
    int32 Shown = 0, Scanned = 0;
    for (const auto& Record : Snapshot)
    {
        if (Record.Sequence <= After) continue;
        if (Matches(Record))
        {
            const FString Row = FString::Printf(TEXT("\n%llu %s [%s] %s truncated=%s: %s"), Record.Sequence, *Record.Time.ToIso8601(),
                *Record.Category.ReplaceCharWithEscapedChar(), ToString(Record.Verbosity), Record.Truncated ? TEXT("yes") : TEXT("no"), *Record.Message.ReplaceCharWithEscapedChar());
            if (Shown >= Limit || Out.Len() + Row.Len() > MaxOutputChars - 600) break;
            Out += Row; ++Shown;
        }
        ++Scanned; Next = Record.Sequence;
    }
    Out += FString::Printf(TEXT("\npage: matched retained=%d; shown=%d; remaining matches=%d; scanned=%d\nnextCursor: %s:%llu\nRepeat with the same filters and nextCursor. Filters match stored text only; truncated tails and discarded records cannot be searched. A later call may report new retention loss."),
        Matched, Shown, Matched - Shown, Scanned, *CurrentSession.ToString(EGuidFormats::Digits), Next);
    return Out;
}

FString UTurboMCPReadToolset::ReadTurboMCPEditorLog(const FString& Category, const FString& Contains, const FString& Since, const FString& Cursor, int32 Limit)
{
    return FTurboMCPEditorLog::Get().Read(Category, Contains, Since, Cursor, Limit);
}

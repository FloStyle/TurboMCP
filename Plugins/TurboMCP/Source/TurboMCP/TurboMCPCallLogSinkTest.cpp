// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "TurboMCPCallLog.h"

#include "Async/Async.h"
#include "Containers/Set.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/Event.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/UniquePtr.h"

namespace
{
	using FCallLogStage = FTurboMCPCallLog::EDiskStage;
	using FCallLogEntry = FTurboMCPCallLog::FEntry;

	FCallLogEntry MakeCallLogEntry(const FString& Tool)
	{
		FCallLogEntry Entry;
		Entry.Timestamp = TEXT("2026-11-17T12:00:00Z");
		Entry.Phase = TEXT("end");
		Entry.Source = TEXT("plugin");
		Entry.SessionId = TEXT("fixture-session");
		Entry.Toolset = TEXT("Fixture");
		Entry.Tool = Tool;
		Entry.Arguments = TEXT("{\"enabled\":true}");
		Entry.Result = TEXT("changed: a fixture value");
		Entry.DurationMs = 12.25;
		Entry.ResultChars = 123;
		Entry.TokenEstimate = 31;
		return Entry;
	}

	bool ReadCallLogLines(FAutomationTestBase& Test, const FString& Path, int64 FileLimit,
		TArray<TSharedPtr<FJsonObject>>& Out)
	{
		Out.Reset();
		TArray<uint8> Bytes;
		if (!Test.TestTrue(TEXT("Owned log bytes can be read"), FFileHelper::LoadFileToArray(Bytes, *Path)))
		{
			return false;
		}
		if (!Test.TestTrue(TEXT("Log file is nonempty and within its byte limit"),
			!Bytes.IsEmpty() && Bytes.Num() <= FileLimit))
		{
			return false;
		}
		Test.TestEqual(TEXT("Log ends at a record boundary"), Bytes.Last(), static_cast<uint8>('\n'));
		const auto Decoded = StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
		const auto RoundTrip = StringCast<UTF8CHAR>(Decoded.Get(), Decoded.Length());
		if (!Test.TestTrue(TEXT("All log bytes are valid UTF-8"),
			RoundTrip.Length() == Bytes.Num() && FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) == 0))
		{
			return false;
		}
		const FString Text(Decoded.Length(), Decoded.Get());
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			const auto Encoded = StringCast<UTF8CHAR>(*Line, Line.Len());
			Test.TestTrue(TEXT("One record plus its line ending fits the disk record limit"),
				Encoded.Length() + FCString::Strlen(LINE_TERMINATOR) <= 16 * 1024);
			TSharedPtr<FJsonObject> Object;
			if (!Test.TestTrue(TEXT("Each line parses as one JSON object"),
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Object) && Object.IsValid()))
			{
				return false;
			}
			Out.Add(MoveTemp(Object));
		}
		return true;
	}

	bool CallLogBytesEqual(const FString& Path, const TArray<uint8>& Expected)
	{
		TArray<uint8> Actual;
		return FFileHelper::LoadFileToArray(Actual, *Path) && Actual == Expected;
	}

	bool HasCallLogFields(const FJsonObject& Object, const FCallLogEntry& Entry)
	{
		FString Text;
		double Number = 0;
		bool Error = false;
		auto MatchesText = [&](const TCHAR* Name, const FString& Expected)
		{
			return Object.TryGetStringField(Name, Text) && Text == Expected;
		};
		return MatchesText(TEXT("time"), Entry.Timestamp) && MatchesText(TEXT("phase"), Entry.Phase)
			&& MatchesText(TEXT("source"), Entry.Source) && MatchesText(TEXT("session"), Entry.SessionId)
			&& MatchesText(TEXT("toolset"), Entry.Toolset) && MatchesText(TEXT("tool"), Entry.Tool)
			&& Object.TryGetNumberField(TEXT("duration_ms"), Number) && Number == Entry.DurationMs
			&& Object.TryGetNumberField(TEXT("result_chars"), Number) && Number == Entry.ResultChars
			&& Object.TryGetNumberField(TEXT("token_estimate"), Number) && Number == Entry.TokenEstimate
			&& Object.TryGetBoolField(TEXT("error"), Error) && Error == Entry.bError;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCallLogSinkTest, "TurboMCP.Diagnostics.CallLogSink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPCallLogSinkTest::RunTest(const FString&)
{
	IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
	const FString TempBase = FPlatformProcess::UserTempDir();
	if (!TestFalse(TEXT("The OS temporary directory is available"), TempBase.IsEmpty()))
	{
		return false;
	}
	const FString Root = FPaths::Combine(TempBase, TEXT("TurboMCP-CallLogSink-") + FGuid::NewGuid().ToString());
	if (!TestTrue(TEXT("The fixture owns a new temporary directory"),
		!Files.DirectoryExists(*Root) && !Files.FileExists(*Root) && Files.CreateDirectory(*Root)))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		TestTrue(TEXT("The owned temporary directory is removed"), Files.DeleteDirectoryRecursively(*Root));
	};

	// The real sink writes to this isolated path. Refusals are injected at the individual file operations.
	const FString Path = FPaths::Combine(Root, TEXT("missing"), TEXT("calls.jsonl"));
	const FString PreviousPath = Path + TEXT(".1");
	constexpr int64 FileLimit = 1300;
	TOptional<FCallLogStage> RefusedStage = FCallLogStage::Directory;
	TUniquePtr<FTurboMCPCallLog> Log(FTurboMCPCallLog::CreateDiskSinkForTest(Path, FileLimit,
		[&](FCallLogStage Stage) { return RefusedStage.IsSet() && RefusedStage.GetValue() == Stage; }));
	const FCallLogEntry Small = MakeCallLogEntry(TEXT("small"));
	Log->Write(Small);
	TestFalse(TEXT("Denied directory creation emits no file"), Files.FileExists(*Path));
	TestTrue(TEXT("Directory failure is visible separately"), Log->RenderDiskStatus().Contains(TEXT("directory=1")));

	RefusedStage = FCallLogStage::OpenAppend;
	Log->Write(Small);
	TestFalse(TEXT("Denied append open emits no file"), Files.FileExists(*Path));
	TestTrue(TEXT("Append admission failure is visible"), Log->RenderDiskStatus().Contains(TEXT("append open=1")));

	RefusedStage.Reset();
	Log->Write(Small);
	const auto Initial = Log->GetSnapshot();
	if (!TestEqual(TEXT("Each write is admitted to memory once"), Initial.Entries.Num(), 3))
	{
		return false;
	}
	for (const FCallLogEntry& Entry : Initial.Entries)
	{
		TestTrue(TEXT("Disk errors do not change operation data"),
			Entry.Result == Small.Result && Entry.Arguments == Small.Arguments && Entry.DurationMs == Small.DurationMs
			&& Entry.ResultChars == Small.ResultChars && Entry.TokenEstimate == Small.TokenEstimate && !Entry.bError);
	}
	TestTrue(TEXT("A later success retains both failure counts"),
		Log->RenderTimeline().Contains(TEXT("directory=1 append open=1")));
	TArray<TSharedPtr<FJsonObject>> Rows;
	if (!ReadCallLogLines(*this, Path, FileLimit, Rows) || !TestEqual(TEXT("One small record reached disk"), Rows.Num(), 1))
	{
		return false;
	}
	const TSharedPtr<FJsonObject>& SmallRow = Rows[0];
	FString Arguments;
	FString Result;
	TestTrue(TEXT("Ordinary JSON fields and measured values remain exact"),
		SmallRow.IsValid() && HasCallLogFields(*SmallRow, Small)
		&& SmallRow->TryGetStringField(TEXT("arguments"), Arguments) && Arguments == Small.Arguments
		&& SmallRow->TryGetStringField(TEXT("result"), Result) && Result == Small.Result
		&& !SmallRow->HasField(TEXT("omitted_utf8_bytes")));

	TArray<uint8> SmallBytes;
	if (!TestTrue(TEXT("Known initial bytes are retained for failure comparison"), FFileHelper::LoadFileToArray(SmallBytes, *Path)))
	{
		return false;
	}
	RefusedStage = FCallLogStage::Size;
	Log->Write(Small);
	TestTrue(TEXT("Unknown file size refuses growth"), CallLogBytesEqual(Path, SmallBytes));
	TestTrue(TEXT("Size failure is visible"), Log->RenderDiskStatus().Contains(TEXT("size=1")));
	RefusedStage = FCallLogStage::Write;
	Log->Write(Small);
	TestTrue(TEXT("Denied write leaves prior bytes exact"), CallLogBytesEqual(Path, SmallBytes));
	TestTrue(TEXT("Write failure is visible"), Log->RenderDiskStatus().Contains(TEXT("write=1")));
	RefusedStage = FCallLogStage::Flush;
	Log->Write(Small);
	TestTrue(TEXT("Flush failure does not claim persistence"),
		Log->RenderDiskStatus().Contains(TEXT("flush=1")) && Log->RenderDiskStatus().Contains(TEXT("persistence was not verified")));
	RefusedStage.Reset();
	TestTrue(TEXT("A successful write with failed flush still forms a complete record"),
		ReadCallLogLines(*this, Path, FileLimit, Rows) && Rows.Num() == 2);

	FCallLogEntry Pressure = MakeCallLogEntry(TEXT("pressure"));
	Pressure.Result = FString::ChrN(600, TEXT('x'));
	Log->Write(Pressure);
	Log->Write(Pressure);
	TArray<uint8> ActiveBytes;
	TArray<uint8> PreviousBytes;
	if (!TestTrue(TEXT("A successful rotation creates both files"), FFileHelper::LoadFileToArray(ActiveBytes, *Path)
		&& FFileHelper::LoadFileToArray(PreviousBytes, *PreviousPath)))
	{
		return false;
	}

	RefusedStage = FCallLogStage::DeleteBackup;
	Log->Write(Pressure);
	TestTrue(TEXT("Refused backup deletion preserves both files"),
		CallLogBytesEqual(Path, ActiveBytes) && CallLogBytesEqual(PreviousPath, PreviousBytes));
	TestTrue(TEXT("Deletion refusal is visible"), Log->RenderDiskStatus().Contains(TEXT("backup deletion=1")));
	RefusedStage = FCallLogStage::MoveActive;
	Log->Write(Pressure);
	TestTrue(TEXT("Failed move preserves active bytes and emits no new rotate marker"),
		CallLogBytesEqual(Path, ActiveBytes));
	TestFalse(TEXT("The previously deleted backup is absent"), Files.FileExists(*PreviousPath));
	TestTrue(TEXT("The failed move reports the specific backup loss"),
		Log->RenderDiskStatus().Contains(TEXT("rotation move=1"))
		&& Log->RenderDiskStatus().Contains(TEXT("previous backups lost during failed rotation: 1")));

	// Both producers start at the same full-file boundary. Each completed call must survive once
	// across active and backup even though the seed generation is no longer retained.
	RefusedStage.Reset();
	FCallLogEntry First = Pressure;
	First.Tool = TEXT("parallel-a");
	FCallLogEntry Second = Pressure;
	Second.Tool = TEXT("parallel-b");
	const int32 EntriesBefore = Log->NumEntries();
	FEventRef Start(EEventMode::ManualReset);
	FEventRef ReadyA;
	FEventRef ReadyB;
	auto A = Async(EAsyncExecution::Thread, [&]
	{
		ReadyA->Trigger();
		Start->Wait();
		Log->Write(First);
	});
	auto B = Async(EAsyncExecution::Thread, [&]
	{
		ReadyB->Trigger();
		Start->Wait();
		Log->Write(Second);
	});
	const bool bReadyA = ReadyA->Wait(5000);
	const bool bReadyB = ReadyB->Wait(5000);
	Start->Trigger();
	A.Wait();
	B.Wait();
	TestTrue(TEXT("Both producers reach the same starting gate"), bReadyA && bReadyB);
	TestEqual(TEXT("Both concurrent writes are admitted to memory exactly once"), Log->NumEntries(), EntriesBefore + 2);
	TSet<FString> ConcurrentTools;
	for (const FString& File : {Path, PreviousPath})
	{
		if (!ReadCallLogLines(*this, File, FileLimit, Rows) || !TestEqual(TEXT("Each generation has a marker and one record"), Rows.Num(), 2))
		{
			return false;
		}
		FString Phase;
		FString Tool;
		TestTrue(TEXT("Each real rotation has exactly one leading marker"),
			Rows[0]->TryGetStringField(TEXT("phase"), Phase) && Phase == TEXT("rotate")
			&& Rows[1]->TryGetStringField(TEXT("phase"), Phase) && Phase == TEXT("end"));
		TestTrue(TEXT("Each generation retains one admitted call"), Rows[1]->TryGetStringField(TEXT("tool"), Tool));
		ConcurrentTools.Add(Tool);
	}
	TestTrue(TEXT("Both distinct concurrent calls survive without duplicate records"),
		ConcurrentTools.Num() == 2 && ConcurrentTools.Contains(First.Tool) && ConcurrentTools.Contains(Second.Tool));
	TestTrue(TEXT("Later rotations retain the prior loss diagnostic"),
		Log->RenderDiskStatus().Contains(TEXT("previous backups lost during failed rotation: 1")));

	// A historic partial record is kept for inspection instead of accepting a
	// new record that would join its bytes to the incomplete line.
	const FString PartialPath = FPaths::Combine(Root, TEXT("partial.jsonl"));
	const TArray<uint8> PartialBytes = {'{', '"', 'x'};
	if (!TestTrue(TEXT("Partial record fixture is written"), FFileHelper::SaveArrayToFile(PartialBytes, *PartialPath)))
	{
		return false;
	}
	TUniquePtr<FTurboMCPCallLog> PartialLog(FTurboMCPCallLog::CreateDiskSinkForTest(PartialPath, FileLimit));
	PartialLog->Write(Small);
	TestTrue(TEXT("An unterminated existing line refuses further growth"),
		CallLogBytesEqual(PartialPath, PartialBytes) && PartialLog->NumEntries() == 1
		&& PartialLog->RenderDiskStatus().Contains(TEXT("complete final line")));

	const FString UnicodePath = FPaths::Combine(Root, TEXT("unicode.jsonl"));
	constexpr int64 UnicodeLimit = 32 * 1024;
	TUniquePtr<FTurboMCPCallLog> UnicodeLog(FTurboMCPCallLog::CreateDiskSinkForTest(UnicodePath, UnicodeLimit));
	FCallLogEntry Large = MakeCallLogEntry(TEXT("unicode"));
	Large.Arguments.Empty();
	for (int32 Index = 0; Index < 1800; ++Index)
	{
		Large.Arguments += TEXT("\U0001F642\u03c0\"\\\n");
	}
	Large.Result = Large.Arguments;
	Large.ResultChars = Large.Result.Len();
	Large.TokenEstimate = 999;
	Large.bError = true;
	UnicodeLog->Write(Large);
	if (!ReadCallLogLines(*this, UnicodePath, UnicodeLimit, Rows) || !TestEqual(TEXT("One Unicode record"), Rows.Num(), 1))
	{
		return false;
	}
	const TSharedPtr<FJsonObject>& LargeRow = Rows[0];
	TestTrue(TEXT("Oversized records preserve factual metrics and error status"),
		LargeRow.IsValid() && HasCallLogFields(*LargeRow, Large));
	const TSharedPtr<FJsonObject>* Omissions = nullptr;
	if (!TestTrue(TEXT("Omitted UTF-8 bytes are identified by field"),
		LargeRow->TryGetObjectField(TEXT("omitted_utf8_bytes"), Omissions) && Omissions && Omissions->IsValid()))
	{
		return false;
	}
	for (const TCHAR* Field : {TEXT("arguments"), TEXT("result")})
	{
		FString Retained;
		double Count = 0;
		const bool bPresent = LargeRow->TryGetStringField(Field, Retained) && (*Omissions)->TryGetNumberField(Field, Count);
		const int32 OriginalSize = StringCast<UTF8CHAR>(*Large.Arguments, Large.Arguments.Len()).Length();
		const int32 RetainedSize = StringCast<UTF8CHAR>(*Retained, Retained.Len()).Length();
		TestTrue(TEXT("Retained Unicode is an exact prefix with accurate omitted bytes"), bPresent
			&& RetainedSize > 0 && RetainedSize < OriginalSize && Count == OriginalSize - RetainedSize
			&& Large.Arguments.StartsWith(Retained, ESearchCase::CaseSensitive));
	}
	TestTrue(TEXT("Omissions are visible in the sink status"), UnicodeLog->RenderDiskStatus().Contains(TEXT("1 written record(s)")));
	const auto UnicodeMemory = UnicodeLog->GetSnapshot();
	TestTrue(TEXT("Disk clipping does not change admitted in-memory data"),
		UnicodeMemory.Entries.Num() == 1 && UnicodeMemory.Entries[0].Arguments == Large.Arguments
		&& UnicodeMemory.Entries[0].Result == Large.Result && UnicodeMemory.Entries[0].bError);
	return !HasAnyErrors();
}

#endif

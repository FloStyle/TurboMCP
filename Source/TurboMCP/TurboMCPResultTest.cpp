// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// *** THE C++ WRITER AND THE PYTHON READER MUST AGREE, AND THIS IS WHERE THAT IS CHECKED. ***
//
// ARC02's line is written here and parsed by `Tools/tmcp_result.py`. A disagreement between the two turns every
// migrated tool into a protocol failure - which is the OUTCOME ARC02 says a client must never read as success -
// and it is not something either side can detect alone. The Python suite asserts the reader's rules; these
// native cases assert what the writer actually emits, field by field, so a change to either side that breaks
// the contract fails here or there rather than on a user's first call.
//
// The escaping case is the one that matters most: a diagnostic containing a quote, a backslash and a newline is
// the ordinary shape of an engine message, and a writer whose escaping is wrong produces a line that LOOKS
// right and cannot be parsed.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "TurboMCPResult.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPResultLineTest,
	"TurboMCP.Result.LineContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPResultLineTest::RunTest(const FString& Parameters)
{
	// --- the vocabulary, which is what a client classifies on ------------------------------------------
	TestEqual(TEXT("Changed is 'changed'"),
		FTurboMCPResult::OutcomeToken(ETurboMCPResultOutcome::Changed), TEXT("changed"));
	TestEqual(TEXT("OutcomeUnknown is 'outcome_unknown'"),
		FTurboMCPResult::OutcomeToken(ETurboMCPResultOutcome::OutcomeUnknown), TEXT("outcome_unknown"));
	TestEqual(TEXT("not_applicable compile"),
		FTurboMCPResult::CompileToken(ETurboMCPResultCompile::NotApplicable), TEXT("not_applicable"));
	TestEqual(TEXT("failed rollback"),
		FTurboMCPResult::RollbackToken(ETurboMCPResultRollback::Failed), TEXT("failed"));
	TestEqual(TEXT("not_requested save"),
		FTurboMCPResult::SaveToken(ETurboMCPResultSave::NotRequested), TEXT("not_requested"));

	// --- a complete success, checked field by field -----------------------------------------------------
	{
		FTurboMCPResult Result;
		Result.Outcome = ETurboMCPResultOutcome::Changed;
		Result.Code = TEXT("ok");
		Result.SetChanged(true);
		// Deliberately NOT sorted and with a duplicate, so the canonicalisation is exercised rather than assumed.
		Result.Assets = {TEXT("/Game/Zeta"), TEXT("/Game/Alpha"), TEXT("/Game/Alpha")};
		Result.Compile = ETurboMCPResultCompile::Passed;
		Result.OperationId = TEXT("test-op-1");

		const FString Line = Result.RenderLine();
		// `TestTrue` takes two arguments on this engine revision, so a detail goes in the message.
		TestTrue(FString::Printf(TEXT("the line begins with the documented prefix (got: %s)"),
			*Line.Left(40)), Line.StartsWith(TEXT("; tmcp-result-v1: ")));
		TestTrue(TEXT("schema is 1"), Line.Contains(TEXT("\"schema\": 1")));
		TestTrue(TEXT("outcome is changed"), Line.Contains(TEXT("\"outcome\": \"changed\"")));
		TestTrue(TEXT("code is ok"), Line.Contains(TEXT("\"code\": \"ok\"")));
		TestTrue(TEXT("changed is the JSON literal true"), Line.Contains(TEXT("\"changed\": true")));
		TestTrue(TEXT("assets are sorted"), Line.Contains(TEXT("\"assets\": [\"/Game/Alpha\", \"/Game/Zeta\"]")));
		TestTrue(TEXT("a duplicate asset appears once"),
			Line.Find(TEXT("/Game/Alpha")) == Line.Find(TEXT("/Game/Alpha"), ESearchCase::CaseSensitive,
				ESearchDir::FromEnd));
		TestTrue(TEXT("compile is passed"), Line.Contains(TEXT("\"compile\": \"passed\"")));
		TestTrue(TEXT("rollback is not_needed"), Line.Contains(TEXT("\"rollback\": \"not_needed\"")));
		TestTrue(TEXT("save is not_requested"), Line.Contains(TEXT("\"save\": \"not_requested\"")));
		TestTrue(TEXT("complete is true"), Line.Contains(TEXT("\"complete\": true")));
		TestTrue(TEXT("the operation id is carried"), Line.Contains(TEXT("\"operationId\": \"test-op-1\"")));
		TestEqual(TEXT("a completed success maps to exit 0"), Result.SuggestedExitCode(), 0);

		// No optional field appears when it was not set, so a client can tell "absent" from "empty".
		TestFalse(TEXT("no diagnostics array when there are none"), Line.Contains(TEXT("diagnostics")));
		TestFalse(TEXT("no effects array when there are none"), Line.Contains(TEXT("effects")));
		TestFalse(TEXT("no truncated flag when nothing was trimmed"), Line.Contains(TEXT("truncated")));
	}

	// --- the tri-state `changed`, which ARC05 needs for a failed restoration -----------------------------
	{
		FTurboMCPResult Result;
		Result.Outcome = ETurboMCPResultOutcome::Failed;
		Result.Code = TEXT("rollback_failed");
		Result.SetChangedUnknown();
		Result.Rollback = ETurboMCPResultRollback::Failed;

		const FString Line = Result.RenderLine();
		TestTrue(TEXT("changed is the JSON literal null, not false"),
			Line.Contains(TEXT("\"changed\": null")));
		// *** THE ONE MAPPING THAT MATTERS MOST HERE. *** ARC05: "If restoration fails, report `failed` with
		// `rollback=failed` and `changed=null`. Do not report unchanged." A caller told `changed: false` would
		// conclude the asset is as it was, and it may not be.
		TestEqual(TEXT("a failed rollback maps to exit 2, not 1"), Result.SuggestedExitCode(), 2);
	}

	// --- escaping, which is where a correct-looking line stops being parseable ---------------------------
	{
		FTurboMCPResult Result;
		Result.Outcome = ETurboMCPResultOutcome::Refused;
		Result.Code = TEXT("pie_active");
		Result.SetChanged(false);
		// The ordinary shape of an engine message: a quote, a backslash and a newline.
		Result.Diagnostics = {TEXT("the pin \"then\" has no link\\path\nsecond line")};
		Result.OperationId = TEXT("test-op-escape");

		const FString Line = Result.RenderLine();
		TestFalse(TEXT("a raw quote is escaped"), Line.Contains(TEXT("\"then\" has no")));
		TestTrue(TEXT("the escaped quote is present"), Line.Contains(TEXT("\\\"then\\\"")));
		TestTrue(TEXT("a backslash is escaped"), Line.Contains(TEXT("\\\\path")));
		TestTrue(TEXT("a newline is escaped rather than emitted raw"),
			Line.Contains(TEXT("\\nsecond line")));
		// The whole line must remain ONE line: a raw newline inside it would split the authoritative line and
		// the client would read the first half as the body and find no result at all.
		TestEqual(TEXT("the rendered line contains no raw newline"),
			Line.Replace(TEXT("\\n"), TEXT("")).Contains(TEXT("\n")), false);
	}

	// --- the boundary: `AppendTo` puts the line last, exactly once ----------------------------------------
	{
		FTurboMCPResult Result;
		Result.Outcome = ETurboMCPResultOutcome::Changed;
		Result.Code = TEXT("ok");
		Result.SetChanged(true);
		Result.OperationId = TEXT("test-op-append");

		const FString WithoutNewline = Result.AppendTo(TEXT("changed: did the thing"));
		const FString WithNewline = Result.AppendTo(TEXT("changed: did the thing\n"));
		TestEqual(TEXT("a body without a trailing newline gets exactly one"),
			WithoutNewline, FString(TEXT("changed: did the thing\n")) + Result.RenderLine());
		TestEqual(TEXT("a body with a trailing newline does not get a second"),
			WithNewline, FString(TEXT("changed: did the thing\n")) + Result.RenderLine());
		TestTrue(TEXT("the line is LAST, so it is authoritative"),
			WithoutNewline.EndsWith(Result.RenderLine()));
		TestEqual(TEXT("the appended line is absent from the body itself"),
			Result.AppendTo(TEXT("")).Equals(Result.RenderLine()), true);
	}

	// --- ARC04's new-diagnostic rule: it must be able to FIRE, and to stay silent on pre-existing errors ----
	//
	// This is the rule that was narrowed in the product until a suite passed. Proving both directions here is
	// what stops that narrowing from being invisible: the negative control (an unchanged pre-existing set yields
	// nothing) and the positive control (a new identity, and a new OCCURRENCE of an existing identity, each
	// yield exactly one refusal reason) both run against the function the writer actually calls.
	{
		const TArray<FString> Baseline = {
			TEXT("error: Can't connect pins A and Return Value"),
			TEXT("error: Dependency cycle detected"),
			TEXT("warning: unused variable")};

		TestEqual(TEXT("an unchanged baseline yields no new diagnostic"),
			FTurboMCPResult::NewDiagnosticsSince(Baseline, Baseline).Num(), 0);

		// Reordering must not matter: the baseline is a multiset, not a sequence.
		const TArray<FString> Reordered = {
			TEXT("warning: unused variable"),
			TEXT("error: Dependency cycle detected"),
			TEXT("error: Can't connect pins A and Return Value")};
		TestEqual(TEXT("a reordered baseline yields no new diagnostic"),
			FTurboMCPResult::NewDiagnosticsSince(Baseline, Reordered).Num(), 0);

		// POSITIVE CONTROL 1: a new identity, at an unchanged total count. An implementation that compared
		// counts would pass this and be wrong, which is ARC04's "same count is still new".
		TArray<FString> SameCountDifferentMessage = Baseline;
		SameCountDifferentMessage[2] = TEXT("warning: a different warning the edit introduced");
		const TArray<FString> ByIdentity = FTurboMCPResult::NewDiagnosticsSince(Baseline, SameCountDifferentMessage);
		TestEqual(TEXT("a new message at the same count is new"), ByIdentity.Num(), 1);
		TestTrue(TEXT("the new message is the one reported, not the pre-existing ones"),
			ByIdentity.Num() == 1 && ByIdentity[0].Contains(TEXT("a different warning")));

		// POSITIVE CONTROL 2: the same identity one extra time. An identity-only set difference reports nothing.
		TArray<FString> Repeated = Baseline;
		Repeated.Add(TEXT("error: Dependency cycle detected"));
		const TArray<FString> ByMultiplicity = FTurboMCPResult::NewDiagnosticsSince(Baseline, Repeated);
		TestEqual(TEXT("a repeated diagnostic occurrence is new"), ByMultiplicity.Num(), 1);
		TestTrue(TEXT("the repeated identity is the one reported"),
			ByMultiplicity.Num() == 1 && ByMultiplicity[0].Contains(TEXT("Dependency cycle")));

		// A message that DISAPPEARS is not new: an edit that fixes something must not be refused for it.
		TArray<FString> Fewer = Baseline;
		Fewer.RemoveAt(2);
		TestEqual(TEXT("a diagnostic that disappeared is not new"),
			FTurboMCPResult::NewDiagnosticsSince(Baseline, Fewer).Num(), 0);

		// An asset with no baseline at all: every diagnostic is new, which is why ARC04 requires a baseline.
		TestEqual(TEXT("with no baseline every diagnostic is new"),
			FTurboMCPResult::NewDiagnosticsSince({}, Baseline).Num(), Baseline.Num());
	}

	// --- an operation id is unique per invocation, and is not claimed as a deduplication key ---------------
	{
		const FString First = FTurboMCPResult::NewOperationId();
		const FString Second = FTurboMCPResult::NewOperationId();
		TestFalse(TEXT("two operation ids differ"), First == Second);
		TestFalse(TEXT("an operation id is non-empty"), First.IsEmpty());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR



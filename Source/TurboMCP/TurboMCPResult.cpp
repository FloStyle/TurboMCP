// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// TurboMCPResult.cpp - the writer for ARC02's authoritative final line. See TurboMCPResult.h for why it is
// separate from the coordinator.

#include "TurboMCPResult.h"

#include "Misc/Guid.h"

namespace
{
	const TCHAR* ResultPrefix = TEXT("; tmcp-result-v1:");

	/** A JSON string literal, escaped the way the client's parser reads it back. */
	FString Quote(const FString& Text)
	{
		return TEXT("\"") + TurboMCPJsonEscape(Text) + TEXT("\"");
	}

	/** A JSON array of strings, in the order given - callers sort first when order is not meaningful. */
	FString StringArray(const TArray<FString>& Items)
	{
		if (Items.Num() == 0)
		{
			return TEXT("[]");
		}
		TArray<FString> Quoted;
		Quoted.Reserve(Items.Num());
		for (const FString& Item : Items)
		{
			Quoted.Add(Quote(Item));
		}
		return TEXT("[") + FString::Join(Quoted, TEXT(", ")) + TEXT("]");
	}
}

FString TurboMCPJsonEscape(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len() + 8);
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		const TCHAR Character = Text[Index];
		switch (Character)
		{
		case TEXT('"'):  Out += TEXT("\\\""); break;
		case TEXT('\\'): Out += TEXT("\\\\"); break;
		case TEXT('\n'): Out += TEXT("\\n"); break;
		case TEXT('\r'): Out += TEXT("\\r"); break;
		case TEXT('\t'): Out += TEXT("\\t"); break;
		case TEXT('\b'): Out += TEXT("\\b"); break;
		case TEXT('\f'): Out += TEXT("\\f"); break;
		default:
			// Control characters below 0x20 are not legal raw in a JSON string. They are emitted as \u00XX
			// rather than dropped, because dropping a character from a diagnostic is the kind of silent loss
			// this release keeps finding.
			if (Character < 0x20)
			{
				Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Character));
			}
			else
			{
				Out.AppendChar(Character);
			}
			break;
		}
	}
	return Out;
}

FString FTurboMCPResult::NewOperationId()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

FString FTurboMCPResult::OutcomeToken(ETurboMCPResultOutcome Value)
{
	switch (Value)
	{
	case ETurboMCPResultOutcome::Read:           return TEXT("read");
	case ETurboMCPResultOutcome::Planned:        return TEXT("planned");
	case ETurboMCPResultOutcome::Unchanged:      return TEXT("unchanged");
	case ETurboMCPResultOutcome::Changed:        return TEXT("changed");
	case ETurboMCPResultOutcome::Refused:        return TEXT("refused");
	case ETurboMCPResultOutcome::Failed:         return TEXT("failed");
	case ETurboMCPResultOutcome::Partial:        return TEXT("partial");
	case ETurboMCPResultOutcome::OutcomeUnknown: return TEXT("outcome_unknown");
	}
	return TEXT("outcome_unknown");
}

FString FTurboMCPResult::CompileToken(ETurboMCPResultCompile Value)
{
	switch (Value)
	{
	case ETurboMCPResultCompile::Passed:        return TEXT("passed");
	case ETurboMCPResultCompile::Failed:        return TEXT("failed");
	case ETurboMCPResultCompile::NotApplicable: return TEXT("not_applicable");
	case ETurboMCPResultCompile::NotRun:        return TEXT("not_run");
	}
	return TEXT("not_run");
}

FString FTurboMCPResult::RollbackToken(ETurboMCPResultRollback Value)
{
	switch (Value)
	{
	case ETurboMCPResultRollback::Complete:     return TEXT("complete");
	case ETurboMCPResultRollback::Failed:       return TEXT("failed");
	case ETurboMCPResultRollback::NotNeeded:    return TEXT("not_needed");
	case ETurboMCPResultRollback::NotSupported: return TEXT("not_supported");
	}
	return TEXT("not_supported");
}

FString FTurboMCPResult::SaveToken(ETurboMCPResultSave Value)
{
	switch (Value)
	{
	case ETurboMCPResultSave::Saved:          return TEXT("saved");
	case ETurboMCPResultSave::Failed:         return TEXT("failed");
	case ETurboMCPResultSave::Partial:        return TEXT("partial");
	case ETurboMCPResultSave::NotRequested:   return TEXT("not_requested");
	case ETurboMCPResultSave::NotApplicable:  return TEXT("not_applicable");
	}
	return TEXT("not_requested");
}

int32 FTurboMCPResult::SuggestedExitCode() const
{
	// The SAME mapping the client applies (`tmcp_result.py`, and C03 asserts it from the other side). Two
	// implementations of one rule is a risk, and the mitigation is that both are asserted against the same
	// table: 0 completed, 1 refusal/failure/partial, 2 unknown or not-established.
	if (Outcome == ETurboMCPResultOutcome::OutcomeUnknown)
	{
		return 2;
	}
	if (Rollback == ETurboMCPResultRollback::Failed)
	{
		return 2;
	}
	if (Outcome == ETurboMCPResultOutcome::Failed
		|| Outcome == ETurboMCPResultOutcome::Refused
		|| Outcome == ETurboMCPResultOutcome::Partial)
	{
		return 1;
	}
	if (!bComplete)
	{
		return 2;
	}
	if (Rollback == ETurboMCPResultRollback::NotSupported
		&& Outcome == ETurboMCPResultOutcome::Changed)
	{
		return 2;
	}
	return 0;
}

TArray<FString> FTurboMCPResult::NewDiagnosticsSince(const TArray<FString>& Baseline, const TArray<FString>& After)
{
	// ARC04: "Pre-existing diagnostics may remain. Any new error or warning causes rollback. A changed diagnostic
	// with the same count is still new." Identity alone is not enough for that second sentence - the count per
	// identity is what changes - so both are compared here, over the WHOLE set the caller passes.
	TMap<FString, int32> Allowed;
	for (const FString& Diagnostic : Baseline)
	{
		Allowed.FindOrAdd(Diagnostic) += 1;
	}

	TArray<FString> New;
	TMap<FString, int32> Seen;
	for (const FString& Diagnostic : After)
	{
		const int32* AllowedCount = Allowed.Find(Diagnostic);
		const int32 Limit = (AllowedCount != nullptr) ? *AllowedCount : 0;
		const int32 Occurrence = (Seen.FindOrAdd(Diagnostic) += 1);
		if (Occurrence > Limit)
		{
			New.Add(Diagnostic);
		}
	}
	return New;
}

FString FTurboMCPResult::RenderLine() const
{
	// Assets are canonical: sorted and de-duplicated, so two runs that affected the same set produce the same
	// line and a fingerprint over it is stable (ARC06).
	TArray<FString> SortedAssets = Assets;
	SortedAssets.Sort();
	for (int32 Index = SortedAssets.Num() - 1; Index > 0; --Index)
	{
		if (SortedAssets[Index] == SortedAssets[Index - 1])
		{
			SortedAssets.RemoveAt(Index);
		}
	}

	FString Json = TEXT("{");
	Json += FString::Printf(TEXT("\"schema\": %d, "), 1);
	Json += FString::Printf(TEXT("\"outcome\": %s, "), *Quote(OutcomeToken(Outcome)));
	Json += FString::Printf(TEXT("\"code\": %s, "), *Quote(Code));
	if (Changed == 2)
	{
		Json += TEXT("\"changed\": null, ");
	}
	else
	{
		Json += FString::Printf(TEXT("\"changed\": %s, "), Changed == 1 ? TEXT("true") : TEXT("false"));
	}
	Json += FString::Printf(TEXT("\"assets\": %s, "), *StringArray(SortedAssets));
	Json += FString::Printf(TEXT("\"compile\": %s, "), *Quote(CompileToken(Compile)));
	Json += FString::Printf(TEXT("\"rollback\": %s, "), *Quote(RollbackToken(Rollback)));
	Json += FString::Printf(TEXT("\"save\": %s, "), *Quote(SaveToken(Save)));
	Json += FString::Printf(TEXT("\"complete\": %s, "), bComplete ? TEXT("true") : TEXT("false"));
	Json += FString::Printf(TEXT("\"operationId\": %s"), *Quote(OperationId));

	// The optional fields are APPENDED rather than reserved, so a client that does not know them is unaffected
	// and a client that does can read them - which is what ARC02 asks for ("Add a diagnostics array when
	// individual failures need stable codes and target paths. Add `effects` ...").
	if (Diagnostics.Num() > 0)
	{
		Json += FString::Printf(TEXT(", \"diagnostics\": %s"), *StringArray(Diagnostics));
	}
	if (Effects.Num() > 0)
	{
		Json += FString::Printf(TEXT(", \"effects\": %s"), *StringArray(Effects));
	}
	if (bTruncated)
	{
		Json += TEXT(", \"truncated\": true");
		if (Omitted > 0)
		{
			Json += FString::Printf(TEXT(", \"omitted\": %d"), Omitted);
		}
	}
	Json += TEXT("}");

	return FString::Printf(TEXT("%s %s"), ResultPrefix, *Json);
}

FString FTurboMCPResult::AppendTo(const FString& Body) const
{
	const FString Line = RenderLine();
	if (Body.IsEmpty())
	{
		return Line;
	}
	if (Body.EndsWith(TEXT("\n")))
	{
		return Body + Line;
	}
	return Body + TEXT("\n") + Line;
}

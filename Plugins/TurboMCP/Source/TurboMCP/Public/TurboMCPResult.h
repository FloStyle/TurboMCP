// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// *** THE C++ SIDE OF ARC02: THE ONE AUTHORITATIVE RESULT LINE. ***
//
// The Python client reads this format (`Tools/tmcp_result.py`, which holds C03-C08). This file WRITES it, and
// the two must agree: a mismatch turns every migrated tool into a protocol failure, which is exactly the
// outcome ARC02 says must never read as success. The field set, the enumerations and the prefix are therefore
// defined ONCE here and once there, and `TurboMCPToolsets.h`'s tool wrappers never build the line by hand -
// "Only the outer tool boundary appends this line. Inner helpers return typed internal results, not formatted
// public strings."
//
// WHY IT IS SEPARATE FROM THE COORDINATOR. The coordinator decides what HAPPENED; this decides how to SAY it.
// A writer that formats its own line can spell an outcome the client cannot classify, and a writer that
// decides its own outcome can report `unchanged` about an edit that may have applied. Keeping the two apart is
// what makes the vocabulary closed.

#pragma once

#include "CoreMinimal.h"

/** The operation outcome, exactly the values ARC02 defines. */
enum class ETurboMCPResultOutcome : uint8
{
	Read,
	Planned,
	Unchanged,
	Changed,
	Refused,
	Failed,
	Partial,
	OutcomeUnknown,
};

/** The compile outcome, exactly the values ARC02 defines. */
enum class ETurboMCPResultCompile : uint8
{
	Passed,
	Failed,
	NotApplicable,
	NotRun,
};

/** The rollback outcome, exactly the values ARC02 defines. */
enum class ETurboMCPResultRollback : uint8
{
	Complete,
	Failed,
	NotNeeded,
	NotSupported,
};

/** The save outcome, exactly the values ARC02 defines. */
enum class ETurboMCPResultSave : uint8
{
	Saved,
	Failed,
	Partial,
	NotRequested,
	NotApplicable,
};

/**
 * One result, as fields. Built by a writer and rendered once, at the tool boundary.
 *
 * `Changed` is a tri-state on purpose: ARC02 allows `null`, and ARC05 requires it - "If restoration fails,
 * report `failed` with `rollback=failed` and `changed=null`." A bool could not express "the live state is not
 * known", and that missing state is release finding G05.
 */
struct FTurboMCPResult
{
	ETurboMCPResultOutcome Outcome = ETurboMCPResultOutcome::Read;
	FString Code = TEXT("ok");
	/** 0 = false, 1 = true, 2 = null/unknown. */
	int32 Changed = 0;
	TArray<FString> Assets;
	ETurboMCPResultCompile Compile = ETurboMCPResultCompile::NotApplicable;
	ETurboMCPResultRollback Rollback = ETurboMCPResultRollback::NotNeeded;
	ETurboMCPResultSave Save = ETurboMCPResultSave::NotRequested;
	bool bComplete = true;
	FString OperationId;
	/** Stable per-diagnostic codes with their target paths, when individual failures need naming. */
	TArray<FString> Diagnostics;
	/** Known external effects: UI changes, script execution, files. Never used for an unobserved effect. */
	TArray<FString> Effects;
	/** Set when the body was trimmed for a budget, so a reader can tell a complete answer from a short one. */
	bool bTruncated = false;
	int32 Omitted = 0;

	/** A fresh, unique id for one invocation. Not a deduplication guarantee (ARC02 says so explicitly). */
	static FString NewOperationId();

	/** The snake_case token for an outcome, which is also its JSON value. */
	static FString OutcomeToken(ETurboMCPResultOutcome Value);
	static FString CompileToken(ETurboMCPResultCompile Value);
	static FString RollbackToken(ETurboMCPResultRollback Value);
	static FString SaveToken(ETurboMCPResultSave Value);

	/** ARC02's exit-code mapping, so the engine side and the client agree about what an outcome means. */
	int32 SuggestedExitCode() const;

	/** Set `Changed` from a bool, for the ordinary cases. */
	void SetChanged(bool bValue) { Changed = bValue ? 1 : 0; }
	/** Set `Changed` to null, for the case where the live state is not known. */
	void SetChangedUnknown() { Changed = 2; }

	/**
	 * The authoritative final line: `; tmcp-result-v1: {...}`.
	 *
	 * The JSON is written by hand rather than through a serializer, and that is deliberate: a serializer that
	 * reorders or reformats keys cannot break the client, but a serializer whose escaping differs from the
	 * client's parser CAN - so the escaping is explicit and minimal (the characters that must be escaped in a
	 * JSON string), and the client parses with a real JSON parser that handles exactly those.
	 */
	FString RenderLine() const;

	/**
	 * ARC04's new-diagnostic rule, as a pure function: the occurrences in `After` that exceed the baseline
	 * count for the same normalized identity.
	 *
	 * It is pure so a native case can prove it FIRES. A rule that cannot fail has been this project's most
	 * repeated defect, and the production writer calls this exact function rather than a copy of it.
	 * Identity AND multiplicity: one instance before and two after yields one new diagnostic, which an
	 * identity-only set difference would report as nothing.
	 */
	static TArray<FString> NewDiagnosticsSince(const TArray<FString>& Baseline, const TArray<FString>& After);

	/**
	 * The line appended to a body, with a newline when the body does not already end in one.
	 *
	 * Every migrated tool's public answer goes through this, so there is exactly one place that decides how the
	 * boundary looks - and exactly one place to fix if it is wrong.
	 */
	FString AppendTo(const FString& Body) const;
};

/** Escape a string for a JSON string literal, matching what the client's parser expects. */
FString TurboMCPJsonEscape(const FString& Text);

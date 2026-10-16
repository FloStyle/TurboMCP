// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPDataTableRows.h"

#include "Engine/DataTable.h"
#include "Tools/TurboMCPToolsets.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "TurboMCPAssetResolver.h"

namespace
{
	/** Load the table and hand back a refusal that names what was wrong, or the table. */
	UDataTable* LoadTable(const FString& Asset, FString& OutError)
	{
		if (Asset.IsEmpty())
		{
			return nullptr;
		}

		FTurboMCPAssetResolver::FResolved Resolved;
		OutError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}

		UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
		UDataTable* Table = Cast<UDataTable>(Loaded);
		if (Table == nullptr)
		{
			// THE REFUSAL NAMES WHAT IT FOUND INSTEAD, so a caller who pointed at a Blueprint is told that rather
			// than only that it was not a DataTable.
			OutError = FString::Printf(
				TEXT("error: '%s' resolves to %s, which is a %s and not a DataTable."),
				*Asset, *Resolved.ObjectPath,
				Loaded != nullptr ? *Loaded->GetClass()->GetName() : TEXT("nothing"));
			return nullptr;
		}
		return Table;
	}

	/** The preview text: every item the edit would change, listed rather than counted. */
	FString PreviewText(UDataTable* Table, const FTurboMCPDataTableRows::FPlan& Plan)
	{
		FString Out = FString::Printf(TEXT("data table: %s"), *Table->GetPathName());
		Out += FString::Printf(TEXT("\nrow struct: %s"),
			Table->GetRowStruct() != nullptr ? *Table->GetRowStruct()->GetPathName() : TEXT("none"));
		Out += FString::Printf(TEXT("\nrows now: %d"), Table->GetRowMap().Num());

		Out += FString::Printf(TEXT("\nitems the edit would change: %d"), Plan.Changes.Num());
		for (const FTurboMCPDataTableRows::FFieldChange& Change : Plan.Changes)
		{
			// *** THE TERNARIES ARE RESOLVED INTO `FString` VARIABLES RATHER THAN PASSED TO `Printf`, AND THAT IS A
			// MEASURED REQUIREMENT RATHER THAN STYLE. *** Passing `TEXT("...") : TEXT("")` straight into a variadic
			// argument made UE5's compile-time format checker refuse the call:
			//
			//     error C7595: 'UE::Core::Private::FormatStringSan::TCheckedFormatStringPrivate<...>'
			//
			// The sanitizer has to prove each argument's type against its specifier, and an array literal whose type
			// depends on a conditional is not something it can prove. A NAMED `FString` IS. This is the same family
			// as the split `TEXT()` literal that is `error C2816` in this project: **the format checker is part of
			// the compiler here, so a string built the convenient way is a build failure rather than a warning.**
			const FString OldText = Change.OldText.IsEmpty() ? FString(TEXT("(empty)")) : Change.OldText;
			const FString Suffix = Change.bUnchanged
				? FString(TEXT("   [already this value, so nothing would be written]"))
				: FString();

			Out += FString::Printf(TEXT("\n  %s . %s: %s  ->  %s%s"),
				*Change.RowName, *Change.Field, *OldText, *Change.NewText, *Suffix);
		}

		// *** EVERY REMOVAL BY NAME, NEVER A COUNT. *** `D-45`: *"a writer that removes anything lists each removal
		// by key with its class and title, never a count ... a count says how much is gone and a list says what."*
		// For a data table the "class and title" of a row IS its name and its struct, which is what is printed.
		Out += FString::Printf(TEXT("\nrows the edit would REMOVE: %d"), Plan.Removals.Num());
		if (Plan.Removals.Num() == 0)
		{
			Out += TEXT("\n  (none)");
		}
		for (const FString& Name : Plan.Removals)
		{
			Out += FString::Printf(TEXT("\n  - %s"), *Name);
		}

		Out += FString::Printf(TEXT("\nrows the edit would ADD: %d"), Plan.Additions.Num());
		for (const FString& Name : Plan.Additions)
		{
			Out += FString::Printf(TEXT("\n  + %s"), *Name);
		}

		// REFUSALS LAST, SO A PREVIEW WITH ANY IS UNMISTAKABLE, and spelled `error:` so both the preview and the
		// edit surface the same wording - the spec requires that a Preview and its Edit twin REFUSE THE SAME INPUT.
		for (const FString& Refusal : Plan.Refusals)
		{
			Out += FString::Printf(TEXT("\nerror: %s"), *Refusal);
		}

		return Out;
	}

	/** Shared by both entry points, so the preview and the edit cannot disagree about validity. */
	FString Run(const FString& Asset, const TArray<FString>& Lines, bool bWrite, FString& OutReport)
	{
		// *** THE OPERATION TEXT IS PARSED BEFORE THE ASSET IS LOADED, AND THAT ORDER IS THE FIX. ***
		// Loading the table first makes EVERY malformed line come back as "the asset did not resolve", so the
		// parser's own refusals are unreachable: a suite then asserts on syntax it can never see, and TEN
		// CHECKS FAIL FROM ONE ORDERING MISTAKE. **A REFUSAL ABOUT THE REQUEST MUST NOT BE
		// MASKED BY A REFUSAL ABOUT THE TARGET.** It is also the reader's own idiom: `TurboMCPDataTable.cpp`
		// validates its `include` filter *before anything loads, so a typo costs nothing*.
		if (Asset.IsEmpty())
		{
			return TEXT("error: no data table given. Pass a DataTable in any path form, for example DT_Items.");
		}

		TArray<FTurboMCPDataTableRows::FRowOp> Ops;
		FString Error = FTurboMCPDataTableRows::Parse(Lines, Ops);
		if (!Error.IsEmpty())
		{
			return Error;
		}

		UDataTable* Table = LoadTable(Asset, Error);
		if (Table == nullptr)
		{
			return Error;
		}

		FTurboMCPDataTableRows::FPlan Plan;
		Error = FTurboMCPDataTableRows::Plan(Table, Ops, Plan);
		if (!Error.IsEmpty())
		{
			return Error;
		}

		// THE SAME REFUSALS GATE BOTH PATHS, WHICH IS THE SPEC'S RULE FOR EVERY `P/E` PAIR: *"put every check in
		// Plan, not in Apply, so the Preview and its Edit twin refuse the SAME input."*
		if (!Plan.bAcceptable())
		{
			return PreviewText(Table, Plan);
		}

		if (!bWrite)
		{
			return PreviewText(Table, Plan);
		}

		Error = FTurboMCPDataTableRows::Apply(Table, Plan, OutReport);
		if (!Error.IsEmpty())
		{
			// `Apply` always reads the written rows back, INCLUDING after a partial write. Do not hide that
			// measurement behind the refusal: it names every requested field that is missing or differs.
			return OutReport.IsEmpty() ? Error : FString::Printf(TEXT("%s\n\n%s"), *Error, *OutReport);
		}

		return FString::Printf(TEXT("%s\n\n%s"), *PreviewText(Table, Plan), *OutReport);
	}
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPDataTableRows(
	const FString& Asset,
	const TArray<FString>& Rows)
{
	FString Report;
	return Run(Asset, Rows, /*bWrite=*/false, Report);
}

FString UTurboMCPEditToolset::EditTurboMCPDataTableRows(
	const FString& Asset,
	const TArray<FString>& Rows)
{
	FString Report;
	return Run(Asset, Rows, /*bWrite=*/true, Report);
}

// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"

class UDataTable;

/**
 * ROW OPERATIONS ON A DATA TABLE, AS TEXT, CHECKED BEFORE ANYTHING IS WRITTEN.
 *
 * WHY THIS IS A SEPARATE UNIT FROM `TurboMCPDataTable`. That file is the READER and it stays the reader. This is
 * the writer, and the split is the same one the graph side settled on: ONE traversal, owned by the reader, that the
 * writer calls rather than reimplements.
 *
 * *** HOW THE INDEX SHIFT IS HANDLED, WHICH IS `D-45`'S SUBJECT. ***
 *
 * `D-45`: *"every writer states what it can destroy, and deletes by identity ... it re-resolves every node by key at
 * apply time rather than trusting a plan from an earlier call."* A DataTable makes that concrete and sharp, because
 * THE ROWS HAVE NO STABLE IDENTITY OTHER THAN THEIR NAME: the storage is a `TMap<FName, uint8*>`, so removing one
 * row cannot invalidate another row's identity - BUT ANYTHING THAT ADDRESSED ROWS BY POSITION WOULD BE WRONG THE
 * MOMENT ONE IS REMOVED.
 *
 * SO NOTHING HERE USES AN INDEX. **EVERY OPERATION NAMES ITS ROW BY NAME**, the plan stores names rather than
 * pointers or offsets, and `Apply` RE-RESOLVES EACH NAME AGAINST THE TABLE AS IT FINDS IT AT THAT MOMENT. A row
 * added by this call is also a name, so an operation after it in the same call can change the row it just added.
 *
 * *** AND THE VALUES GO THROUGH THE ENGINE'S OWN TEXT IMPORT, WHICH IS WHAT THE SPEC ASKS FOR AND WHY. ***
 *
 * The spec: *"Values are written through the row struct's own text import, so the accepted spellings are the
 * struct's export text and not a format invented here."* `UScriptStruct::ImportText` is the engine's own entry
 * point - the data table editor's own paste path calls exactly it (`DataTableEditor.cpp:972`) - so a field spelling
 * this plugin accepts is a field spelling the engine accepts, with no second dialect to drift.
 *
 * `ImportText` REPLACES THE WHOLE STRUCT, so changing one field without destroying the others CANNOT be done by
 * importing the caller's fragment directly. The mechanism is a ROUND TRIP: EXPORT the row as the engine prints it,
 * overwrite the named fields in that text, and IMPORT the result. **FIELDS THE CALLER DID NOT MENTION ARE PRESERVED
 * BY CONSTRUCTION RATHER THAN BY A COPY THAT COULD BE FORGOTTEN.**
 */
namespace FTurboMCPDataTableRows
{
	/** What one operation asks for. */
	enum class ERowOp : uint8
	{
		/** Create the row, or set the named fields on the row that already has that name. */
		Set,

		/** Remove the row. THE ONLY OP THAT DESTROYS. */
		Remove,
	};

	/** One requested field change, as the caller spelled it. */
	struct FFieldEdit
	{
		FString Name;
		FString Text;
	};

	/** One row operation, re-resolved by NAME at apply time and never by index. */
	struct FRowOp
	{
		ERowOp Op = ERowOp::Set;
		FString RowName;
		TArray<FFieldEdit> Fields;

		/** The caller's spelling, kept so a refusal can quote the line that caused it. */
		FString Source;
	};

	/** One field a Set would change, with both values so the preview can print them. */
	struct FFieldChange
	{
		FString RowName;
		FString Field;
		FString OldText;
		FString NewText;

		/** True when the field's value is already the requested one, so nothing would be written. */
		bool bUnchanged = false;
	};

	/** What a plan found, whether or not it is acceptable. */
	struct FPlan
	{
		/** In the order given, so the preview reads like the request. */
		TArray<FRowOp> Ops;

		/** Every field a Set would change, INCLUDING the ones already at the requested value, flagged. */
		TArray<FFieldChange> Changes;

		/** Every row a Remove would delete, BY NAME. NEVER A COUNT - `D-45` says a count says how much is gone. */
		TArray<FString> Removals;

		/** Names this call would ADD, so the report can say what is new rather than only what changed. */
		TArray<FString> Additions;

		/** Everything that must be reported. A plan with any refusal is NOT acceptable. */
		TArray<FString> Refusals;

		bool bAcceptable() const { return Refusals.Num() == 0; }
	};

	/** Parse the caller's operation text into operations, or return an error naming the accepted spelling. */
	FString Parse(const TArray<FString>& Lines, TArray<FRowOp>& Out);

	/** Work out what would change, WITHOUT writing. Everything a writer must refuse is refused here. */
	FString Plan(UDataTable* Table, const TArray<FRowOp>& Ops, FPlan& Out);

	/** Carry the plan out in one transaction, then RE-READ the rows back and compare. */
	FString Apply(UDataTable* Table, const FPlan& Plan, FString& OutReport);
}
